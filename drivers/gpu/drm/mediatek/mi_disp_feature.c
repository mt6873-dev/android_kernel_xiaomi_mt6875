// SPDX-License-Identifier: GPL-2.0
/*
 * mi_disp_feature: /dev/mi_display/disp_feature bridge for on-screen FOD
 *
 * The HyperOS SurfaceFlinger FOD monitor talks to the display driver through
 * this node.  On this platform the panel only supports whole-panel HBM, so the
 * bridge simply translates the fingerprint status reported by SurfaceFlinger
 * into DISPPARAM_HBM_FOD_ON / DISPPARAM_HBM_FOD_OFF requests.  Together with
 * the dim overlay drawn by SystemUI this reproduces the stock FOD highlight.
 */

#define pr_fmt(fmt) "mi_disp_feature: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/errno.h>
#include <linux/types.h>

#define MI_DISP_CLASS_NAME	"mi_display"
#define MI_DISP_DEV_NAME	"disp_feature"

/* feature ids used by SurfaceFlinger */
#define MI_DISP_FEATURE_LOW_BRIGHTNESS_FOD	0x0B
#define MI_DISP_FEATURE_FP_STATUS		0x0C

/* fingerprint states */
#define MI_FP_UP		0
#define MI_FP_DOWN		1
#define MI_FP_UNLOCK_SUCCESS	2
#define MI_FP_UNLOCK_FAIL	3

/* HBM hold time per state, milliseconds */
#define MI_FOD_HOLD_DOWN	3000
#define MI_FOD_HOLD_SUCCESS	200
#define MI_FOD_HOLD_FAIL	500

struct mi_disp_base {
	__u32 flag;
	__u32 disp_id;
};

struct mi_disp_feature_req {
	struct mi_disp_base base;
	__u32 feature_id;
	__s32 feature_val;
	__u32 tx_len;
	__u64 tx_ptr;
	__u32 rx_len;
	__u64 rx_ptr;
};

struct mi_disp_brightness_req {
	struct mi_disp_base base;
	__u32 brightness;
	__u32 brightness_clone;
};

struct mi_disp_event_req {
	struct mi_disp_base base;
	__u32 type;
};

struct mi_disp_version {
	struct mi_disp_base base;
	__u32 version;
};

#define MI_DISP_IOCTL_VERSION		_IOR('D', 0x00, struct mi_disp_version)
#define MI_DISP_IOCTL_SET_FEATURE	_IOWR('D', 0x01, struct mi_disp_feature_req)
#define MI_DISP_IOCTL_REGISTER_EVENT	_IOW('D', 0x07, struct mi_disp_event_req)
#define MI_DISP_IOCTL_DEREGISTER_EVENT	_IOW('D', 0x08, struct mi_disp_event_req)
#define MI_DISP_IOCTL_GET_BRIGHTNESS	_IOR('D', 0x0B, struct mi_disp_brightness_req)
#define MI_DISP_IOCTL_SET_BRIGHTNESS	_IOW('D', 0x0C, struct mi_disp_brightness_req)
#define MI_DISP_IOCTL_SET_COUNT_INFO	_IOWR('D', 0x0D, struct mi_disp_feature_req)
#define MI_DISP_IOCTL_GET_FEATURE	_IOWR('D', 0x0F, struct mi_disp_feature_req)

/* provided by mtk_dsi.c */
extern int mtk_dsi_set_fod_hbm(bool en);

static dev_t mi_dev;
static struct cdev mi_cdev;
static struct class *mi_class;
static struct device *mi_device;

static DEFINE_MUTEX(mi_lock);
static DECLARE_WAIT_QUEUE_HEAD(mi_event_wq);
static struct workqueue_struct *mi_wq;
static struct delayed_work mi_off_work;

static bool mi_hbm_on;
static unsigned long mi_off_at;

