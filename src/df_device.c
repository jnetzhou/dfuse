/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall fusexmp.c `pkg-config fuse --cflags --libs` -o fusexmp
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
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

static int xmp_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
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

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to)
{
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	int res;

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int res;

	res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	close(res);
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;
	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
	.access		= xmp_access,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

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

	[DF_OP_READDIR] = action_readdir,
	[DF_OP_GETATTR] = action_getattr,
	[DF_OP_READLINK] = action_readlink,
	[DF_OP_MKDIR] = action_enosys,
	[DF_OP_OPEN] = action_open,
	[DF_OP_RELEASE] = action_enosys,
	[DF_OP_READ] = action_enosys,
	[DF_OP_WRITE] = action_enosys,
	[DF_OP_UNLINK] = action_enosys,
	[DF_OP_RMDIR] = action_enosys,

	[DF_OP_TRUNCATE] = action_enosys,
	[DF_OP_RENAME] = action_enosys,
	[DF_OP_CHMOD] = action_enosys,
	[DF_OP_CHOWN] = action_enosys,
	[DF_OP_ACCESS] = action_enosys,
	[DF_OP_SYMLINK] = action_enosys,
	[DF_OP_LINK] = action_enosys,

	[DF_OP_MKNOD] = action_enosys,
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

int main(void)
{
	int sock = -1;
	int ret;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	uint32_t host_version;
	sigset_t sig;

	sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (0 > sock) {
		perror("socket");
		return EXIT_FAILURE;
	}

	memset(&addr, 0, addr_len);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(DF_DEVICE_PORT);

	printf("Attempt to connect to host\n");

	ret = connect(sock, (struct sockaddr *)&addr, addr_len);
	if (0 > ret) {
		perror("socket");
		return EXIT_FAILURE;
	}

	printf("Connected to host\n");

	sigemptyset(&sig);
	sigaddset(&sig, SIGPIPE);
	sigprocmask(SIG_BLOCK, &sig, NULL);

	ret = df_read_handshake(sock, &host_version);
	if (0 > ret)
		return EXIT_FAILURE;

	ret = df_send_handshake(sock, DF_PROTOCOL_VERSION);
	if (0 > ret)
		return EXIT_FAILURE;

	if (host_version != DF_PROTOCOL_VERSION) {
		printf("protocol version mismatch, host : %u, device : %u\n",
				host_version, DF_PROTOCOL_VERSION);
		return EXIT_FAILURE;
	}

	ret = event_loop(sock);

	if (-1 != sock)
		close(sock);

	return ret;

	/* TODO gcc warning */
	printf("%p\n", &xmp_oper);
}
