#ifndef __QUEUE_H_
#define __QUEUE_H_

#include "list.h"

struct queue {
	struct bs_cond pending_cond;
	struct bs_mutex pending_lock;
	struct list_head pending_list;

	int efd;
	void (*notify_callback)(void *);
	void *data;
	int limit;
	int count;
};

struct element {
	struct list_node e_list;
	void *data;
};

struct queue* q_init(unsigned int limit,
		void (*notify_callback)(void *), void *data);
bool q_empty(struct queue *q);
void q_add(struct queue *q, struct element *elem);
struct element* q_first_entry(struct queue *q);
void q_del(struct element *elem);
void q_notify(struct queue *q, int count);

#endif
