// SPDX-License-Identifier: GPL-2.0
/* Xiaomi dijun-v-oss Millet freezer policy, using the native v1 callback. */
#include <linux/capability.h>
#include <linux/cgroup.h>
#include <linux/cred.h>
#include <linux/millet.h>

int millet_can_attach(struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct task_struct *task;

	if (__kuid_val(current_euid()) == 1000 || capable(CAP_SYS_ADMIN))
		return 0;

	cgroup_taskset_for_each(task, css, tset) {
		if (task != current)
			return -EACCES;
	}
	return 0;
}
