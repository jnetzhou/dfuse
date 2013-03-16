#include <unistd.h>

#include <errno.h>
#include <assert.h>

static ssize_t my_read(int fd, void *buf, size_t count)
{
	return TEMP_FAILURE_RETRY(read(fd, buf, count));
}

static ssize_t my_write(int fd, void *buf, size_t count)
{
	return TEMP_FAILURE_RETRY(write(fd, buf, count));
}

ssize_t df_read(int fd, void *buf, size_t count)
{
	size_t read_so_far = 0;
	ssize_t read_this_time = 0;

	while (read_so_far < count) {
		read_this_time = my_read(fd,
				buf + read_so_far,
				count - read_so_far);
		if (-1 == read_this_time)
			return -errno;
		if (0 == read_this_time)
			return 0;

		read_so_far += read_this_time;
	}

	assert(read_so_far == count);

	return read_so_far;
}

ssize_t df_write(int fd, void *buf, size_t count)
{
	size_t written_so_far = 0;
	ssize_t written_this_time = 0;

	while (written_so_far < count) {
		written_this_time = my_write(fd,
				buf + written_so_far,
				count - written_so_far);
		if (-1 == written_this_time)
			return -errno;
		if (0 == written_this_time)
			return 0;

		written_so_far += written_this_time;
	}

	assert(written_so_far == count);

	return written_so_far;
}
