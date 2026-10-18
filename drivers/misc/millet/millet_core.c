// MIUI ADD: Power_ProcessFreeze
/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 *
 * File name: millet.c
 * Description: smart frozen control
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2019/11/27
 */

#define pr_fmt(fmt) "millet_millet-core: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/freezer.h>
#include <net/sock.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "millet.h"

int frozen_uid_min = 10000;
EXPORT_SYMBOL_GPL(frozen_uid_min);
unsigned long binder_warn_ahead_space = WARN_AHEAD_SPACE;
EXPORT_SYMBOL_GPL(binder_warn_ahead_space);
static struct millet_sock millet_sk;
static bool millet_ready;
struct proc_dir_entry *millet_rootdir;
unsigned int millet_debug = 0;
EXPORT_SYMBOL_GPL(millet_debug);
bool millet_binder_switch = true;
EXPORT_SYMBOL_GPL(millet_binder_switch);
static int millet_freeze_switch;
module_param(millet_debug, uint, 0644);
module_param(frozen_uid_min, int, 0644);
module_param(binder_warn_ahead_space, ulong, 0644);
module_param(millet_freeze_switch, int, 0660);
module_param(millet_binder_switch, bool, 0644);
enum MILLET_VERSION millet_v= VERSION_1_0;

static void dump_send_msg(struct millet_data *msg)
{
	if (!millet_debug)
		return;

	pr_info("-----up-report msg-head dump-----\n");
	if (!msg) {
		pr_err("msg is NULL");
		return;
	}

	pr_info("up msg: %d\n", msg->msg_type);
	pr_info("up type: %d\n", msg->owner);
	pr_info("up src_port: 0x%lu\n", msg->src_port);
	pr_info("up dest_port: 0x%lu\n", msg->dst_port);
	pr_info("up uid: %d\n", msg->uid);
}

static void dump_recv_msg(struct millet_userconf *msg)
{
	if (!millet_debug)
		return;

	pr_info("-----recv msg from user-----\n");
	if (!msg) {
		pr_err("msg is NULL");
		return;
	}

	pr_info("msg: %d\n", msg->msg_type);
	pr_info("type: %d\n", msg->owner);
	pr_info("src_port: 0x%lu\n", msg->src_port);
	pr_info("dest_port: 0x%lu\n", msg->dst_port);
}

bool judge_millet_freeze_switch(void)
{
	return 1 == millet_freeze_switch;
}


bool millet_monitor_ready(enum MILLET_TYPE type)
{
	int monitor;

	if (!READ_ONCE(millet_ready) || !TYPE_VALID(type) ||
	    !atomic_read(&millet_sk.mod[type].has_init))
		return false;
	monitor = millet_sk.mod[type].monitor;
	if (!TYPE_VALID(monitor))
		monitor = type;
	return atomic_read(&millet_sk.mod[monitor].has_init) &&
		atomic_read(&millet_sk.mod[monitor].loop_init);
}

int millet_sendto_user(struct task_struct *tsk,
		struct millet_data *data, struct millet_sock *sk)
{
	int ret, msg_len = 0;
	int monitor, monitor_port = 0;
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	struct millet_data *payload = NULL;
	struct timespec64 ts;

	if (!sk || !data || !MSG_VALID(data->msg_type) ||
	    !TYPE_VALID(data->owner))
		return -EINVAL;
	if (!millet_monitor_ready(data->owner))
		return -ENOTCONN;

	msg_len = sizeof(struct millet_data);
	skb = nlmsg_new(msg_len, GFP_ATOMIC);
	if (!skb) {
		pr_err("%s alloc_skb failed! %d\n",
		       __func__, data->owner);
		return RET_ERR;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, msg_len, 0);
	if (!nlh) {
		kfree_skb(skb);
		return RET_ERR;
	}

