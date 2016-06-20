#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include "util.h"
#include "event.h"
#include "timer.h"

static void timer_handler(int fd, int events, void *data)
{
	struct signalfd_siginfo fdsi;
	struct timer *t = data;

	if (read(fd, &fdsi, sizeof(struct signalfd_siginfo)) < 0)
		return;

	t->callback(t->data);
	
	switch(t->type) {
		case TIMER_PERIODIC:
		case TIMER_ABSOLUTE:
			break;
		case TIMER_ONESHOT:
			unregister_event(fd);
			close(fd);
			break;
		default:
			break;
	}

}

struct timer* create_timer(const char *name, int signo)
{
	struct sigevent sev;
	struct timer *t;
	
	t = xcalloc(1, sizeof(*t));
	t->name = name;
	
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = t->signo = signo;
	
	if (timer_create(CLOCK_REALTIME, &sev, &t->tid) < 0) {
		panic("failed to create timer");
		free(t);
		return NULL;
	}

	return t;
}

int add_timer(struct timer *t, enum timer_type type, 
		unsigned int msec, void (*callback)(void *), void *data)
{
	struct itimerspec it;
	sigset_t mask;
	int sfd;

	t->type = type;
	t->callback = callback;
	t->data = data;

	memset(&it, 0, sizeof(it));

	/* Inhibit default SIGRTMIN handling */
	sigemptyset(&mask);
	sigaddset(&mask, t->signo);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		bs_debug("failed to sigprocmask: %m");
		return -1;
	}

	sfd = signalfd(-1, &mask, 0);
	if (sfd < 0) {
		bs_debug("failed to create signal fd");
		return -1;
	}
	
	switch (type) {
		case TIMER_PERIODIC:
			it.it_value.tv_sec = msec / 1000;
			it.it_value.tv_nsec = (msec % 1000) * 1000000;
			it.it_interval.tv_sec = msec / 1000;
			it.it_interval.tv_nsec = (msec % 1000) * 1000000;
			break;
		case TIMER_ABSOLUTE:
		case TIMER_ONESHOT:
			it.it_value.tv_sec = msec / 1000;
			it.it_value.tv_nsec = (msec % 1000) * 1000000;
			break;
		default:
			return -1;
	}

	if (timer_settime(t->tid, 0, &it, NULL) < 0) {
		bs_debug("timer_settime failed: %m");
		return -1;
	}
	
	if (register_event(sfd, timer_handler, t) < 0) {
		bs_debug("register_event failed");
		return -1;
	}

	return 0;
}

int cancle_timer(struct timer *t)
{
	struct itimerspec it;
	
	it.it_value.tv_sec = 0;
	it.it_value.tv_nsec = 0;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_nsec = 0;

	if (timer_settime(t->tid, 0, &it, NULL) < 0) {
		bs_debug("timer_settime failed: %m");
		return -1;
	}

	return 0;
}

int modify_timer(struct timer *t, enum timer_type type, unsigned int msec)
{
	struct itimerspec it;

	t->type = type;
	
	switch (type) {
		case TIMER_PERIODIC:
			it.it_value.tv_sec = msec / 1000;
			it.it_value.tv_nsec = (msec % 1000) * 1000000;
			it.it_interval.tv_sec = msec / 1000;
			it.it_interval.tv_nsec = (msec % 1000) * 1000000;
			break;
		case TIMER_ABSOLUTE:
		case TIMER_ONESHOT:
			it.it_value.tv_sec = msec / 1000;
			it.it_value.tv_nsec = (msec % 1000) * 1000000;
			break;
		default:
			return -1;
	}

	if (timer_settime(t->tid, 0, &it, NULL) < 0) {
		bs_debug("timer_settime failed: %m");
		return -1;
	}

	return 0;
}

void del_timer(struct timer *t)
{
	timer_delete(t->tid);
	unregister_event(t->sfd);
	close(t->sfd);
	free(t);
}

void set_timer_event(struct timer *t, int event)
{
	// FIXME : add atomic or lock 
	t->ev_mask |= event;
}

void clear_timer_event(struct timer *t, int event)
{
	// FIXME : add atomic or lock 
	t->ev_mask &= ~event;
}
