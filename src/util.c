#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <time.h>

#include "util.h"

static void do_nothing(size_t size)
{
	;
}

static void (*try_to_free_routine)(size_t size) = do_nothing;

try_to_free_t set_try_to_free_routine(try_to_free_t routine)
{
	try_to_free_t old = try_to_free_routine;
	if (!routine)
		routine = do_nothing;
	try_to_free_routine = routine;
	return old;
}

void *xmalloc(size_t size)
{
	void *ret = malloc(size);
	if (unlikely(!ret) && unlikely(!size))
		ret = malloc(1);
	if (unlikely(!ret)) {
		try_to_free_routine(size);
		ret = malloc(size);
		if (!ret && !size)
			ret = malloc(1);
		if (!ret)
			panic("Out of memory");
	}
	return ret;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ret = calloc(nmemb, size);
	if (unlikely(!ret) && unlikely(!nmemb || !size))
		ret = calloc(1, 1);
	if (unlikely(!ret)) {
		try_to_free_routine(nmemb * size);
		ret = calloc(nmemb, size);
		if (!ret && (!nmemb || !size))
			ret = calloc(1, 1);
		if (!ret)
			panic("Out of memory");
	}
	return ret;
}

static ssize_t _read(int fd, void *buf, size_t len)
{
	ssize_t nr;
	while (true) {
		nr = read(fd, buf, len);
		if (unlikely(nr < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return nr;
	}
}

static ssize_t _write(int fd, const void *buf, size_t len)
{
	ssize_t nr;
	while (true) {
		nr = write(fd, buf, len);
		if (unlikely(nr < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return nr;
	}
}

ssize_t xread(int fd, void *buf, size_t count)
{
	char *p = buf;
	ssize_t total = 0;

	while (count > 0) {
		ssize_t loaded = _read(fd, p, count);
		if (unlikely(loaded < 0))
			return -1;
		if (unlikely(loaded == 0))
			return total;
		count -= loaded;
		p += loaded;
		total += loaded;
	}

	return total;
}

ssize_t xwrite(int fd, const void *buf, size_t count)
{
	const char *p = buf;
	ssize_t total = 0;

	while (count > 0) {
		ssize_t written = _write(fd, p, count);
		if (unlikely(written < 0))
			return -1;
		if (unlikely(!written)) {
			errno = ENOSPC;
			return -1;
		}
		count -= written;
		p += written;
		total += written;
	}

	return total;
}

/*
 * Return the read value on success, or -1 if efd has been made nonblocking and
 * errno is EAGAIN.  If efd has been marked blocking or the eventfd counter is
 * not zero, this function doesn't return error.
 */
int eventfd_xread(int efd)
{
	int ret;
	eventfd_t value = 0;

	do {
		ret = eventfd_read(efd, &value);
	} while (unlikely(ret < 0) && errno == EINTR);

	if (ret == 0)
		ret = value;
	else if (unlikely(errno != EAGAIN))
		panic("eventfd_read() failed");

	return ret;
}

void eventfd_xwrite(int efd, int value)
{
	int ret;

	do {
		ret = eventfd_write(efd, (eventfd_t)value);
	} while (unlikely(ret < 0) && (errno == EINTR || errno == EAGAIN));

	if (unlikely(ret < 0))
		panic("eventfd_write() failed");
}
