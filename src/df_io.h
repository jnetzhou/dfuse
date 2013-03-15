#ifndef DF_IO_H
#define DF_IO_H

size_t df_read(int fd, void *buf, size_t count);
size_t int df_write(int fd, void *buf, size_t count);

#endif /* DF_IO_H */
