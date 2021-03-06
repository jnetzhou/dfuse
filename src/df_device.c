/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall fusexmp.c `pkg-config fuse --cflags --libs` -o fusexmp
*/
#define USE_UNIX_SOCKET

#include <sys/types.h>
#include <sys/socket.h>
#ifdef USE_UNIX_SOCKET
#include <sys/un.h>
struct sockaddr_un __sizecheck;
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX sizeof(__sizecheck.sun_path)
#endif
#else
#include <netinet/ip.h>
#endif

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "df_protocol.h"
#include "df_data_types.h"

#define DF_DEVICE_PORT 6666

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define FREE(p) do { \
	if (p) {\
		free(p); \
		(p) = NULL; \
	} \
} while (0)

static void char_array_free(char **array)
{
	FREE(*array);
}

static int errno_reply(enum df_op op_code, int err,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	*ans_pld = strdup(strerror(err));
	if (NULL == *ans_pld)
		return -errno;

	return fill_header(ans_hdr, strlen(*ans_pld) + 1, op_code, err);
}

static int action_getattr(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_GETATTR;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;

	struct stat out_stat;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);
	in_path[in_path_len - 1] = '\0';

	/* perform the syscall */
	ret = lstat(in_path, &out_stat);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_STAT, &out_stat,
			DF_DATA_END);
}

static int action_access(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_ACCESS;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	int64_t in_mask;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_INT, &in_mask,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	ret = access(in_path, in_mask);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_END);
}

static int action_mknod(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_MKNOD;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	int64_t in_mode;
	int64_t in_rdev;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_INT, &in_mode,
			DF_DATA_INT, &in_rdev,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	ret = mknod(in_path, in_mode, in_rdev);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_END);
}

static int action_open(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_OPEN;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	struct fuse_file_info in_fi;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	ret = open(in_path, in_fi.flags);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);
	in_fi.fh = ret;

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
}

static int action_read(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_READ;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	int64_t in_size;
	int64_t in_offset;
	struct fuse_file_info in_fi;

	char __attribute__((cleanup(char_array_free))) *out_buf;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_INT, &in_size,
			DF_DATA_INT, &in_offset,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	out_buf = malloc(in_size);
	if (NULL == out_buf)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);
	ret = pread(in_fi.fh, out_buf, in_size, in_offset);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_BUFFER, ret, out_buf,
			DF_DATA_END);
}

static int action_readlink(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_GETATTR;

	char __attribute__((cleanup(char_array_free))) *in_path = NULL;
	int64_t in_path_len;
	int64_t in_size;

	char __attribute__((cleanup(char_array_free))) *out_buf = NULL;
	size_t out_buf_len = 0;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_INT, &in_size,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);
	in_path[in_path_len - 1] = '\0';

	/* perform the syscall */
	out_buf = malloc(in_size);
	ret = readlink(in_path, out_buf, in_size - 1);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);
	out_buf_len = MIN(ret + 1, in_size);
	out_buf[out_buf_len - 1] = '\0';

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_BUFFER, out_buf_len, out_buf,
			DF_DATA_END);
}

static int action_readdir(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	DIR *dp;
	struct dirent *de;
	size_t offset = 0;
	size_t size = 0;
	enum df_op op_code = DF_OP_READDIR;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	int64_t in_offset;
	struct fuse_file_info in_fi;

	struct stat in_stat;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_INT, &in_offset,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);
	in_path[in_path_len - 1] = '\0';

	dp = opendir(in_path);
	if (dp == NULL)
		return errno_reply(op_code, -errno, ans_hdr, ans_pld);

	/* build the answer */
	ret = df_build_payload(ans_pld, &size,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_BLOCK_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	while ((de = readdir(dp)) != NULL) {
		memset(&in_stat, 0, sizeof(in_stat));
		in_stat.st_ino = de->d_ino;
		in_stat.st_mode = de->d_type << 12;
		ret = df_build_payload(ans_pld, &size,
				DF_DATA_BUFFER, strlen(de->d_name) + 1,
				de->d_name,
				DF_DATA_STAT, &in_stat,
				DF_DATA_BLOCK_END);
		if (0 > ret)
			return errno_reply(op_code, -ret, ans_hdr, ans_pld);
	}
	closedir(dp);

	/* terminate the payload */
	ret = df_build_payload(ans_pld, &size,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	return fill_header(ans_hdr, size, op_code, 0);
}

static int action_release(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_RELEASE;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	struct fuse_file_info in_fi;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	ret = close(in_fi.fh);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
}

static int action_unlink(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_UNLINK;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	ret = unlink(in_path);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_END);
}

static int action_write(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	int ret;
	size_t offset = 0;
	enum df_op op_code = DF_OP_WRITE;

	int64_t in_path_len;
	char __attribute__ ((cleanup(char_array_free))) *in_path = NULL;
	int64_t in_size;
	char __attribute__ ((cleanup(char_array_free))) *in_buf = NULL;
	int64_t in_offset;
	struct fuse_file_info in_fi;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &in_path_len, &in_path,
			DF_DATA_BUFFER, &in_size, &in_buf,
			DF_DATA_INT, &in_offset,
			DF_DATA_FUSE_FILE_INFO, &in_fi,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, ans_hdr, ans_pld);

	/* perform the syscall */
	ret = pwrite(in_fi.fh, in_buf, in_size, in_offset);
	if (ret == -1)
		return errno_reply(op_code, errno, ans_hdr, ans_pld);

	return df_request_build(ans_hdr, ans_pld, op_code,
			DF_DATA_INT, (int64_t)ret,
			DF_DATA_END);
}

