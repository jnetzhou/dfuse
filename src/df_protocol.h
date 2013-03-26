#ifndef DF_PROTOCOL_C
#define DF_PROTOCOL_H

#define DF_HEADER_SIZE 8

#define DF_PROTOCOL_VERSION 1U

/* list of the options supported */
enum df_op {
	DF_OP_INVALID = 0,
	DF_OP_READDIR,

	/* TODO add support for closedir/opendir in a second step*/
	DF_OP_GETATTR,
	DF_OP_READLINK,
	DF_OP_MKDIR,
	DF_OP_OPEN,
	DF_OP_RELEASE,
	DF_OP_READ,
	DF_OP_WRITE,
	DF_OP_UNLINK,
	DF_OP_RMDIR,

	DF_OP_TRUNCATE,
	DF_OP_RENAME,
	DF_OP_CHMOD,
	DF_OP_CHOWN,
	DF_OP_ACCESS,
	DF_OP_SYMLINK,
	DF_OP_LINK,

	DF_OP_MKNOD,
	DF_OP_UTIMENS,
	DF_OP_STATFS,
	DF_OP_FSYNC,
	DF_OP_FALLOCATE,
	DF_OP_SETXATTR,
	DF_OP_GETXATTR,
	DF_OP_LISTXATTR,
	DF_OP_REMOVEXATTR,

	DF_OP_QUIT, /**< send a "bye bye" message */
};

/* packet header, aligned on 64bits */
struct df_packet_header {
	/** size of useful data in the payload part of the packet */
	uint32_t payload_size;
	/** code of the operation requested */
	uint8_t op_code;
	/** non-zero if the packet is an host packet */
	uint8_t is_host_packet;
	/** error code i.e. errno value */
	uint16_t error;
};

/* types of data exchanged */
enum df_data_type {
	DF_DATA_END = 0,

	DF_DATA_BLOCK_END,

	DF_DATA_BUFFER,
	DF_DATA_FUSE_FILE_INFO,
	DF_DATA_INT,
	DF_DATA_STAT,
	DF_DATA_STATVFS,
	DF_DATA_TIMESPEC,
};

int fill_header(struct df_packet_header *header, size_t size,
		enum df_op op_code, int error);

const char *df_op_code_to_str(enum df_op);

int df_send_handshake(int fd, uint32_t prot_version);

int df_read_handshake(int fd, uint32_t *prot_version);

int df_read_message(int fd, struct df_packet_header *header, char **payload);

/**
 * parses the payload content, storing values according to the
 * (df_data_type, lvalue_pointer) passed as an argument list
 * @param payload Payload to parse
 * @param offset Points to a value, initially equal to zero, updated in each
 * call of df_parse_payload. When all has been parsed, it should be equal to
 * size
 * @param size Total size of the payload
 * @return errno-compatible negative value on error, otherwise 0
 */
int df_parse_payload(char *payload, size_t *offset, size_t size, ...);

int df_vparse_payload(char *payload, size_t *offset, size_t size, va_list args);

/**
 * builds a payload, given a list of arguments, (df_data_type, value) pairs,
 * ended by a DF_DATA_END, if payload is NULL, it is allocated, if not, new
 * data is appended, with payload being potentially reallocated
 */
int df_build_payload(char **payload, size_t *size, ...);

int df_vbuild_payload(char **payload, size_t *size, va_list args);

/* write an entire message, header + payload */
int df_write_message(int fd, struct df_packet_header *header, char *payload);

int df_remote_call(int sock, enum df_op op_code, ...);

int df_remote_answer(int sock, enum df_op op_code, ...);

#endif /* DF_PROTOCOL_H */
