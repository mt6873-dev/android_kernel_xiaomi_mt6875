// MIUI ADD: Power_ProcessFreeze
#ifndef MILLET_H
#define MILLET_H

#include <linux/freezer.h>
#include <net/sock.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/millet.h>
#include <linux/cgroup.h>

#define RET_OK 0
#define RET_ERR -1
#define WARN_AHEAD_MSGS		3
#define RESERVE_ORDER		17
#define WARN_AHEAD_SPACE	(1 << RESERVE_ORDER)
#define CHECK_BUFFER_DETAIL_SAPCE   (1 << 16)
#define LARGE_BUFFER_THRESHOLD      (1 << 15)
#define BUFFER_CHECK_INTERVAL_MS    5000

struct millet_sock;
typedef void (*recv_hook)(void *data, unsigned int len);
typedef int (*send_hook)(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk);
typedef void (*init_hook)(struct millet_sock *sk);

struct millet_sock {
	struct sock *sock;

	struct mod_info {
		int id;
		enum MILLET_TYPE monitor;
		atomic_t port;
		atomic_t loop_init;
		recv_hook recv_from;
		send_hook send_to;
		init_hook init;
		spinlock_t lock;
		struct {
			u64 send_suc;
			u64 send_fail;
			u64 runtime;
		} stat;
		char name[NAME_MAXLEN];
		void *priv;
		atomic_t has_init;
	} mod[MILLET_TYPES_NUM];
};

static inline int frozen_task_group(struct task_struct *task)
{
	return !(task->flags & PF_KTHREAD) &&
		(freezing(task) || frozen(task) || cgroup_task_freeze(task) ||
		 cgroup_task_frozen(task));
}

bool millet_monitor_ready(enum MILLET_TYPE type);
#endif
// END Power_ProcessFreeze
