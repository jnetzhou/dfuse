#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdarg.h>

#include <fuse.h>

#include "df_protocol.h"
#include "df_io.h"
#include "df_data_types.h"

static const char * const op_to_str[] = {
	[DF_OP_INVALID]     = "DF_OP_INVALID = 0",
	[DF_OP_READDIR]     = "DF_OP_READDIR",

	[DF_OP_GETATTR]     = "DF_OP_GETATTR",
	[DF_OP_READLINK]    = "DF_OP_READLINK",
	[DF_OP_MKDIR]       = "DF_OP_MKDIR",
	[DF_OP_OPEN]        = "DF_OP_OPEN",
	[DF_OP_RELEASE]     = "DF_OP_RELEASE",
	[DF_OP_READ]        = "DF_OP_READ",
	[DF_OP_WRITE]       = "DF_OP_WRITE",
	[DF_OP_UNLINK]      = "DF_OP_UNLINK",
	[DF_OP_RMDIR]       = "DF_OP_RMDIR",

	[DF_OP_TRUNCATE]    = "DF_OP_TRUNCATE",
	[DF_OP_RENAME]      = "DF_OP_RENAME",
	[DF_OP_CHMOD]       = "DF_OP_CHMOD",
	[DF_OP_CHOWN]       = "DF_OP_CHOWN",
	[DF_OP_ACCESS]      = "DF_OP_ACCESS",
	[DF_OP_SYMLINK]     = "DF_OP_SYMLINK",
	[DF_OP_LINK]        = "DF_OP_LINK",

	[DF_OP_MKNOD]       = "DF_OP_MKNOD",
	[DF_OP_UTIMENS]     = "DF_OP_UTIMENS",
	[DF_OP_STATFS]      = "DF_OP_STATFS",
	[DF_OP_FSYNC]       = "DF_OP_FSYNC",
	[DF_OP_FALLOCATE]   = "DF_OP_FALLOCATE",
	[DF_OP_SETXATTR]    = "DF_OP_SETXATTR",
	[DF_OP_GETXATTR]    = "DF_OP_GETXATTR",
	[DF_OP_LISTXATTR]   = "DF_OP_LISTXATTR",
	[DF_OP_REMOVEXATTR] = "DF_OP_REMOVEXATTR",

	[DF_OP_QUIT]        = "DF_OP_QUIT",
};

const char *df_op_code_to_str(enum df_op op)
{
	return op_to_str[op];
}

int df_send_handshake(int fd, uint32_t prot_version)
{
	ssize_t ret;

	prot_version = htobe32(prot_version);

	ret = df_write(fd, &prot_version, sizeof(prot_version));
	if (0 > ret)
		return -errno;

	return 0;
}

int df_read_handshake(int fd, uint32_t *prot_version)
{
	ssize_t ret;

	ret = df_read(fd, prot_version, sizeof(*prot_version));
	if (0 > ret)
		return -errno;

	*prot_version = htobe32(*prot_version);

	return 0;
}

/* converts back a header from big endian to host order */
static void unmarshall_header(struct df_packet_header *header)
{
	header->payload_size = be16toh(header->payload_size);
	if (header->is_host_packet)
		header->error = be16toh(header->error);
}

/* reads a message header */
static int read_header(int fd, struct df_packet_header *header)
{
	ssize_t ret;

	ret = df_read(fd, header, sizeof(*header));
	if (0 > ret)
		return ret;

	unmarshall_header(header);

	return 0;
}

/* reads a payload, given a header we have just received */
static int read_payload(int fd, struct df_packet_header *header, char **payload)
{
	*payload = calloc(header->payload_size, sizeof(**payload));
	if (NULL == *payload)
		return -errno;

	return df_read(fd, *payload, header->payload_size);
}

int df_read_message(int fd, struct df_packet_header *header, char **payload)
{
	int ret;

	if (NULL == header || NULL == payload || NULL != *payload)
		return -EINVAL;

	ret = read_header(fd, header);
	if (0 > ret)
		return ret;

	ret = read_payload(fd, header, payload);
	if (0 > ret)
		return ret;

	return 0;
}

/* reallocates space to append new data to the payload */
static int adjust_payload(char **payload, size_t *size, size_t data_size)
{
	size_t new_size;
	char *new_payload;

	new_size = *size + data_size;
	new_payload = realloc(*payload, new_size);
	if (NULL == new_payload)
		return -errno;
	*payload = new_payload;

	return 0;
}

