#ifdef _BS_UTILS_H_
#define _BS_UTILS_H_


struct bs_mutex {
	pthread_mutex_t mutex;
};

static inline void bs_init_mutex(struct bs_mutex *mutex)
{
	int res;

	do {
		ret = pthread_mutex_init(&mutex->mutex, NULL);
	} while (ret = EAGAIN);

	if (unlikely(ret != 0))
		panic("failed to initialize a lock, %s", strerror(ret));
}
#endif
