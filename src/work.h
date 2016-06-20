#ifndef __WORK_H__
#define __WORK_H__

#include <stdbool.h>

#include "list.h"
#include "util.h"

struct work;

typedef void (*work_func_t)(struct work *);

struct work {
	struct list_node w_list;
	work_func_t fn;
	work_func_t done;
};

struct work_queue {
	int wq_state;
	struct list_head pending_list;
};

static inline bool is_main_thread(void)
{
	return gettid() == getpid();
}

static inline bool is_worker_thread(void)
{
	return !is_main_thread();
}

int init_work_queue(void);
struct work_queue *create_work_queue(const char *name);
void queue_work(struct work_queue *q, struct work *work);
void queue_work_first_entry(struct work_queue *q, struct work *work);
#define emerge_work		queue_work_first_entry
bool work_queue_empty(struct work_queue *q);

#endif
