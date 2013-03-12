/**
 * @file adbfuse.c
 * @author carrier.nicolas0@gmail.com
 * @date 08 mar. 2013
 *
 * Fuse file system over the adb protocol
 */
#include <stdio.h>
#include <errno.h>

#include <sysdeps.h>

#include <adb.h>
#include <fdevent.h>
#include <adb_client.h>
#include <file_sync_service.h>

#include "adb_bridge.h"

#define FUSE_USE_VERSION 26

#include <fuse.h>

/**
 * TODO
 * from man 2 access :
 *   "check real user's permissions for a file"
 * @param path
 * @param mask
 * @return
 */
static int df_access(const char *path, int mask)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 chmod :
 *   "chmod, fchmod - change permissions of a file"
 * @param path
 * @param mode
 * @return
 */
static int df_chmod(const char *path, mode_t mode)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 chown :
 *   "chown, fchown, lchown - change ownership of a file"
 * @param path
 * @param uid_t
 * @param gid_t
 * @return
 */
static int df_chown(const char *path, uid_t uid, gid_t gid)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * example implementation uses lstat
 * from man 2 lstat :
 *   "stat, fstat, lstat - get file status"
 * @param path
 * @param stbuf
 * @return
 */
static int df_getattr(const char *path, struct stat *stbuf)
{
	int ret;
	int len;
	int syncfd;
	syncmsg msg;

	syncfd = adb_connect("sync:");
	if (0 > syncfd) {
		fprintf(stderr, "sync error: %s\n", adb_error());
		return -EIO;
	}

	len = strlen(path);

	msg.req.id = ID_STAT;
	msg.req.namelen = htoll(len);
	ret = writex(syncfd, &msg.req, sizeof(msg.req));
	if (0 > ret)
		return -errno;
	ret = writex(syncfd, path, len);
	if (0 > ret)
		return -errno;

	ret = readx(syncfd, &msg.stat, sizeof(msg.stat));
	if (0 > ret)
		return -errno;

	if (msg.stat.id != ID_STAT)
		return -EIO;

	stbuf->st_mode = msg.stat.mode;
	stbuf->st_size = msg.stat.size;
	stbuf->st_mtime = msg.stat.time;

	sync_quit(syncfd);

	return 0;
}

/**
 * TODO
 * from man 2 mkidr :
 *   "mkdir - create a directory"
 * @param path
 * @param mode
 * @return
 */
static int df_mkdir(const char *path, mode_t mode)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 mknod :
 *   "mknod - create a special or ordinary file"
 * @param path
 * @param mode
 * @param rdev
 * @return
 */
static int df_mknod(const char *path, mode_t mode, dev_t rdev)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 open :
 *   "open, creat - open and possibly create a file or device"
 * @param path
 * @param fi
 * @return
 */
static int df_open(const char *path, struct fuse_file_info *fi)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 link :
 *   "link - make a new name for a file"
 * @param from
 * @param to
 * @return
 */
static int df_link(const char *from, const char *to)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 read :
 *   "read - read from a file descriptor"
 * @param path
 * @param buf
 * @param size
 * @param offset
 * @param fi
 * @return
 */
static int df_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 read :
 *   "readdir - read directory entry"
 * @param path
 * @param buf
 * @param filler
 * @param offset
 * @param fi
 * @return
 */
static int df_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	int ret;
	int syncfd;
	void sync_ls_cb(unsigned mode, unsigned size, unsigned time,
			const char *name, void *cookie)
	{
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_mode = mode;
		st.st_size = size;
		st.st_mtime = time;
		ret = filler(buf, name, NULL, 0);
		if (0 > ret)
			fprintf(stderr, "%s : %s\n",  __func__, adb_error());
	}

	syncfd = adb_connect("sync:");
	if (0 > syncfd) {
		fprintf(stderr, "sync error: %s\n", adb_error());
		return -EIO;
	}

	ret = sync_ls(syncfd, path, sync_ls_cb, 0);
	if (0 > ret)
		return -EIO;

	sync_quit(syncfd);

	return 0;
}

/**
 * TODO
 * from man 2 readlink :
 *   "readlink - read value of a symbolic link"
 * @param path
 * @param buf
 * @param size
 * @return
 */
static int df_readlink(const char *path, char *buf, size_t size)
{
	int shellfd;
	int len;
	int ret;
	char *cmd;
	char *arrow;
	char *cr;

	/* send "ls -l PATH" command */
	ret = asprintf(&cmd, "shell:ls -l %s", path);
	if (-1 == ret)
		return -ENOMEM;
	shellfd = adb_connect(cmd);
	free(cmd);
	if (0 > shellfd) {
		fprintf(stderr, "shell error: %s\n", adb_error());
		return -EIO;
	}

	/* read answer */
	len = TEMP_FAILURE_RETRY(adb_read(shellfd, buf, size));
	if (0 == len)
		return -ENOENT;
	if (0 > len)
		return -errno;
	if (len >= size)
		return -ENOMEM;
	adb_close(shellfd);

	/* strip \n */
	buf[len - 1] = '\0';
	/* strip \r if any */
	if (1 < len) {
		cr = strchr(buf, '\r');
		if (NULL != cr)
			*cr = '\0';
	}
	/* the link target is after the " -> " pattern */
	arrow = strstr(buf, " -> ");
	if (NULL == arrow)
		return -ENOENT;
	arrow += 4;

	memmove(buf, arrow, len - (arrow - buf));

	return 0;
}

/**
 * TODO
 * from man 2 rename :
 *   "rename - change the name or location of a file"
 * @param from
 * @param to
 * @return
 */
static int df_rename(const char *from, const char *to)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 rename :
 *   "rmdir - delete a directory"
 * @param path
 * @return
 */
static int df_rmdir(const char *path)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 symlink :
 *   "symlink - make a new name for a file"
 * @param from
 * @param to
 * @return
 */
static int df_symlink(const char *from, const char *to)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 truncate :
 *   "truncate, ftruncate - truncate a file to a specified length"
 * @param path
 * @param size
 * @return
 */
static int df_truncate(const char *path, off_t size)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 unlink :
 *   "unlink - delete a name and possibly the file it refers to"
 * @param path
 * @return
 */
static int df_unlink(const char *path)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 write :
 *   "write - write to a file descriptor"
 * @param path
 * @param buf
 * @param size
 * @param offset
 * @param fi
 * @return
 */
static int df_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

static struct fuse_operations df_oper = {
	.access		= df_access,
	.chmod		= df_chmod,
	.chown		= df_chown,
	.getattr	= df_getattr,
	.mkdir		= df_mkdir,
	.mknod		= df_mknod,
	.open		= df_open,
	.link		= df_link,
	.read		= df_read,
	.readdir	= df_readdir,
	.readlink	= df_readlink,
	.rename		= df_rename,
	.rmdir		= df_rmdir,
	.symlink	= df_symlink,
	.truncate	= df_truncate,
	.unlink		= df_unlink,
	.write		= df_write,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &df_oper, NULL);
}
