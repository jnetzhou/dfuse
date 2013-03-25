/**
 * @file df_host.c
 * @author carrier.nicolas0@gmail.com
 * @date 08 mar. 2013
 *
 * dfuse, or droid fuse : file system in userspace over the adb protocol
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fdevent.h>
#include <adb.h>
#include <adb_client.h>
#include <file_sync_service.h>

#define FUSE_USE_VERSION 26

#include <fuse.h>

#include "adb_bridge.h"
#include "df_protocol.h"
#include "df_data_types.h"

/* #define DF_HOST_PORT 6665 */
#define DF_HOST_PORT 6666

/**
 * @var sock
 * @brief socket opened on the device, via which file system request will pass
 */
static int sock = -1;

static int dbg;

#define FREE(p) do { \
	if (p) \
		free(p); \
	(p) = NULL; \
} while (0) \

static void char_array_free(char **array)
{
	FREE(*array);
}

static int df_getattr(const char *path, struct stat *stbuf)
{
	int ret;
	char __attribute__ ((cleanup(char_array_free)))*payload = NULL;
	char __attribute__ ((cleanup(char_array_free)))*answer_payload = NULL;
	size_t size = 0;
	struct df_packet_header header = {
		.op_code = DF_OP_GETATTR,
		.is_host_packet = 1,
		.error = 0,
	};
	size_t offset = 0;

	ret = df_build_payload(&payload, &size,
			DF_DATA_BUFFER, strlen(path) + 1, path,
			DF_DATA_END);
	if (0 > ret)
		return ret;
	header.payload_size = size;

	ret = df_write_message(sock, &header, payload);
	if (0 > ret)
		return ret;

	ret = df_read_message(sock, &header, &answer_payload);
	if (0 > ret)
		return ret;
	if (0 != header.error)
		return -header.error;

	ret = df_parse_payload(answer_payload, &offset, header.payload_size,
			DF_DATA_STAT, stbuf,
			DF_DATA_END);

	if (dbg)
		dump_stat(stbuf);

	return ret;
}

static int df_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	int ret;
	char __attribute__ ((cleanup(char_array_free)))*payload = NULL;
	size_t size = 0;
	struct df_packet_header header = {
		.op_code = DF_OP_READDIR,
		.is_host_packet = 1,
		.error = 0,
	};
	char __attribute__ ((cleanup(char_array_free)))*entry_path = NULL;
	size_t payload_offset = 0;
	int64_t ioffset = offset;
	int64_t len;
	struct stat st;

	ret = df_build_payload(&payload, &size,
			DF_DATA_BUFFER, strlen(path) + 1, path,
			DF_DATA_INT, ioffset,
			DF_DATA_FUSE_FILE_INFO, fi,
			DF_DATA_END);
	if (0 > ret)
		return ret;
	header.payload_size = size;

	ret = df_write_message(sock, &header, payload);
	if (0 > ret)
		return ret;

	FREE(payload);
	ret = df_read_message(sock, &header, &payload);
	if (0 > ret)
		return ret;
	if (0 != header.error) {
		fprintf(stderr, " *** %d %s, %s\n", header.error, payload,
				strerror(header.error));
		return -header.error;
	}

	ret = df_parse_payload(payload, &payload_offset, header.payload_size,
			DF_DATA_FUSE_FILE_INFO, fi,
			DF_DATA_BLOCK_END);
	if (0 > ret)
		return ret;

	/*
	 * all has been parsed when consumed data (offset) equals size minus 8
	 * because the last DF_DATA_END is never popped
	 */
	while (payload_offset + 8 < header.payload_size) {
		ret = df_parse_payload(payload, &payload_offset,
				header.payload_size,

				DF_DATA_BUFFER, &len, &entry_path,
				DF_DATA_STAT, &st,
				DF_DATA_BLOCK_END);
		if (0 > ret)
			return ret;
		if (filler(buf, entry_path, &st, 0))
			break;
		FREE(entry_path);
	}

	ret = df_parse_payload(payload, &payload_offset, header.payload_size,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	return ret;
}