	payload = nlmsg_data(nlh);
	memset(payload, 0, msg_len);
	payload->owner = data->owner;
	payload->msg_type = data->msg_type;
	payload->uid = data->uid;
	memcpy(payload->pri, data->pri, sizeof(payload->pri));
	/* These are kernel-private bookkeeping slots, never userspace pointers. */
	switch (data->owner) {
	case SIG_TYPE:
		payload->mod.k_priv.sig.killed_pid = data->mod.k_priv.sig.killed_pid;
		payload->mod.k_priv.sig.reason = data->mod.k_priv.sig.reason;
		break;
	case BINDER_TYPE:
		payload->mod.k_priv.binder.trans.caller_uid = data->mod.k_priv.binder.trans.caller_uid;
		payload->mod.k_priv.binder.trans.caller_pid = data->mod.k_priv.binder.trans.caller_pid;
		payload->mod.k_priv.binder.trans.caller_tid = data->mod.k_priv.binder.trans.caller_tid;
		payload->mod.k_priv.binder.trans.dst_pid = data->mod.k_priv.binder.trans.dst_pid;
		payload->mod.k_priv.binder.trans.tf_oneway = data->mod.k_priv.binder.trans.tf_oneway;
		payload->mod.k_priv.binder.trans.code = data->mod.k_priv.binder.trans.code;
		break;
	case BINDER_ST_TYPE:
		payload->mod.k_priv.binder.stat.pid = data->mod.k_priv.binder.stat.pid;
		payload->mod.k_priv.binder.stat.tid = data->mod.k_priv.binder.stat.tid;
		payload->mod.k_priv.binder.stat.reason = data->mod.k_priv.binder.stat.reason;
		break;
	case PKG_TYPE:
		payload->mod.k_priv.pkg.pkg_owner = data->mod.k_priv.pkg.pkg_owner;
		payload->mod.k_priv.pkg.owner_pid = data->mod.k_priv.pkg.owner_pid;
		break;
	default:
		break;
	}
	payload->src_port = MILLET_KERNEL_ID;
	payload->dst_port = MILLET_USER_ID;
	monitor = sk->mod[payload->owner].monitor;
	if (!TYPE_VALID(monitor))
		monitor = payload->owner;

	if (!atomic_read(&sk->mod[monitor].has_init)) {
		kfree_skb(skb);
		return RET_ERR;
	}

	if (!atomic_read(&sk->mod[monitor].loop_init)) {
		if (unlikely(millet_debug))
			pr_err("monitor %d no hand shake!!", monitor);
		kfree_skb(skb);
		return RET_ERR;
	}
	payload->monitor = monitor;
	ktime_get_ts64(&ts);
	payload->tm.sec  = ts.tv_sec;
	payload->tm.nsec = ts.tv_nsec;
	monitor_port = atomic_read(&sk->mod[monitor].port);
	dump_send_msg(payload);
	ret = nlmsg_unicast(sk->sock, skb, monitor_port);

	if (ret < 0) {
		if (millet_debug)
			pr_err("nlmsg_unicast failed! %s errno %d\n",
				__func__, ret);
		return RET_ERR;
	} else {
		if (millet_debug)
			pr_info("nlmsg_unicast snd msg success to %d\n",
					monitor_port);
	}

	return RET_OK;
}
EXPORT_SYMBOL_GPL(millet_sendto_user);


int millet_sendmsg(enum MILLET_TYPE type, struct task_struct *tsk,
		struct millet_data *data)
{
	u64 walltime, timecost;
	unsigned long flags;
	int ret = RET_OK;

	if (!TYPE_VALID(type) || !data || !tsk)
		return -EINVAL;
	if (!millet_monitor_ready(type))
		return -ENOTCONN;

	if (!millet_sk.mod[type].send_to) {
		pr_err("mod %d send_to interface is NULL\n", type);
		return RET_ERR;
	}

	walltime = ktime_to_us(ktime_get());
	ret = millet_sk.mod[type].send_to(tsk, data, &millet_sk);

	if (!atomic_read(&millet_sk.mod[type].has_init))
                return ret;

	spin_lock_irqsave(&millet_sk.mod[type].lock, flags);

	if (ret < 0) {
		millet_sk.mod[type].stat.send_fail++;
	} else if (ret == RET_OK)
		millet_sk.mod[type].stat.send_suc++;

