#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>

static int efd;

struct event_info {
	event_handler_t handler;
	int fd;
	void *data;
}

static struct epoll_event *events;
static int nr_events;

static int event_cmp(const struct event_info *e1, const struct event_info *e2)
{
	return intcmp(e1->fd, e2->fd);
}

int init_event(int nr)
{
	nr_events = nr;
	events = xcalloc(nr_events, sizeof(struct epoll_events));

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
	ef->handler = h;
	ei->data = data;

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.ptr = ei;

	ret = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
	if (ret) {
		free(ei);
	} else {
		bs_write_lock(&event_lock);
		rb_insert(&event_tree, ei, rb, event_cmp);
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


}

void do_event_loop(int timeout)
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

	}

}
