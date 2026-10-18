/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MILLET_H
#define _LINUX_MILLET_H

#include <linux/types.h>
#include <uapi/linux/millet.h>

struct task_struct;
struct cgroup_taskset;

#ifdef CONFIG_MILLET_CORE
bool millet_monitor_ready(enum MILLET_TYPE type);
#else
static inline bool millet_monitor_ready(enum MILLET_TYPE type) { return false; }
#endif

#ifdef CONFIG_MILLET_SIG
void millet_report_signal(int sig, struct task_struct *task);
#else
static inline void millet_report_signal(int sig, struct task_struct *task) { }
#endif

#ifdef CONFIG_MILLET_BINDER
extern bool millet_binder_switch;
extern unsigned long binder_warn_ahead_space;
void millet_binder_notify(enum BINDER_EXTRA_MSG event,
	struct task_struct *dst, struct task_struct *src,
	int caller_tid, bool oneway, unsigned int code);
void millet_binder_query_uid(int uid);
void millet_binder_query_stat(int uid, enum BINDER_STAT stat);
#else
static inline void millet_binder_notify(enum BINDER_EXTRA_MSG event,
	struct task_struct *dst, struct task_struct *src,
	int caller_tid, bool oneway, unsigned int code) { }
#endif

#ifdef CONFIG_MILLET_CGROUP
int millet_can_attach(struct cgroup_taskset *tset);
#endif

#endif