	timecost = ktime_to_us(ktime_get()) - walltime;
	millet_sk.mod[type].stat.runtime += timecost;
	spin_unlock_irqrestore(&millet_sk.mod[type].lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(millet_sendmsg);


static void recv_handler(struct sk_buff *skb)
{
	struct millet_userconf request;
	struct nlmsghdr *nlh;
	int from;

	if (!skb || __kuid_val(NETLINK_CREDS(skb)->uid) > 1000 ||
	    skb->len < NLMSG_HDRLEN)
		return;
	nlh = nlmsg_hdr(skb);
	if (!nlmsg_ok(nlh, skb->len) ||
	    nlmsg_len(nlh) < sizeof(request))
		return;
	memcpy(&request, nlmsg_data(nlh), sizeof(request));
	if (request.src_port != MILLET_USER_ID ||
	    request.dst_port != MILLET_KERNEL_ID ||
	    !TYPE_VALID(request.owner) ||
	    !atomic_read(&millet_sk.mod[request.owner].has_init))
		return;

	/* Use the authenticated sender, not the user-supplied nlmsg_pid. */
	from = NETLINK_CB(skb).portid;
	dump_recv_msg(&request);
	switch (request.msg_type) {
	case LOOPBACK_MSG: {
		struct millet_data data = { 0 };

		data.msg_type = LOOPBACK_MSG;
		data.owner = request.owner;
		atomic_set(&millet_sk.mod[request.owner].port, from);
		atomic_set(&millet_sk.mod[request.owner].loop_init, 1);
		millet_sendto_user(current, &data, &millet_sk);
		break;
	}
	case MSG_TO_KERN:
		if (millet_sk.mod[request.owner].recv_from)
			millet_sk.mod[request.owner].recv_from(&request,
							    sizeof(request));
		break;
	default:
		break;
	}
}

static int millet_sock_show(struct seq_file *m, void *v)
{
	int i;
	unsigned long flags;
	u64 send_suc[MILLET_TYPES_NUM];
	u64 send_fail[MILLET_TYPES_NUM];
	u64 runtime[MILLET_TYPES_NUM];
	u64 total_send_suc, total_send_fail, total_runtime;

	memset(send_suc, 0, sizeof(u64) * MILLET_TYPES_NUM);
	memset(send_fail, 0, sizeof(u64) * MILLET_TYPES_NUM);
	memset(runtime, 0, sizeof(u64) * MILLET_TYPES_NUM);

	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		spin_lock_irqsave(&millet_sk.mod[i].lock, flags);
		send_suc[i] = millet_sk.mod[i].stat.send_suc;
		send_fail[i] = millet_sk.mod[i].stat.send_fail;
		runtime[i] = millet_sk.mod[i].stat.runtime;
		spin_unlock_irqrestore(&millet_sk.mod[i].lock, flags);
	}

	total_send_suc = total_send_fail = total_runtime = 0;
	seq_printf(m, "-----------------------------\n\n");
	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		if (!send_suc[i] && !send_fail[i])
			continue;

		total_send_suc += send_suc[i];
		total_send_fail += send_fail[i];
		total_runtime += runtime[i];

		seq_printf(m, "name: %s mod id %d:\n",
		           millet_sk.mod[i].name, i);
		seq_printf(m, "send suc: %llu\n", send_suc[i]);
		seq_printf(m, "send fail: %llu\n", send_fail[i]);
		seq_printf(m, "runtime: %llu us\n\n", runtime[i]);

	}

	seq_printf(m, "---------total info--------------------\n");
	seq_printf(m, "send_suc: %llu\n", total_send_suc);
	seq_printf(m, "send_fail: %llu\n", total_send_fail);
	seq_printf(m, "runtime: %llu us\n", total_runtime);
	return 0;
}

static int millet_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, millet_sock_show, NULL);
}

static void stat_reset(void)
{
	int i;
	unsigned long flags;

	for (i = O_TYPE + 1; i < MILLET_TYPES_NUM; i++) {
		spin_lock_irqsave(&millet_sk.mod[i].lock, flags);
		millet_sk.mod[i].stat.send_suc = 0;
		millet_sk.mod[i].stat.send_fail = 0;
		millet_sk.mod[i].stat.runtime = 0;
		spin_unlock_irqrestore(&millet_sk.mod[i].lock, flags);
	}
}

