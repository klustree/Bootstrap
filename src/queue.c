/*
 * Copyright (C) 2015 Yoonki Kim <klustree@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>

#include "util.h"
#include "event.h"
#include "queue.h"

static void q_handler(int fd, int events, void *data)
{
	struct queue *q = data;
	int ret;

	ret = eventfd_xread(fd);

	q->notify_callback(q->data);
	
}

struct queue* q_init(unsigned int limit,
		void (*notify_callback)(void *), void *data)
{
	struct queue *q;

	q = xcalloc(1, sizeof(struct queue));
	INIT_LIST_HEAD(&q->pending_list);
	
	bs_init_mutex(&q->pending_lock);
	bs_cond_init(&q->pending_cond);

	q->notify_callback = notify_callback;
	q->data = data;
	q->limit = limit;

	if (notify_callback) {
		q->efd = eventfd(0, 0);
		if (q->efd == -1) {
			abort();
		}

		if (register_event(q->efd, q_handler, q) < 0) {
			abort();
		}
	}

	return q;
}

bool q_empty(struct queue *q)
{
	return list_empty(&q->pending_list);
}

void q_add(struct queue *q, struct element *elem)
{
	list_add_tail(&elem->e_list, &q->pending_list);
}

struct element* q_first_entry(struct queue *q)
{
	struct element *element;

	element = list_first_entry(&q->pending_list, struct element, e_list);
	return element;
}

void q_del(struct element *elem)
{
	list_del(&elem->e_list);
}

void q_notify(struct queue *q, int count)
{
	eventfd_xwrite(q->efd, count);
}

int q_notify_off(struct queue *q)
{
	modify_event(q->efd, ~EPOLLIN);
}

int q_notify_on(struct queue *q)
{
	modify_event(q->efd, EPOLLIN);
}