/* adjusts the size of the payload and appends the data at it's end */
static int append_data(char **payload, size_t *size, void *marshalled_data,
		size_t marshalled_data_size)
{
	int ret;

	ret = adjust_payload(payload, size, MARSHALLED_FFI_SIZE);
	if (0 > ret)
		return ret;

	memcpy(*payload + *size, marshalled_data, marshalled_data_size);
	*size = *size + marshalled_data_size;

	return 0;
}

/* marshalls a ffi struct and append it to the payload resized to contain it */
static int append_fuse_file_info(char **payload, size_t *size,
		struct fuse_file_info *data)
{
	int64_t marshalled_ffi[MARSHALLED_FFI_FIELDS];

	marshall_fuse_file_info(data, marshalled_ffi);

	return append_data(payload, size, marshalled_ffi, MARSHALLED_FFI_SIZE);
}

static int append_int(char **payload, size_t *size, int64_t data)
{
	data = htobe64(data);

	return append_data(payload, size, &data, sizeof(data));
}

static int append_stat(char **payload, size_t *size, struct stat *data)
{
	int64_t marshalled_stat[MARSHALLED_STAT_FIELDS];

	marshall_stat(data, marshalled_stat);

	return append_data(payload, size, marshalled_stat,
			MARSHALLED_STAT_SIZE);
}
static int append_statvfs(char **payload, size_t *size, struct statvfs *data)
{
	int64_t marshalled_statvfs[MARSHALLED_STATVFS_FIELDS];

	marshall_statvfs(data, marshalled_statvfs);

	return append_data(payload, size, marshalled_statvfs,
			MARSHALLED_STATVFS_SIZE);
}

static int append_timespec(char **payload, size_t *size, struct timespec *data)
{
	int64_t marshalled_timespec[MARSHALLED_TIMESPEC_FIELDS];

	marshall_timespec(data, marshalled_timespec);

	return append_data(payload, size, marshalled_timespec,
			MARSHALLED_TIMESPEC_SIZE);
}


int df_build_payload(char **payload, size_t *size, ...)
{
	va_list args;
	enum df_data_type data_type;
	int loop = 1;
	int ret = 0;

	/* data types */
	void *buffer_data;
	size_t buffer_size;
	struct fuse_file_info ffi_data;
	int64_t int_data;
	struct stat stat_data;
	struct statvfs statvfs_data;
	struct timespec timespec_data;

	if (NULL == payload || NULL != *payload || NULL == size)
		return -EINVAL;
	*size = 0;

	va_start(args, size);
	do {
		data_type = va_arg(args, enum df_data_type);
		/* prefix each datum by it's type */
		ret = append_int(payload, size, data_type);
		if (0 > ret)
			goto out;

		switch (data_type) {
		case DF_DATA_BUFFER:
			buffer_data = va_arg(args, void *);
			buffer_size = va_arg(args, size_t);
			ret = append_data(payload, size, buffer_data,
					buffer_size);
			if (0 > ret)
				goto out;
			break;

		case DF_DATA_FUSE_FILE_INFO:
			ffi_data = va_arg(args, struct fuse_file_info);
			ret = append_fuse_file_info(payload, size, &ffi_data);
			if (0 > ret)
				goto out;
			break;

		case DF_DATA_INT:
			int_data = va_arg(args, int64_t);
			ret = append_int(payload, size, int_data);
			if (0 > ret)
				goto out;
			break;

		case DF_DATA_STAT:
			stat_data = va_arg(args, struct stat);
			ret = append_stat(payload, size, &stat_data);
			if (0 > ret)
				goto out;
			break;

		case DF_DATA_STATVFS:
			statvfs_data = va_arg(args, struct statvfs);
			ret = append_statvfs(payload, size, &statvfs_data);
			if (0 > ret)
				goto out;
			break;


		case DF_DATA_TIMESPEC:
			timespec_data = va_arg(args, struct timespec);
			ret = append_timespec(payload, size, &timespec_data);
			if (0 > ret)
				goto out;
			break;

		case DF_DATA_END:
			loop = 0;
			ret = 0;
			break;
		}
	} while (loop);

out:
	va_end(args);

	return ret;
}

/* converts a header from host order to big endian */
static void marshall_header(struct df_packet_header *header)
{
	header->payload_size = htobe16(header->payload_size);
	if (header->is_host_packet)
		header->error = htobe16(header->error);
}

/* write an entire message, header + payload */
int df_write_message(int fd, struct df_packet_header *header, char *payload)
{
	ssize_t ret;

	if (0 > fd || NULL == header || NULL == payload)
		return -EINVAL;

	marshall_header(header);
	ret = df_write(fd, header, sizeof(*header));
	unmarshall_header(header);
	if (0 > ret)
		return ret;

	return df_write(fd, payload, header->payload_size);
}
