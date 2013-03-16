#ifndef DF_IO_H
#define DF_IO_H

ssize_t df_read(int fd, void *buf, size_t count);
ssize_t df_write(int fd, void *buf, size_t count);

#endif /* DF_IO_H */