static ssize_t millet_stat_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	unsigned int value;
	int ret;

	ret = kstrtouint_from_user(buf, count, 10, &value);
	if (ret)
		return ret;
	if (value != 1)
		return -EINVAL;
	stat_reset();
	return count;
}

static const struct file_operations millet_proc_fops = {
	.open   = millet_stat_open,
	.read   = seq_read,
	.write   = millet_stat_write,
	.llseek   = seq_lseek,
	.release   = single_release,
};

static int millet_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", millet_v);
	return 0;
}

static int millet_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, millet_version_show, NULL);
}

static const struct file_operations millet_version_fops = {
	.open   = millet_version_open,
	.read   = seq_read,
	.llseek   = seq_lseek,
	.release   = single_release,
};

int register_millet_hook(int type, recv_hook recv_from,
		send_hook send_to, init_hook init)
{
	if (!TYPE_VALID(type)) {
		pr_err("%s: type is invalid! %d\n",
				__func__, type);
		return RET_ERR;
	}

	if (millet_sk.mod[type].init) {
		pr_err("%s: init has been registered %d\n",
		       __func__, type);
		return RET_ERR;
	} else
		millet_sk.mod[type].init = init;

	if (millet_sk.mod[type].recv_from) {
		pr_err("%s: recv_from has been registered %d\n",
		       __func__, type);
		return RET_ERR;
	} else
		millet_sk.mod[type].recv_from = recv_from;

	if (millet_sk.mod[type].send_to) {
		pr_err("%s: send_to has been registered %d\n",
		       __func__, type);
		return RET_ERR;
	} else
		millet_sk.mod[type].send_to = send_to;

	if (millet_debug) {
		printk("type %d register hook\n", type);
		dump_stack();
	}

	return RET_OK;
}
EXPORT_SYMBOL_GPL(register_millet_hook);


int unregister_millet_hook(int type)
{
	if (!TYPE_VALID(type)) {
		pr_err("%s: type is invalid! %d\n",
		       __func__, type);
		return RET_ERR;
	}

	millet_sk.mod[type].recv_from = NULL;
	millet_sk.mod[type].send_to = NULL;
	millet_sk.mod[type].init = NULL;
	return RET_OK;
}
EXPORT_SYMBOL_GPL(unregister_millet_hook);

int init_millet_subsystem(int type)
{
	if (!TYPE_VALID(type)) {
		pr_err("%s: type is invalid! %d\n",
				__func__, type);
		return RET_ERR;
	}

	atomic_set(&millet_sk.mod[type].port, 0);
	atomic_set(&millet_sk.mod[type].loop_init, 0);
	spin_lock_init(&millet_sk.mod[type].lock);
	if (millet_sk.mod[type].init)
		millet_sk.mod[type].init(&millet_sk);

	strlcpy(millet_sk.mod[type].name, NAME_ARRAY[type], NAME_MAXLEN);
	return RET_OK;
}
EXPORT_SYMBOL_GPL(init_millet_subsystem);


static int __init millet_init(void)
{
	struct netlink_kernel_cfg cfg = { .input = recv_handler };
	int type;

	for (type = 0; type < MILLET_TYPES_NUM; type++)
		spin_lock_init(&millet_sk.mod[type].lock);

	millet_sk.sock = netlink_kernel_create(&init_net, NETLINK_MILLET, &cfg);
	if (!millet_sk.sock)
		return -ENOMEM;
	millet_rootdir = proc_mkdir("millet", NULL);
	if (!millet_rootdir)
		goto fail;
	if (!proc_create("millet_stat", 0644, millet_rootdir, &millet_proc_fops) ||
	    !proc_create("version", 0444, millet_rootdir, &millet_version_fops))
		goto fail;
	WRITE_ONCE(millet_ready, true);
	pr_info("Millet protocol %u ready\n", millet_v);
	return 0;
fail:
	if (millet_rootdir)
		remove_proc_subtree("millet", NULL);
	netlink_kernel_release(millet_sk.sock);
	millet_sk.sock = NULL;
	return -ENOMEM;
}

subsys_initcall(millet_init);
MODULE_LICENSE("GPL");
// END Power_ProcessFreeze
