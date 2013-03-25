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

static int dbg;

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

static int fill_header(struct df_packet_header *header, size_t size,
		enum df_op op_code, int error)
{
	if (NULL == header)
		return -EINVAL;

	memset(header, 0, sizeof(*header));
	header->payload_size = size;
	header->op_code = op_code;
	header->error = error;

	return 0;
}
static int errno_reply(enum df_op op_code, int err,
		struct df_packet_header *answer_header, char **answer_payload)
{
	*answer_payload = strdup(strerror(err));
	if (NULL == *answer_payload)
		return -errno;

	return fill_header(answer_header, strlen(*answer_payload) + 1, op_code,
			err);
}

static int action_getattr(struct df_packet_header *header, char *payload,
		struct df_packet_header *answer_header, char **answer_payload)
{
	int ret;
	size_t offset = 0;
	char __attribute__ ((cleanup(char_array_free))) *path = NULL;
	struct stat st;
	size_t size = 0;
	int64_t path_len;
	int ipath_len;
	enum df_op op_code = DF_OP_GETATTR;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &path_len, &path,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);
	path[path_len - 1] = '\0';

	if (dbg) {
		fprintf(stderr, "offset   = %u\n", offset);
		ipath_len = path_len;
		fprintf(stderr, "path_len = %d\n", ipath_len);
		fprintf(stderr, "path     = %.*s\n", ipath_len, path);
		fprintf(stderr, "action %s %s\n", df_op_code_to_str(op_code),
				path);
	}

	/* perform the syscall */
	ret = lstat(path, &st);
	if (ret == -1)
		return errno_reply(op_code, errno, answer_header,
				answer_payload);

	if (dbg)
		dump_stat(&st);

	/* build the answer */
	ret = df_build_payload(answer_payload, &size,
			DF_DATA_STAT, &st,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);
	memset(answer_header, 0, sizeof(*answer_header));
	answer_header->payload_size = size;
	answer_header->op_code = op_code;
	answer_header->error = 0;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int action_readlink(struct df_packet_header *header, char *payload,
		struct df_packet_header *answer_header, char **answer_payload)
{
	int ret;
	char __attribute__((cleanup(char_array_free))) *path = NULL;
	int64_t path_len;
	int64_t target_len;
	char __attribute__((cleanup(char_array_free))) *link = NULL;
	size_t link_len = 0;
	size_t offset = 0;
	size_t size = 0;
	enum df_op op_code = DF_OP_GETATTR;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &path_len, &path,
			DF_DATA_INT, &target_len,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);
	if (path[path_len - 1] != '\0')
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);

	if (dbg) {
		fprintf(stderr, "action %s\n", df_op_code_to_str(op_code));
		fprintf(stderr, "\tinput :\n");
		fprintf(stderr, "\t  path_len   = %lld\n", path_len);
		fprintf(stderr, "\t  path       = %.*s\n", (int)path_len, path);
		fprintf(stderr, "\t  target_len = %lld\n", target_len);
	}

	/* perform the syscall */
	link = malloc(target_len);
	ret = readlink(path, link, target_len - 1);
	if (ret == -1)
		return errno_reply(op_code, errno, answer_header,
				answer_payload);
	link_len = MIN(ret + 1, target_len);
	link[link_len - 1] = '\0';

	/* build the answer */
	ret = df_build_payload(answer_payload, &size,
			DF_DATA_BUFFER, link_len, link,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);
	memset(answer_header, 0, sizeof(*answer_header));
	answer_header->payload_size = size;
	answer_header->op_code = op_code;
	answer_header->error = 0;

	if (dbg) {
		fprintf(stderr, "\tresult :\n");
		fprintf(stderr, "\t  result : %s\n", link);
	}

	return 0;
}

static int action_readdir(struct df_packet_header *header, char *payload,
		struct df_packet_header *answer_header, char **answer_payload)
{
	DIR *dp;
	struct dirent *de;
	int ret;
	size_t offset = 0;
	char __attribute__ ((cleanup(char_array_free))) *path = NULL;
	struct stat st;
	size_t size = 0;
	int64_t path_len;
	enum df_op op_code = DF_OP_READDIR;
	int64_t ioffset;
	struct fuse_file_info fi;

	/* retrieve the arguments */
	ret = df_parse_payload(payload, &offset, header->payload_size,
			DF_DATA_BUFFER, &path_len, &path,
			DF_DATA_INT, &ioffset,
			DF_DATA_FUSE_FILE_INFO, &fi,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);
	if (path[path_len - 1] != '\0')
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);

	if (dbg) {
		fprintf(stderr, "path     = %s\n", path);
		fprintf(stderr, "path_len = %lld\n", path_len);
		fprintf(stderr, "ioffset  = %lld\n", ioffset);
		fprintf(stderr, "action %s %s\n", df_op_code_to_str(op_code),
				path);
	}

	dp = opendir(path);
	if (dp == NULL)
		return errno_reply(op_code, -errno, answer_header,
				answer_payload);

	/* build the answer */
	ret = df_build_payload(answer_payload, &size,
			DF_DATA_FUSE_FILE_INFO, &fi,
			DF_DATA_BLOCK_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);


	while ((de = readdir(dp)) != NULL) {
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		ret = df_build_payload(answer_payload, &size,

				DF_DATA_BUFFER, strlen(de->d_name) + 1,
				de->d_name,
				DF_DATA_STAT, &st,
				DF_DATA_BLOCK_END);
		if (0 > ret)
			return errno_reply(op_code, -ret, answer_header,
				answer_payload);
	}
	closedir(dp);

	/* terminate the payload */
	ret = df_build_payload(answer_payload, &size,
			DF_DATA_END);
	if (0 > ret)
		return errno_reply(op_code, -ret, answer_header,
				answer_payload);

	memset(answer_header, 0, sizeof(*answer_header));
	answer_header->payload_size = size;
	answer_header->op_code = op_code;
	answer_header->error = 0;

	return 0;
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
		struct df_packet_header *answer_header, char **answer_payload)
{
	return errno_reply(header->op_code, ENOSYS, answer_header,
			answer_payload);
}

typedef int (*action_t)(struct df_packet_header *header, char *payload,
		struct df_packet_header *answer_header, char **answer_payload);

static action_t dispatch_table[] = {
	[DF_OP_INVALID] = action_enosys,

	[DF_OP_READDIR] = action_readdir,
	[DF_OP_GETATTR] = action_getattr,
	[DF_OP_READLINK] = action_readlink,
	[DF_OP_MKDIR] = action_enosys,
	[DF_OP_OPEN] = action_enosys,
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
		struct df_packet_header *answer_header, char **answer_payload)
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

	return action(header, payload, answer_header, answer_payload);
}

static int event_loop(int sock)
{
	int ret;
	struct df_packet_header header;
	char __attribute__ ((cleanup(char_array_free))) *payload = NULL;
	struct df_packet_header answer_header;
	char __attribute__ ((cleanup(char_array_free))) *answer_payload = NULL;

	do {
		memset(&header, 0, sizeof(header));
		ret = df_read_message(sock, &header, &payload);
		if (0 > ret)
			return ret;

		memset(&answer_header, 0, sizeof(answer_header));
		ret = dispatch(&header, payload,
				&answer_header, &answer_payload);
		FREE(payload);
		if (0 > ret)
			return ret;

		ret = df_write_message(sock, &answer_header, answer_payload);
		FREE(answer_payload);
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
