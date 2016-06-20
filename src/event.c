#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>

#include "rbtree.h"
#include "util.h"
#include "event.h"

static int efd;
static struct rb_root events_tree = RB_ROOT;
static struct bs_rw_lock events_lock = BS_RW_LOCK_INITIALIZER;

struct event_info {
	event_handler_t handler;
	int fd;
	void *data;
	struct rb_node rb;
};

static struct epoll_event *events;
static int nr_events;

static int event_cmp(const struct event_info *e1, const struct event_info *e2)
{
	return intcmp(e1->fd, e2->fd);
}

int init_event(int nr)
{
	nr_events = nr;
	events = (struct epoll_event *)xcalloc(nr_events, sizeof(struct epoll_event));

	efd = epoll_create(nr);
	if (efd < 0) {
		return -1;
	}

	return 0;
}

static struct event_info* lookup_event(int fd)
{
	struct event_info key = { .fd = fd };
	struct event_info *ret;

	bs_read_lock(&events_lock);
	ret = rb_search(&events_tree, &key, rb, event_cmp);
	bs_rw_unlock(&events_lock);

	return ret;
}

int register_event(int fd, event_handler_t h, void *data)
{
	int ret;
	struct epoll_event ev;
	struct event_info *ei;

	ei = xcalloc(1, sizeof(*ei));
	ei->fd = fd;
	ei->handler = h;
	ei->data = data;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.ptr = ei;

	ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
	if (ret) {
		free(ei);
	} else {
		bs_write_lock(&events_lock);
		rb_insert(&events_tree, ei, rb, event_cmp);
		bs_rw_unlock(&events_lock);
	}

	return ret;
}

void unregister_event(int fd)
{
	int ret;
	struct event_info *ei;

	ei = lookup_event(fd);
	if (!ei)
		return;

	ret = epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
	if (ret)
		printf("failed to delete epoll event for fd %d", fd);

	bs_write_lock(&events_lock);
	rb_erase(&ei->rb, &events_tree);
	bs_rw_unlock(&events_lock);
	free(ei);

	/*
	 * Although ei is no longer valid pointer, ei->handler() might be about
	 * to be called in do_event_loop().  Refreshing the event loop is safe.
	 */
	event_force_refresh();
}

int modify_event(int fd, unsigned int new_events)
{
	int ret;
	struct epoll_event ev;
	struct event_info *ei;

	ei = lookup_event(fd);
	if (!ei) {
		bs_debug("event info for fd %d not found", fd);
		return -1;
	}

	memset(&ev, 0, sizeof(ev));
	ev.events = new_events;
	ev.data.ptr = ei;

	ret = epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
	if (ret) {
		bs_debug("failed to delete epoll event for fd %d: %m", fd);
		return -1;
	}
	return 0;
}

static bool event_loop_refresh;

void event_force_refresh(void)
{
	event_loop_refresh = true;
}

void event_loop(int timeout)
{
	int i, nr;

refresh:
	event_loop_refresh = false;
	nr = epoll_wait(efd, events, nr_events, timeout);
	if (nr < 0) {
		if (errno == EINTR)
			return;
		fprintf(stderr, "epoll_wait failed: %m");
		exit(1);
	} else if (nr) {
		for (i = 0; i < nr; i++) {
			struct event_info *ei;

			ei = (struct event_info *)events[i].data.ptr;
			ei->handler(ei->fd, events[i].events, ei->data);

			if (event_loop_refresh)
				goto refresh;
		}
	}

}