static int df_readlink(const char *path, char *buf, size_t size)
{
	int ret;
	char __attribute__((cleanup(char_array_free))) *payload = NULL;
	char __attribute__((cleanup(char_array_free))) *answer_payload = NULL;
	size_t pl_size = 0;
	struct df_packet_header header = {
		.op_code = DF_OP_READLINK,
		.is_host_packet = 1,
		.error = 0,
	};
	size_t offset = 0;
	int64_t target_len = size;
	char __attribute__((cleanup(char_array_free))) *tmp_buf = NULL;

	ret = df_build_payload(&payload, &pl_size,
			DF_DATA_BUFFER, strlen(path) + 1, path,
			DF_DATA_INT, target_len,
			DF_DATA_END);
	if (0 > ret)
		return ret;
	header.payload_size = pl_size;

	ret = df_write_message(sock, &header, payload);
	if (0 > ret)
		return ret;

	ret = df_read_message(sock, &header, &answer_payload);
	if (0 > ret)
		return ret;
	if (0 != header.error)
		return -header.error;

	ret = df_parse_payload(answer_payload, &offset, header.payload_size,
			DF_DATA_BUFFER, &target_len, &tmp_buf,
			DF_DATA_END);

	strncpy(buf, tmp_buf, size);
	buf[size] = '\0';

	if (dbg)
		fprintf(stderr, "[%s] target of %s is %s\n",
				df_op_code_to_str(header.op_code), path, buf);

	return ret;
}

static struct fuse_operations df_oper = {
	.getattr	= df_getattr,
	.readdir	= df_readdir,
	.readlink	= df_readlink,
};

/**
 * Says wheter we can talk to any device or not
 * @return non-zero if a device is present and we can talk through a running
 * adb host server
 */
/*static*/ int can_talk_to_a_device()
{
	int hellofd;

	/* check a server is a live and we can talk to it */
	hellofd = _adb_connect("shell:ls");
	/*
	 * TODO on success, keeptrack of the device serial and use it until the
	 * end to avoid TOCTTOU
	 */
	if (0 > hellofd)
		return 0;

	close(hellofd);

	return 1;
}

/* ./misc/adb forward tcp:6665 tcp:6666 */
int main(int argc, char *argv[])
{
	int ret;
	uint32_t device_version = 0;
	struct sockaddr_in addr;
	struct sockaddr_in cli_addr;
	socklen_t addr_len = sizeof(addr);
	int srv_sock;
	int optval = 1;

	/* TODO set up forwarding to device
	if (!can_talk_to_a_device()) {
		fprintf(stderr, "can't talk to a device : '%s'\n", adb_error());
		return EXIT_FAILURE;
	}
	*/

	srv_sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == srv_sock) {
		perror("socket");
		return EXIT_FAILURE;
	}

	ret = setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &optval,
			sizeof(optval));
	if (0 > ret) {
		perror("setsockopt");
		return EXIT_FAILURE;
	}

	memset(&addr, 0, addr_len);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(DF_HOST_PORT);

	ret = bind(srv_sock, (struct sockaddr *)&addr, addr_len);
	if (-1 == ret) {
		perror("bind");
		return EXIT_FAILURE;
	}

	ret = listen(srv_sock, 1);
	if (-1 == ret) {
		perror("listen");
		return EXIT_FAILURE;
	}

	/* TODO launch the client */

	printf("Waiting for client\n");

	memset(&cli_addr, 0, addr_len);
	sock = accept4(srv_sock, (struct sockaddr *)&cli_addr, &addr_len,
			SOCK_CLOEXEC);
	if (-1 == sock) {
		perror("accept");
		return EXIT_FAILURE;
	}

	printf("Client %d is connected\n", sock);

	ret = df_send_handshake(sock, DF_PROTOCOL_VERSION);
	if (0 > ret)
		return EXIT_FAILURE;

	ret = df_read_handshake(sock, &device_version);
	if (0 > ret)
		return EXIT_FAILURE;

	if (device_version != DF_PROTOCOL_VERSION) {
		printf("protocol version mismatch, host : %u, device : %u\n",
				DF_PROTOCOL_VERSION, device_version);
		return EXIT_FAILURE;
	}

	printf("File system initialization\n");

	ret = fuse_main(argc, argv, &df_oper, NULL);

	close(sock);
	close(srv_sock);

	return ret;
}
