#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdarg.h>

#include <fuse.h>

#include "df_protocol.h"
#include "df_io.h"

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
	if (header->generic.is_host_packet)
		header->device.error = be16toh(header->device.error);
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

/*
	DF_DATA_FUSE_FILE_INFO,
	DF_DATA_INT,
	DF_DATA_STAT,
	DF_DATA_STATVFS,
	DF_DATA_STRING_,
	DF_DATA_STRING_LIST,
	DF_DATA_TIMESPEC,
 */
/**
 *
 * @param ms Marshalled struct
 */
static void marshalled_struct_to_be64(int64_t *ms, int size)
{
	while (size--)
		ms[size] = htobe64(ms[size]);
}

#define MARSHALLED_FFI_FIELDS 6

static int marshall_fuse_file_info(struct fuse_file_info *ffi,
		int64_t *marshalled_ffi)
{
	if (NULL == ffi || NULL == marshalled_ffi)
		return -EINVAL;

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

	return 0;
}

static int append_fuse_file_info(char **payload, size_t *size,
		struct fuse_file_info *ffi)
{
	int64_t marshalled_ffi[MARSHALLED_FFI_FIELDS];
	int ret;
	size_t new_size;
	size_t m_ffi_size = MARSHALLED_FFI_FIELDS * sizeof(int64_t);
	char *new_payload;

	if (NULL == size || NULL == ffi)
		return -EINVAL;

	ret = marshall_fuse_file_info(ffi, marshalled_ffi);
	if (0 > ret)
		return ret;

	new_size = *size + m_ffi_size;
	new_payload = realloc(*payload, new_size);
	if (NULL == new_payload)
		return -errno;
	*payload = new_payload;

	memcpy(*payload + *size, marshalled_ffi, m_ffi_size);
	*size = new_size;

	return 0;
}

int df_build_payload(char **payload, size_t *size, ...)
{
	va_list args;
	enum df_data_type data_type;
	int loop = 1;
	int ret = 1;
	struct fuse_file_info ffi;

	if (NULL == payload || NULL != *payload || NULL == size)
		return -EINVAL;
	*size = 0;

	va_start(args, size);
	do {
		data_type = va_arg(args, enum df_data_type);
		switch (data_type) {
		case DF_DATA_FUSE_FILE_INFO:
			ffi = va_arg(args, struct fuse_file_info);
			ret = append_fuse_file_info(payload, size, &ffi);
			if (0 > ret)
				goto out;
			break;

		case DF_DATA_INT:
			break;

		case DF_DATA_STAT:
			break;

		case DF_DATA_STATVFS:
			break;

		case DF_DATA_STRING_:
			break;

		case DF_DATA_STRING_LIST:
			break;

		case DF_DATA_TIMESPEC:
			break;


		case DF_DATA_INVALID:
			loop = 0;
			break;
		}
	} while (loop);

out:
	va_end(args);

	return ret;
}

/* write an entire message, header + payload */
int df_write_message(int fd, struct df_packet_header *header, char *payload)
{

	return 0;
}


