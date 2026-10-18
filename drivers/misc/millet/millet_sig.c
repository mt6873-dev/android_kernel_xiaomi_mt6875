// MIUI ADD: Power_ProcessFreeze
#define pr_fmt(fmt) "millet_millet-sig: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/signal.h>
#include "millet.h"

static int last_report_task;
extern int millet_sendmsg(enum MILLET_TYPE type, struct task_struct *t,
		struct millet_data *data);
extern int millet_sendto_user(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk);
extern int register_millet_hook(int type, recv_hook recv_from, send_hook send_to,
		init_hook init);
extern int init_millet_subsystem(int type);

void millet_report_signal(int sig, struct task_struct *dst)
{
	struct millet_data data = { 0 };

	if (!dst || !millet_monitor_ready(SIG_TYPE))
		return;

	if (sig == SIGKILL
			|| sig == SIGTERM
			|| sig == SIGABRT
			|| sig == SIGQUIT) {

		data.mod.k_priv.sig.caller_task = current;
		data.mod.k_priv.sig.killed_task = dst;
		data.pri[0]= sig;
		data.mod.k_priv.sig.reason = KILLED_BY_PRO;
		millet_sendmsg(SIG_TYPE, dst, &data);
	}
}

static int signals_sendmsg(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk)
{
	int ret = 0;

	if (!sk || !data || !tsk) {
		pr_err("%s input invalid\n", __FUNCTION__);
		return RET_ERR;
	}

	data->mod.k_priv.sig.killed_pid = task_tgid_nr(tsk);
	data->uid = task_uid(tsk).val;
	data->msg_type = MSG_TO_USER;
	data->owner = SIG_TYPE;

	if (frozen_task_group(tsk)
		&& (data->mod.k_priv.sig.killed_pid != *(int *)sk->mod[SIG_TYPE].priv)) {
		*(int *)sk->mod[SIG_TYPE].priv = data->mod.k_priv.sig.killed_pid;
		ret = millet_sendto_user(tsk, data, sk);
	}

	return ret;
}

static void signas_init_millet(struct millet_sock *sk)
{
	if (sk) {
		sk->mod[SIG_TYPE].monitor = SIG_TYPE;
		sk->mod[SIG_TYPE].priv = (void *)&last_report_task;
		atomic_set(&sk->mod[SIG_TYPE].has_init, 1);
	}
}

static int __init sig_mod_init(void)
{
	register_millet_hook(SIG_TYPE, NULL,
		signals_sendmsg, signas_init_millet);
	init_millet_subsystem(SIG_TYPE);

	return 0;
}

module_init(sig_mod_init);
MODULE_LICENSE("GPL");
// END Power_ProcessFreeze
