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
	/** unique id use in a request's packet match that of answer(s) */
	uint8_t request_id;
	/** id of the paquet for a given request_id */
	uint8_t part_id;
	/** size of useful data in the payload part of the packet */
	uint16_t payload_size;

	union {
		struct {
			/** non zero if this paquet terminates the request */
			uint8_t end_of_request;
			/** code of the operation requested */
			uint8_t op_code;
			/** for header alignment on 64bit */
			uint16_t zero_padding;
		} host;
		struct {
			/** non zero if this paquet terminates the answer */
			uint8_t end_of_answer;
			/** for header alignment on 64bit */
			uint8_t zero_padding;
			/** error code i.e. function return value */
			uint16_t error;
		} device;
		struct {
			/** non-zero if it's the last part of the message */
			uint8_t end_of_message;
			/** non-zero if the packet is an host packet */
			uint8_t is_host_packet;
			/** for header alignment on 64bit */
			uint16_t padding;
		} generic;
	};
};

/* types of data exchanged */
enum df_data_type {
	DF_DATA_END = 0,

	DF_DATA_BUFFER,
	DF_DATA_FUSE_FILE_INFO,
	DF_DATA_INT,
	DF_DATA_STAT,
	DF_DATA_STATVFS,
	DF_DATA_STRING,
	DF_DATA_STRING_LIST,
	DF_DATA_TIMESPEC,
};

int df_send_handshake(int fd, uint32_t prot_version);

int df_read_handshake(int fd, uint32_t *prot_version);

int df_read_message(int fd, struct df_packet_header *header, char **payload);

/**
 * builds a payload, given a list of arguments, (df_data_type, value) pairs,
 * ended by a DF_DATA_INVALID, if payload is NULL, it is allocated, if not, new
 * data is appended, with payload being potentially reallocated
 */
int df_build_payload(char **payload, size_t *size, ...);

/* write an entire message, header + payload */
int df_write_message(int fd, struct df_packet_header *header, char *payload);

#endif /* DF_PROTOCOL_H */
