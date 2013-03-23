#ifndef DATA_TYPES_H_
#define DATA_TYPES_H_

#define ELT_SIZE sizeof(int64_t)

#define MARSHALLED_FFI_FIELDS 6
#define MARSHALLED_FFI_SIZE ((MARSHALLED_FFI_FIELDS) * ELT_SIZE)

#define MARSHALLED_STAT_FIELDS 13
#define MARSHALLED_STAT_SIZE ((MARSHALLED_STAT_FIELDS) * ELT_SIZE)

#define MARSHALLED_STATVFS_FIELDS 11
#define MARSHALLED_STATVFS_SIZE ((MARSHALLED_STATVFS_FIELDS) * ELT_SIZE)

#define MARSHALLED_TIMESPEC_FIELDS 2
#define MARSHALLED_TIMESPEC_SIZE ((MARSHALLED_TIMESPEC_FIELDS) * ELT_SIZE)

void marshall_fuse_file_info(struct fuse_file_info *ffi,
		int64_t *marshalled_ffi);
void unmarshall_fuse_file_info(struct fuse_file_info *ffi,
		int64_t *marshalled_ffi);

void dump_stat(struct stat *st);
void marshall_stat(struct stat *st, int64_t *marshalled_stat);
void unmarshall_stat(struct stat *st, int64_t *marshalled_stat);

void marshall_statvfs(struct statvfs *stv, int64_t *marshalled_statvfs);
void unmarshall_statvfs(struct statvfs *stv, int64_t *marshalled_statvfs);

void marshall_timespec(struct timespec *ts, int64_t *marshalled_timespec);
void unmarshall_timespec(struct timespec *ts, int64_t *marshalled_timespec);

#endif /* DATA_TYPES_H_ */
