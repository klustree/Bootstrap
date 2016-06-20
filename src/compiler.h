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

#ifndef _BS_COMPILER_H_
#define _BS_COMPILER_H_

#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>

#include "config.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define __LOCAL(var, line) __ ## var ## line
#define _LOCAL(var, line) __LOCAL(var, line)
#define LOCAL(var) _LOCAL(var, __LINE__)

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)


#endif	/* SD_COMPILER_H */
