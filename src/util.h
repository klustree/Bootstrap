#ifndef _BS_UTILS_H_
#define _BS_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <search.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>

#include "list.h"

#define panic(fmt, arg...) 	do { fprintf(stderr,"panic!! "fmt,"\n", ##arg); abort(); } while(0)

#define bs_trace(fmt, arg...) 	do { fprintf(stderr, "%s:%s:line %d: "fmt"\n", __FILE__,__func__,__LINE__,##arg); } while(0)
#define bs_debug(fmt, arg...) 	do { fprintf(stderr, fmt"\n", ##arg); } while(0)
#define bs_err(fmt, arg...) 	do { fprintf(stderr, fmt"\n", ##arg); } while(0)
#define bs_warn(fmt, arg...) 	do { fprintf(stderr, fmt"\n", ##arg); } while(0)
#define bs_notice(fmt, arg...) 	do { fprintf(stderr, fmt"\n", ##arg); } while(0)

#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define min(x, y) ({ 		\
	typeof(x) _x = (x);		\
	typeof(y) _y = (y);		\
	(void) (&_x == &_y);	\
	_x < _y ? _x : _y; })

#define max(x, y) ({ 		\
	typeof(x) _x = (x);		\
	typeof(y) _y = (y);		\
	(void) (&_x == &_y);	\
	_x > _y ? _x : _y; })

/*
 * Compares two integer values
 *
 * If the first argument is larger than the second one, intcmp() returns 1.  If
 * two members are equal, returns 0.  Otherwise, returns -1.
 */
#define intcmp(x, y) \
({					\
	typeof(x) _x = (x);		\
	typeof(y) _y = (y);		\
	(void) (&_x == &_y);		\
	_x < _y ? -1 : _x > _y ? 1 : 0;	\
})

typedef void (*try_to_free_t)(size_t);
try_to_free_t set_try_to_free_routine(try_to_free_t);
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
ssize_t xread(int fd, void *buf, size_t len);
ssize_t xwrite(int fd, const void *buf, size_t len);
int eventfd_xread(int efd);
void eventfd_xwrite(int efd, int value);

/* wrapper for pthread_mutex */
#define BS_MUTEX_INITIALIZER { .mutex = PTHREAD_MUTEX_INITIALIZER }

struct bs_mutex {
	pthread_mutex_t mutex;
};

static inline void bs_init_mutex(struct bs_mutex *mutex)
{
	int ret;

	do {
		ret = pthread_mutex_init(&mutex->mutex, NULL);
	} while (ret = EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to initialize a lock, %s", strerror(ret));
}

static inline void bs_destroy_mutex(struct bs_mutex *mutex)
{
	int ret;

	do {
		ret = pthread_mutex_destroy(&mutex->mutex);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to destroy a lock, %s", strerror(ret));
}

static inline void bs_mutex_lock(struct bs_mutex *mutex)
{
	int ret;

	do {
		ret = pthread_mutext_lock(&mutex->mutex);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to lock for reading, %s", strerror(ret));
}

static inline int bs_mutex_trylock(struct bs_mutex *mutex)
{
	return pthread_mutex_trylock(&mutex->mutex);
}

static inline void bs_mutex_unlock(struct bs_mutex *mutex)
{
	int ret;

	do {
		ret = pthread_mutex_unlock(&mutex->mutex);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to unlock, %s", strerror(ret));
}

/* wrapper for pthread_cond */
#define BS_COND_INITIALIZER { .cond = PTHREAD_COND_INITIALIZER }

struct bs_cond {
	pthread_cond_t cond;
};

static inline void bs_cond_init(struct bs_cond *cond)
{
	int ret;

	do {
		ret = pthread_cond_init(&cond->cond, NULL);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to initialize a lock %s", strerror(ret));
}

static inline void bs_destroy_cond(struct bs_cond *cond)
{
	int ret;

	do {
		ret = pthread_cond_destroy(&cond->cond);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to destroy a lock %s", strerror(ret));
}

static inline int bs_cond_wait(struct bs_cond *cond, struct bs_mutex *mutex)
{
	return pthread_cond_wait(&cond->cond, &mutex->mutex);
}

static inline int bs_cond_broadcast(struct bs_cond *cond)
{
	return pthread_cond_broadcast(&cond->cond);
}

/* wrapper for pthread_rwlock */
#define BS_RW_LOCK_INITIALIZER	{ .rwlock = PTHREAD_RWLOCK_INITIALIZER }

struct bs_rw_lock {
	pthread_rwlock_t rwlock;
};

static inline void bs_init_rw_lock(struct bs_rw_lock *lock)
{
	int ret;

	do {
		ret = pthread_rwlock_init(&lock->rwlock, NULL);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to initialize a lock, %s", strerror(ret));
}

static inline void bs_destroy_rw_lock(struct bs_rw_lock *lock)
{
	int ret;

	do {
		ret = pthread_rwlock_destroy(&lock->rwlock);
	} while (ret == EAGAIN);
	
	if (unlikely(ret != 0))
		panic("failed to destroy a lock, %s", strerror(ret));
}

static inline void bs_read_lock(struct bs_rw_lock *lock)
{
	int ret;

	do {
		ret = pthread_rwlock_rdlock(&lock->rwlock);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to lock for reading, %s", strerror(ret));
}

/*
 * Even though POSIX manual it doesn't return EAGAIN, we indeed have met the
 * case that it returned EAGAIN
 */
static inline void bs_write_lock(struct bs_rw_lock *lock)
{
	int ret;

	do {
		ret = pthread_rwlock_wrlock(&lock->rwlock);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to lock for writing, %s", strerror(ret));
}

static inline void bs_rw_unlock(struct bs_rw_lock *lock)
{
	int ret;

	do {
		ret = pthread_rwlock_unlock(&lock->rwlock);
	} while (ret == EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to unlock, %s", strerror(ret));
}
#endif