int action_enosys(struct df_packet_header *header,
		char __attribute__((unused)) *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	return errno_reply(header->op_code, ENOSYS, ans_hdr, ans_pld);
}

typedef int (*action_t)(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld);

static action_t dispatch_table[] = {
	[DF_OP_INVALID] = action_enosys,

	[DF_OP_GETATTR] = action_getattr,
	[DF_OP_READDIR] = action_readdir,
	[DF_OP_READLINK] = action_readlink,
	[DF_OP_OPEN] = action_open,
	[DF_OP_READ] = action_read,
	[DF_OP_RELEASE] = action_release,
	[DF_OP_MKNOD] = action_mknod,
	[DF_OP_WRITE] = action_write,
	[DF_OP_MKDIR] = action_enosys,
	[DF_OP_UNLINK] = action_unlink,
	[DF_OP_RMDIR] = action_enosys,

	[DF_OP_TRUNCATE] = action_enosys,
	[DF_OP_RENAME] = action_enosys,
	[DF_OP_CHMOD] = action_enosys,
	[DF_OP_CHOWN] = action_enosys,
	[DF_OP_ACCESS] = action_access,
	[DF_OP_SYMLINK] = action_enosys,
	[DF_OP_LINK] = action_enosys,

	[DF_OP_UTIMENS] = action_enosys,
	[DF_OP_STATFS] = action_enosys,
	[DF_OP_FSYNC] = action_enosys,
	[DF_OP_FALLOCATE] = action_enosys,
	[DF_OP_SETXATTR] = action_enosys,
	[DF_OP_GETXATTR] = action_enosys,
	[DF_OP_LISTXATTR] = action_enosys,
	[DF_OP_REMOVEXATTR] = action_enosys,

	[DF_OP_QUIT] = action_enosys,
};

static int dispatch(struct df_packet_header *header, char *payload,
		struct df_packet_header *ans_hdr, char **ans_pld)
{
	action_t action;
	enum df_op op = header->op_code;

	if (op > DF_OP_QUIT || (int)op < (int)DF_OP_INVALID)
		return -ENOSYS;

	action = dispatch_table[op];
	if (NULL == action) {
		fprintf(stderr, "NULL action %d\n", op);
		return -EINVAL;
	}

	return action(header, payload, ans_hdr, ans_pld);
}

static int event_loop(int sock)
{
	int ret;
	struct df_packet_header header;
	char __attribute__ ((cleanup(char_array_free))) *payload = NULL;
	struct df_packet_header ans_hdr;
	char __attribute__ ((cleanup(char_array_free))) *ans_pld = NULL;

	do {
		memset(&header, 0, sizeof(header));
		ret = df_read_message(sock, &header, &payload);
		if (0 > ret)
			return ret;

		memset(&ans_hdr, 0, sizeof(ans_hdr));
		ret = dispatch(&header, payload, &ans_hdr, &ans_pld);
		FREE(payload);
		if (0 > ret)
			return ret;

		ret = df_write_message(sock, &ans_hdr, ans_pld);
		FREE(ans_pld);
		if (0 > ret)
			return ret;
	} while (header.op_code != DF_OP_QUIT);

	return 0;
}

static int usage(int status, const char *path)
{
	printf("usage : %s [local]\n", path);
	printf("\tlocal mode is used for testing with both device and host ");
	printf("parts running on the same PC machine\n");

	return status;
}

int main(int argc, char *argv[])
{
	int ret;
	uint32_t device_version = 0;
#ifdef USE_UNIX_SOCKET
	struct sockaddr_un addr;
	struct sockaddr_un cli_addr;
	int domain = AF_UNIX;
#else
	struct sockaddr_in addr;
	struct sockaddr_in cli_addr;
	int domain = AF_INET;
#endif
	socklen_t addr_len = sizeof(addr);
	int srv_sock = -1;
	int sock = -1;
	int optval = 1;
	int local = 0;

	if (argc != 1) {
		if (strcmp("local", argv[1]) == 0)
			local = 1;
		else
			return usage(EXIT_FAILURE, basename(argv[0]));
	}

	/* TODO set up forwarding to device
	if (!can_talk_to_a_device()) {
		fprintf(stderr, "can't talk to a device : '%s'\n", adb_error());
		return EXIT_FAILURE;
	}
	*/

	printf("dfuse device daemon %s(build "__DATE__" - "__TIME__")\n",
			local ? "in local mode " : "");

	srv_sock = socket(domain, SOCK_STREAM | SOCK_CLOEXEC, 0);
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
#ifdef USE_UNIX_SOCKET
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path + 1, UNIX_PATH_MAX - 1, "dfuse.socket");
	*addr.sun_path = '\0';
#else
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(DF_DEVICE_PORT);
#endif

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

	printf("Waiting for host\n");

	memset(&cli_addr, 0, addr_len);
#ifdef HAVE_ACCEPT4
	sock = accept4(srv_sock, (struct sockaddr *)&cli_addr, &addr_len,
			SOCK_CLOEXEC);
#else
	sock = accept(srv_sock, (struct sockaddr *)&cli_addr, &addr_len);
	/* TODO add setting of the cloexec flag separately */
#endif
	if (-1 == sock) {
		perror("accept");
		return EXIT_FAILURE;
	}

	printf("host %d is connected\n", sock);

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

	printf("Server listening for requests\n");

	ret = event_loop(sock);

	close(sock);
	close(srv_sock);

	return ret;
}
