/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_DEADLINE_H
#define _LINUX_SCHED_DEADLINE_H

#include <linux/sched.h>

/*
 * SCHED_DEADLINE tasks has negative priorities, reflecting
 * the fact that any of them has higher prio than RT and
 * NORMAL/BATCH tasks.
 */

#define MAX_DL_PRIO		0

static inline int dl_prio(int prio)
{
	if (unlikely(prio < MAX_DL_PRIO))
		return 1;
	return 0;
}

static inline int dl_task(struct task_struct *p)
{
	return dl_prio(p->prio);
}

static inline bool dl_time_before(u64 a, u64 b)
{
	return (s64)(a - b) < 0;
}

struct root_domain;
void dl_add_task_root_domain(struct task_struct *p);
void dl_clear_root_domain(struct root_domain *rd);
int dl_bw_alloc(int cpu, u64 bw);
void dl_bw_free(int cpu, u64 bw);

#endif /* _LINUX_SCHED_DEADLINE_H */