static void mi_fod_off_work(struct work_struct *work)
{
	bool turned_off = false;

	mutex_lock(&mi_lock);
	if (mi_hbm_on && time_after_eq(jiffies, mi_off_at)) {
		if (mtk_dsi_set_fod_hbm(false) == 0) {
			mi_hbm_on = false;
			turned_off = true;
		}
	}
	mutex_unlock(&mi_lock);

	if (turned_off)
		pr_info("FOD HBM off by timer\n");
}

static void mi_handle_fp_status(int status)
{
	bool want_on;
	unsigned int hold_ms = 0;
	bool changed = false;

	switch (status) {
	case MI_FP_UP:
		want_on = false;
		break;
	case MI_FP_DOWN:
		want_on = true;
		hold_ms = MI_FOD_HOLD_DOWN;
		break;
	case MI_FP_UNLOCK_SUCCESS:
		want_on = true;
		hold_ms = MI_FOD_HOLD_SUCCESS;
		break;
	case MI_FP_UNLOCK_FAIL:
		want_on = true;
		hold_ms = MI_FOD_HOLD_FAIL;
		break;
	default:
		return;
	}

	mutex_lock(&mi_lock);
	if (!want_on) {
		if (mi_hbm_on && mtk_dsi_set_fod_hbm(false) == 0) {
			mi_hbm_on = false;
			changed = true;
			pr_info("FOD HBM off (fp status %d)\n", status);
		}
	} else {
		if (!mi_hbm_on && mtk_dsi_set_fod_hbm(true) == 0) {
			mi_hbm_on = true;
			changed = true;
			pr_info("FOD HBM on (fp status %d)\n", status);
		}
		if (want_on && mi_hbm_on)
			mi_off_at = jiffies + msecs_to_jiffies(hold_ms);
	}
	mutex_unlock(&mi_lock);

	if (want_on && mi_hbm_on && (changed || hold_ms))
		mod_delayed_work(mi_wq, &mi_off_work,
				 msecs_to_jiffies(hold_ms));
}

static long mi_disp_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct mi_disp_feature_req freq;
	struct mi_disp_brightness_req breq;
	struct mi_disp_event_req ereq;
	struct mi_disp_version vreq;
	void __user *uarg = (void __user *)arg;

	if (_IOC_TYPE(cmd) != 'D')
		return -ENOTTY;

	switch (cmd) {
	case MI_DISP_IOCTL_VERSION:
		if (_IOC_SIZE(cmd) < sizeof(vreq))
			return -EINVAL;
		memset(&vreq, 0, sizeof(vreq));
		vreq.version = 1;
		if (copy_to_user(uarg, &vreq, sizeof(vreq)))
			return -EFAULT;
		return 0;

	case MI_DISP_IOCTL_SET_FEATURE:
		if (_IOC_SIZE(cmd) < sizeof(freq))
			return -EINVAL;
		if (copy_from_user(&freq, uarg, sizeof(freq)))
			return -EFAULT;
		if (freq.feature_id == MI_DISP_FEATURE_FP_STATUS)
			mi_handle_fp_status(freq.feature_val);
		else if (freq.feature_id == MI_DISP_FEATURE_LOW_BRIGHTNESS_FOD)
			pr_info_ratelimited("low brightness fod, skip\n");
		return 0;

	case MI_DISP_IOCTL_GET_FEATURE:
		if (_IOC_SIZE(cmd) < sizeof(freq))
			return -EINVAL;
		if (copy_from_user(&freq, uarg, sizeof(freq)))
			return -EFAULT;
		freq.rx_len = 0;
		freq.rx_ptr = 0;
		if (copy_to_user(uarg, &freq, sizeof(freq)))
			return -EFAULT;
		return 0;

	case MI_DISP_IOCTL_GET_BRIGHTNESS:
		if (_IOC_SIZE(cmd) < sizeof(breq))
			return -EINVAL;
		memset(&breq, 0, sizeof(breq));
		breq.brightness = 0x7ff;
		breq.brightness_clone = 0x7ff;
		if (copy_to_user(uarg, &breq, sizeof(breq)))
			return -EFAULT;
		return 0;

	case MI_DISP_IOCTL_SET_BRIGHTNESS:
		if (_IOC_SIZE(cmd) < sizeof(breq))
			return -EINVAL;
		if (copy_from_user(&breq, uarg, sizeof(breq)))
			return -EFAULT;
		return 0;

	case MI_DISP_IOCTL_SET_COUNT_INFO:
		if (_IOC_SIZE(cmd) < sizeof(freq))
			return -EINVAL;
		if (copy_from_user(&freq, uarg, sizeof(freq)))
			return -EFAULT;
		return 0;

	case MI_DISP_IOCTL_REGISTER_EVENT:
	case MI_DISP_IOCTL_DEREGISTER_EVENT:
		if (_IOC_SIZE(cmd) < sizeof(ereq))
			return -EINVAL;
		if (copy_from_user(&ereq, uarg, sizeof(ereq)))
			return -EFAULT;
		return 0;

	default:
		pr_info_ratelimited("unknown ioctl 0x%x\n", cmd);
		return 0;
	}
}

