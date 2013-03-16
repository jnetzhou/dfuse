#include <inttypes.h>
#include <endian.h>

#include <fuse.h>

#include "df_data_types.h"

/**
 * Converts the marshalled struct's content to big endian
 * @param ms Marshalled struct
 * @param size Number of elements of the int64 array
 */
static void marshalled_struct_to_be64(int64_t *ms, int size)
{
	while (size--)
		ms[size] = htobe64(ms[size]);
}

static void marshalled_struct_from_be64(int64_t *ms, int size)
{
	while (size--)
		ms[size] = be64toh(ms[size]);
}

void marshall_fuse_file_info(struct fuse_file_info *ffi,
		int64_t *marshalled_ffi)
{
	marshalled_ffi[0] = ffi->flags;
	marshalled_ffi[1] = ffi->fh_old;
	marshalled_ffi[2] = (ffi->direct_io << 0) +
		(ffi->keep_cache << 1) +
		(ffi->flush << 2) +
		(ffi->nonseekable << 3) +
		(ffi->flock_release << 4);
	marshalled_ffi[3] = ffi->padding;
	marshalled_ffi[4] = ffi->fh;
	marshalled_ffi[5] = ffi->lock_owner;

	marshalled_struct_to_be64(marshalled_ffi, MARSHALLED_FFI_FIELDS);
}

#define BIT0 (1 << 0)
#define BIT1 (1 << 1)
#define BIT2 (1 << 2)
#define BIT3 (1 << 3)
#define BIT4 (1 << 4)

void unmarshall_fuse_file_info(struct fuse_file_info *ffi,
		int64_t *marshalled_ffi)
{
	marshalled_struct_from_be64(marshalled_ffi, MARSHALLED_FFI_FIELDS);

	ffi->flags = marshalled_ffi[0];
	ffi->fh_old = marshalled_ffi[1];
	ffi->direct_io = (marshalled_ffi[2] & BIT0) != 0;
	ffi->keep_cache = (marshalled_ffi[2] & BIT1) != 0;
	ffi->flush = (marshalled_ffi[2] & BIT2) != 0;
	ffi->nonseekable = (marshalled_ffi[2] & BIT3) != 0;
	ffi->flock_release = (marshalled_ffi[2] & BIT4) != 0;
	ffi->padding = marshalled_ffi[3];
	ffi->fh = marshalled_ffi[4];
	ffi->lock_owner = marshalled_ffi[5];
}

void marshall_stat(struct stat *st, int64_t *marshalled_stat)
{
	marshalled_stat[0] = st->st_dev;
	marshalled_stat[1] = st->st_ino;
	marshalled_stat[2] = st->st_mode;
	marshalled_stat[3] = st->st_nlink;
	marshalled_stat[4] = st->st_uid;
	marshalled_stat[5] = st->st_gid;
	marshalled_stat[6] = st->st_rdev;
	marshalled_stat[7] = st->st_size;
	marshalled_stat[8] = st->st_blksize;
	marshalled_stat[9] = st->st_blocks;
	marshalled_stat[10] = st->st_atime;
	marshalled_stat[11] = st->st_mtime;
	marshalled_stat[12] = st->st_ctime;

	marshalled_struct_to_be64(marshalled_stat, MARSHALLED_STAT_FIELDS);
}

void unmarshall_stat(struct stat *st, int64_t *marshalled_stat)
{
	marshalled_struct_from_be64(marshalled_stat, MARSHALLED_STAT_FIELDS);

	st->st_dev = marshalled_stat[0];
	st->st_ino = marshalled_stat[1];
	st->st_mode = marshalled_stat[2];
	st->st_nlink = marshalled_stat[3];
	st->st_uid = marshalled_stat[4];
	st->st_gid = marshalled_stat[5];
	st->st_rdev = marshalled_stat[6];
	st->st_size = marshalled_stat[7];
	st->st_blksize = marshalled_stat[8];
	st->st_blocks = marshalled_stat[9];
	st->st_atime = marshalled_stat[10];
	st->st_mtime = marshalled_stat[11];
	st->st_ctime = marshalled_stat[12];
}

void marshall_statvfs(struct statvfs *stv, int64_t *marshalled_statvfs)
{
	marshalled_statvfs[0] = stv->f_bsize;
	marshalled_statvfs[1] = stv->f_frsize;
	marshalled_statvfs[2] = stv->f_blocks;
	marshalled_statvfs[3] = stv->f_bfree;
	marshalled_statvfs[4] = stv->f_bavail;
	marshalled_statvfs[5] = stv->f_files;
	marshalled_statvfs[6] = stv->f_ffree;
	marshalled_statvfs[7] = stv->f_favail;
	marshalled_statvfs[8] = stv->f_fsid;
	marshalled_statvfs[9] = stv->f_flag;
	marshalled_statvfs[10] = stv->f_namemax;

	marshalled_struct_to_be64(marshalled_statvfs,
			MARSHALLED_STATVFS_FIELDS);
}

void unmarshall_statvfs(struct statvfs *stv, int64_t *marshalled_statvfs)
{
	marshalled_struct_from_be64(marshalled_statvfs,
			MARSHALLED_STATVFS_FIELDS);

	stv->f_bsize = marshalled_statvfs[0];
	stv->f_frsize = marshalled_statvfs[1];
	stv->f_blocks = marshalled_statvfs[2];
	stv->f_bfree = marshalled_statvfs[3];
	stv->f_bavail = marshalled_statvfs[4];
	stv->f_files = marshalled_statvfs[5];
	stv->f_ffree = marshalled_statvfs[6];
	stv->f_favail = marshalled_statvfs[7];
	stv->f_fsid = marshalled_statvfs[8];
	stv->f_flag = marshalled_statvfs[9];
	stv->f_namemax = marshalled_statvfs[10];
}

void marshall_timespec(struct timespec *ts, int64_t *marshalled_timespec)
{
	marshalled_timespec[0] = ts->tv_sec;
	marshalled_timespec[1] = ts->tv_nsec;

	marshalled_struct_to_be64(marshalled_timespec,
			MARSHALLED_TIMESPEC_FIELDS);
}

void unmarshall_timespec(struct timespec *ts, int64_t *marshalled_timespec)
{
	marshalled_struct_from_be64(marshalled_timespec,
			MARSHALLED_TIMESPEC_FIELDS);

	ts->tv_sec = marshalled_timespec[0];
	ts->tv_nsec = marshalled_timespec[1];
}
