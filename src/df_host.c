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

#define FREE(p) do { \
	if (p) \
		free(p); \
	(p) = NULL; \
} while (0) \

static void char_array_free(char **array)
{
	FREE(*array);
}

static int df_getattr(const char *in_path, struct stat *out_stbuf)
{
	int ret;
	enum df_op op_code = DF_OP_GETATTR;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	return df_remote_answer(sock, op_code,
			DF_DATA_STAT, out_stbuf,
			DF_DATA_END);
}

static int df_mknod(const char *in_path, mode_t in_mode, dev_t in_rdev)
{
	int ret;
	enum df_op op_code = DF_OP_MKNOD;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_INT, (int64_t)in_mode,
			DF_DATA_INT, (int64_t)in_rdev,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	return df_remote_answer(sock, op_code,
			DF_DATA_END);
}

static int df_open(const char *in_path, struct fuse_file_info *in_fi)
{
	int ret;
	enum df_op op_code = DF_OP_OPEN;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	return df_remote_answer(sock, op_code,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
}

static int df_read(const char *in_path, char *out_buf, size_t in_size,
		off_t in_offset, struct fuse_file_info *in_fi)
{
	int ret;
	enum df_op op_code = DF_OP_READ;

	int64_t res;
	char __attribute__((cleanup(char_array_free))) *tmp_buf = NULL;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_INT, (int64_t)in_size,
			DF_DATA_INT, (int64_t)in_offset,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	ret = df_remote_answer(sock, op_code,
				DF_DATA_BUFFER, &res, &tmp_buf,
				DF_DATA_END);
	memcpy(out_buf, tmp_buf, res);
	if (0 > ret)
		return ret;

	return res;
}

static int df_readdir(const char *in_path, void *in_buf, fuse_fill_dir_t filler,
		       off_t in_offset, struct fuse_file_info *in_fi)
{
	int ret;
	char __attribute__((cleanup(char_array_free))) *payload = NULL;
	struct df_packet_header header;
	char __attribute__((cleanup(char_array_free))) *entry_path = NULL;
	size_t payload_offset = 0;
	int64_t len;
	struct stat st;

	ret = df_remote_call(sock, DF_OP_READDIR,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_INT, (int64_t)in_offset,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	ret = df_read_message(sock, &header, &payload);
	if (0 > ret)
		return ret;
	if (0 != header.error)
		return -header.error;

	ret = df_parse_payload(payload, &payload_offset, header.payload_size,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_BLOCK_END);
	if (0 > ret)
		return ret;

	/*
	 * all has been parsed when consumed data (payload_offset) equals size
	 * minus 8 because the last DF_DATA_END is popped out of the loop
	 */
	while (payload_offset + 8 < header.payload_size) {
		ret = df_parse_payload(payload, &payload_offset,
				header.payload_size,

				DF_DATA_BUFFER, &len, &entry_path,
				DF_DATA_STAT, &st,
				DF_DATA_BLOCK_END);
		if (0 > ret)
			return ret;
		if (filler(in_buf, entry_path, &st, 0))
			break;
		FREE(entry_path);
	}

	return df_parse_payload(payload, &payload_offset, header.payload_size,
			DF_DATA_END);
}

static int df_readlink(const char *in_path, char *out_buf, size_t in_size)
{
	int ret;
	int64_t target_len = in_size;
	char __attribute__((cleanup(char_array_free))) *tmp_buf = NULL;
	enum df_op op_code = DF_OP_READLINK;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_INT, target_len,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	ret = df_remote_answer(sock, op_code,
			DF_DATA_BUFFER, &target_len, &tmp_buf,
			DF_DATA_END);

	/* TODO use a strlcpy implementation */
	strncpy(out_buf, tmp_buf, in_size);
	out_buf[in_size] = '\0';

	return ret;
}

static int df_release(const char *in_path, struct fuse_file_info *in_fi)
{
	int ret;
	enum df_op op_code = DF_OP_RELEASE;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	return df_remote_answer(sock, op_code,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
}

static int df_write(const char *in_path, const char *in_buf, size_t in_size,
		off_t in_offset, struct fuse_file_info *in_fi)
{
	int ret;
	enum df_op op_code = DF_OP_WRITE;

	int64_t out_res;

	ret = df_remote_call(sock, op_code,
			DF_DATA_BUFFER, strlen(in_path) + 1, in_path,
			DF_DATA_BUFFER, in_size, in_buf,
			DF_DATA_INT, (int64_t)in_offset,
			DF_DATA_FUSE_FILE_INFO, in_fi,
			DF_DATA_END);
	if (0 > ret)
		return ret;

	ret = df_remote_answer(sock, op_code,
				DF_DATA_INT, &out_res,
				DF_DATA_END);
	if (0 > ret)
		return ret;

	return out_res;
}

static struct fuse_operations df_oper = {
	.getattr	= df_getattr,
	.open		= df_open,
	.mknod		= df_mknod,
	.read		= df_read,
	.readdir	= df_readdir,
	.readlink	= df_readlink,
	.release	= df_release,
	.write		= df_write,
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
