/**
 * @file adbfuse.c
 * @author carrier.nicolas0@gmail.com
 * @date 08 mar. 2013
 *
 * Fuse file system over the adb protocol
 */
#include <stdio.h>
#include <errno.h>

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
static int adb_access(const char *path, int mask)
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
static int adb_chmod(const char *path, mode_t mode)
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
static int adb_chown(const char *path, uid_t uid, gid_t gid)
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
static int adb_getattr(const char *path, struct stat *stbuf)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 mkidr :
 *   "mkdir - create a directory"
 * @param path
 * @param mode
 * @return
 */
static int adb_mkdir(const char *path, mode_t mode)
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
static int adb_mknod(const char *path, mode_t mode, dev_t rdev)
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
static int adb_open(const char *path, struct fuse_file_info *fi)
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
static int adb_link(const char *from, const char *to)
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
static int adb_read(const char *path, char *buf, size_t size, off_t offset,
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
static int adb_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
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
static int adb_readlink(const char *path, char *buf, size_t size)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

/**
 * TODO
 * from man 2 rename :
 *   "rename - change the name or location of a file"
 * @param from
 * @param to
 * @return
 */
static int adb_rename(const char *from, const char *to)
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
static int adb_rmdir(const char *path)
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
static int adb_symlink(const char *from, const char *to)
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
static int adb_truncate(const char *path, off_t size)
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
static int adb_unlink(const char *path)
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
static int adb_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	fprintf(stderr, "%s STUB !!!\n", __func__);

	return -ENOSYS;
}

static struct fuse_operations adb_oper = {
	.access		= adb_access,
	.chmod		= adb_chmod,
	.chown		= adb_chown,
	.getattr	= adb_getattr,
	.mkdir		= adb_mkdir,
	.mknod		= adb_mknod,
	.open		= adb_open,
	.link		= adb_link,
	.read		= adb_read,
	.readdir	= adb_readdir,
	.readlink	= adb_readlink,
	.rename		= adb_rename,
	.rmdir		= adb_rmdir,
	.symlink	= adb_symlink,
	.truncate	= adb_truncate,
	.unlink		= adb_unlink,
	.write		= adb_write,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &adb_oper, NULL);
}
