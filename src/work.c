/*
 * Copyright (C) 2015 Yoonki Kim <klustree@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * This code is based on bs.c from Linux target framework (tgt):
 *   Copyright (C) 2007 FUJITA Tomonori <tomof@acm.org>
 *   Copyright (C) 2007 Mike Christie <michaelc@cs.wisc.edu>
 *   Copyright (C) 2009-2011 Nippon Telegraph and Telephone Corporation.
 */
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/types.h>
#include <signal.h>

#include "list.h"
#include "util.h"
//#include "bitops.h"
#include "work.h"
#include "event.h"

/*
 * The protection period from shrinking work queue.  This is necessary
 * to avoid many calls of pthread_create.  Without it, threads are
 * frequently created and deleted and it leads poor performance.
 */
#define WQ_PROTECTION_PERIOD 1000 /* ms */

struct wq_info {
	const char *name;

	struct list_head finished_list;
	struct list_node list;

	struct bs_mutex finished_lock;
	struct bs_mutex startup_lock;

	/* wokers sleep on this and signaled by work producer */
	struct bs_cond pending_cond;
	/* locked by work producer and workers */
	struct bs_mutex pending_lock;
	/* protected by pending_lock */
	struct work_queue q;

	/* protected by uatomic primitives */
	size_t nr_queued_work;

	uint64_t tm_end_of_protection;
};

static int efd;
static LIST_HEAD(wq_info_list);

static void *worker_routine(void *arg);

static uint64_t get_msec_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static bool wq_destroy(struct wq_info *wi)
{

}

static int create_worker_threads(struct wq_info *wi)
{
	pthread_t thread;
	int ret;

	mutex_lock(&wi->startup_lock);
	ret = pthread_create(&thread, NULL, worker_routine, wi);
	if (ret != 0) {
		sd_err("failed to create worket therad: %m");
		mutex_unlock(&wi->startup_lock);
		return -1;
	}
	sd_debug("create thread %s", wi->name);
	mutex_unlock(&wi->startup_lock);

	return 0;
}

void queue_work(struct work_queue *q, struct work *work)
{
	struct wq_info *wi = container_of(q, struct wq_info, q);

	uatomic_inc(&wi->nr_queued_work);
	bs_mutex_lock(&wi->pending_lock);

	list_add_tail(&work->w_list, &wi->q.pending_list);
	bs_mutex_unlock(&wi->pending_lock);

	bs_cond_signal(&wi->pending_cond);
}

static void worker_thread_request_done(int fd, int events, void *data)
{
	struct wq_info *wi;
	struct work *work;
	LIST_HEAD(list);

	eventfd_xread(fd);

	list_for_each_entry(wi, &wq_info_list, list) {
		bs_mutex_lock(&wi->finished_lock);
		list_splice_init(&wi->finished_list, &list);
		bs_mutex_unlock(&wi->finished_lock);

		while(!list_empty(&list)) {
			work = list_first_entry(&list, struct work, w_list);
			list_del(&work->w_list);

			work->done(work);

			uatomic_dec(&wi->nr_queued_work);
		}
	}
}

static void *worker_routine(void *arg)
{
	struct wq_info *wi = arg;
	struct work *work;

	bs_mutex_lock(&wi->startup_lock);
	/* started this thread */
	bs_mutex_unlock(&wi->startup_lock);

	while (true) {

		bs_mutex_lock(&wi->pending_lock);
		if (list_empty(&wi->q.pending_list)) {
			bs_cond_wait(&wi->pending_cond, &wi->pending_lock);
			continue;
		}

		work = list_first_entry(&wi->q.pending_list,
				struct work, w_list);

		list_del(&work->w_list);
		bs_mutex_unlock(&wi->pending_lock);

		if (work->fn)
			work->fn(work);

		bs_mutex_lock(&wi->finished_lock);
		list_add_tail(&work->w_list, &wi->finished_list);
		bs_mutex_unlock(&wi->finished_lock);
	
		/* notify event to worker_thread_request_done */
		eventfd_xwrite(efd, 1);
	}

	pthread_exit(NULL);
}

int init_work_queue(void)
{
	int ret;

	efd = eventfd(0, EFD_NONBLOCK);
	if (efd < 0) {
		bs_err("failed to create event fd: %m");
		return -1;
	}

	ret = register_event(efd, worker_thread_request_done, NULL);
	if (ret) {
		bs_err("failed to register event fd %m");
		close(efd);
		return -1;
	}

	return 0;
}

struct work_queue *create_work_queue(const char *name)
{
	int ret;
	struct wq_info *wi;

	wi = xcalloc(1, sizeof(*wi));
	wi->name = name;

	INIT_LIST_HEAD(&wi->q.pending_list);
	INIT_LIST_HEAD(&wi->finished_list);

	bs_init_cond(&wi->pending_cond);

	bs_init_mutex(&wi->pending_lock);
	bs_init_mutex(&wi->finished_lock);

	ret = create_worker_threads(wi);
	if (ret < 0)
		goto destroy_threads;

	list_add(&wi->list, &wq_info_list);

	return &wi->q;

destroy_threads:
	bs_mutex_unlock(&wi->startup_lock);
	destroy_cond(&wi->pending_cond);
	destroy_mutex(&wi->pending_lock);
	destroy_mutex(&wi->finished_lock);
	free(wi);

	return NULL;
}

bool work_queue_empty(struct work_queue *q)
{
	struct wq_info *wi = container_of(q, struct wq_info, q);

	return uatomic_read(&wi->nr_queued_work) == 0;
}
