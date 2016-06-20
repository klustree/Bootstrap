#ifndef __BS_TIMER_H__
#define __BS_TIMER_H__

enum timer_type {
	TIMER_PERIODIC,
	TIMER_ABSOLUTE,
	TIMER_ONESHOT
};

#define TIMER0	SIGRTMIN
#define TIMER1	SIGRTMIN + 1
#define TIMER2	SIGRTMIN + 2
#define TIMER3	SIGRTMIN + 3
#define TIMER4	SIGRTMIN + 4
#define TIMER5	SIGRTMIN + 5
#define TIMER6	SIGRTMIN + 6
#define TIMER7	SIGRTMIN + 7

struct timer {
	/* private */
	timer_t tid;
	int sfd;
	
	/* public */
	char *name;
	int signo;
	int ev_mask;
	enum timer_type type;
	void (*callback)(void *);
	void *data;
};

struct timer* create_timer(const char *name, int signo);
int add_timer(struct timer *t, enum timer_type type, 
		unsigned int msec, void (*callback)(void *), void *data);
int modify_timer(struct timer *t, enum timer_type type, unsigned int msec);
int cancle_timer(struct timer *t);
void del_timer(struct timer *t);
void set_timer_event(struct timer *t, int event);
void clear_timer_event(struct timer *t, int event);

#endif
