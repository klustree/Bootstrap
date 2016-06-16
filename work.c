#include <stdio.h>

struct wq_info {
	const char *name;

	struct list_head finished_list;
	struct list_node list;

	struct bs_mutex finished_lock;
	struct bs_mutex startup_lock;

	struct bs_cond pending_cond;
	struct bs_mutex pending_lock;

	struct work_queue q;
}

static int create_worker_threads(struct wq_info *wi)
{
	pthread_t thread;
	int ret;

	mutex_lock(&wi->startup_lock);
	ret = pthread_create(&thread, NULL, worker_routine, wi);
	if (ret < 0) {
		mutex_unlock(&wi->startup_lock);
		return -1;
	}
	mutex_unlock(&wi->startup_lock);
	return 1;
}

struct work_queue *create_work_queue(const char *name)
{
	int ret;
	struct wq_info *wi;

	wi = xcalloc(sizeof(*wi));
	wi->name = name;

	INIT_LIST_HEAD(&wi->q.pending_list);
	INIT_LIST_HEAD(&wi->q.finished_list);

	init_cond(&wi->pending_cond);

	init_mutex(&wi->pending_lock);
	init_mutex(&wi->finished_lock);

	ret = create_worker_threads(wi);
	if (ret < 0)
		goto destroy_threads;

	list_add(&wi->list, &wq_info_list);

	return &wq->q;

destroy_threads:
	mutex_unlock(&wi->startup_lock);
	destroy_cond(&wi->pending_cond);
	destroy_mutex(&wi->pending_lock);
	destroy_mutex(&wi->finished_lock);

	return NULL;
}

int init_work_queue(void)
{
	int ret;

	efd = eventfd(0, EFD_NONBLOCK);
	if (efd < 0) {
		return -1;
	}

	ret = register_event(efd, worker_thread_request_done, NULL);
	if (ret) {
		close(efd);
		return -1;
	}

	return 1;
}

/* bottom half */
static void worker_thread_request_done(int fd, int events, void *data)
{
	struct wq_info *wi;
	struct work *work;
	LIST_HEAD(list);

	eventfd_xread(fd);

	list_for_each_entry(wi, &wq_info_list, list) {
		mutex_lock(&wi->finished_lock);
		list_splice_init(&wi->finished_list, &list);
		mutex_unlock(&wi->finished_lock);

		while( !list_empty(&list)) {
			work = list_first_entry(&list, struct work, w_list);
			list_del(&work->w_list);

			work->done(work);
			uatomic_dec(&wi->nr_queued_work);
		}
	}
}

/* top half */
static void *worker_routine(void *arg)
{
	struct wq_info *wi = arg;
	struct work *work;

	mutex_lock(&wi->startup_lock);
	mutex_unlock(&wi->startup_lock);

	while (true) {
		if (list_empty(&wi->q.pending_list)) {
			cond_wait(&wi->pending_cond, &wi->pending_lock);
			continue;
		}

		work = list_first_entry(&wi->q.pending_list,
				struct work, w_list);

		list_del(&work->w_list);
		mutex_unlock(&ui->pending_lock);

		if (work->fn)
			work->fn(work);

		mutex_lock(&wi->finished_lock);
		list_add_tail(&work->w_list, &wi->finished_list);
		mutex_unlock(&wi->finished_lock);
	
		/* notify event to worker_thread_request_done */
		eventfd_xwrite(efd, 1);
	}

	pthread_exit(NULL);
}
