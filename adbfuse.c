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
static int df_readlink(const char *path, char *buf, size_t size)
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