static ssize_t mi_disp_read(struct file *file, char __user *buf,
			    size_t count, loff_t *ppos)
{
	return -EAGAIN;
}

static unsigned int mi_disp_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &mi_event_wq, wait);
	return 0;
}

static int mi_disp_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mi_disp_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mi_disp_fops = {
	.owner		= THIS_MODULE,
	.open		= mi_disp_open,
	.release	= mi_disp_release,
	.read		= mi_disp_read,
	.poll		= mi_disp_poll,
	.unlocked_ioctl	= mi_disp_ioctl,
};

static char *mi_disp_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0660;

	return kasprintf(GFP_KERNEL, "%s/%s", MI_DISP_CLASS_NAME, dev_name(dev));
}

static int __init mi_disp_feature_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&mi_dev, 0, 1, MI_DISP_DEV_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	cdev_init(&mi_cdev, &mi_disp_fops);
	mi_cdev.owner = THIS_MODULE;
	ret = cdev_add(&mi_cdev, mi_dev, 1);
	if (ret < 0) {
		pr_err("cdev_add failed: %d\n", ret);
		goto err_chrdev;
	}

	mi_class = class_create(THIS_MODULE, MI_DISP_CLASS_NAME);
	if (IS_ERR(mi_class)) {
		ret = PTR_ERR(mi_class);
		pr_err("class_create failed: %d\n", ret);
		goto err_cdev;
	}
	mi_class->devnode = mi_disp_devnode;

	mi_device = device_create(mi_class, NULL, mi_dev, NULL, MI_DISP_DEV_NAME);
	if (IS_ERR(mi_device)) {
		ret = PTR_ERR(mi_device);
		pr_err("device_create failed: %d\n", ret);
		goto err_class;
	}

	mi_wq = alloc_ordered_workqueue("mi_disp_feature", 0);
	if (!mi_wq) {
		ret = -ENOMEM;
		goto err_device;
	}
	INIT_DELAYED_WORK(&mi_off_work, mi_fod_off_work);

	pr_info("FOD bridge ready at /dev/%s/%s\n",
		MI_DISP_CLASS_NAME, MI_DISP_DEV_NAME);
	return 0;

err_device:
	device_destroy(mi_class, mi_dev);
err_class:
	class_destroy(mi_class);
err_cdev:
	cdev_del(&mi_cdev);
err_chrdev:
	unregister_chrdev_region(mi_dev, 1);
	return ret;
}

static void __exit mi_disp_feature_exit(void)
{
	if (mi_wq) {
		cancel_delayed_work_sync(&mi_off_work);
		if (mi_hbm_on)
			mtk_dsi_set_fod_hbm(false);
		destroy_workqueue(mi_wq);
		mi_wq = NULL;
	}
	device_destroy(mi_class, mi_dev);
	class_destroy(mi_class);
	cdev_del(&mi_cdev);
	unregister_chrdev_region(mi_dev, 1);
}

module_init(mi_disp_feature_init);
module_exit(mi_disp_feature_exit);

MODULE_DESCRIPTION("mi_display feature node FOD bridge");
MODULE_LICENSE("GPL v2");
