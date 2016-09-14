#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "mgcap_ring.h"
#include "mgcap_rx.h"
#include "mgcap.h"

#define MGCAP_VERSION  "0.0.0"

/* Module parameters, defaults. */
static char *ifname = "eth0";

/* Global variables */
static struct mgc_dev *mgc;


static int mgcap_open(struct inode *, struct file *);
static int mgcap_release(struct inode *, struct file *);
static unsigned int mgcap_poll(struct file *, poll_table *);
static long mgcap_ioctl(struct file *, unsigned int, unsigned long);
static int __init mgcap_init_module(void);
static void mgcap_exit(void);
static void __exit mgcap_exit_module(void);
static ssize_t mgcap_read(struct file *, char __user *, size_t, loff_t *);


static struct file_operations mgcap_fops = {
	.owner = THIS_MODULE,
	.open = mgcap_open,
	.read = mgcap_read,     // tmp
	.poll = mgcap_poll,
	.unlocked_ioctl = mgcap_ioctl,
	.release = mgcap_release,
};

static struct miscdevice mgcap_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mgcap",
	.fops = &mgcap_fops,
};


static int
mgcap_open(struct inode *inode, struct file *filp)
{
	pr_info("entering %s\n", __func__);
	return 0;
}

static int
mgcap_release(struct inode *inode, struct file *filp)
{
	pr_info("entering %s\n", __func__);
	return 0;
}

static unsigned int
mgcap_poll(struct file* filp, poll_table* wait)
{
	pr_info("entering %s\n", __func__);
	return 0;
}

static ssize_t
mgcap_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	int copy_len, available_read_len;
	struct mgc_ring *rx = &mgc->rxring;

	if (ring_empty(rx))
		return 0;

	available_read_len = ring_free_count(rx);
	if (count > available_read_len)
		copy_len = available_read_len;
	else
		copy_len = count;

	if (copy_to_user(buf, rx->read, copy_len)) {
		pr_info("copy_to_user failed\n");
		return -EFAULT;
	}

	ring_read_next(rx, copy_len);

	return copy_len;
}

static long
mgcap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
#if 0
	struct mgc_ring txbuf, rxbuf;

	pr_info("entering %s\n", __func__);

	switch(cmd) {
		case MGCTXSYNC:
			copy_from_user(&txbuf, (void *)arg, sizeof(struct mgc_ring));
			pr_info("MGCTXSYNC: txbuf->head: %p", txbuf.head);
			break;
		case MGCRXSYNC:
			copy_from_user(&rxbuf, (void *)arg, sizeof(struct mgc_ring));
			pr_info("MGCRXSYNC: rxbuf->head: %p", rxbuf.head);
			break;
		default:
			break;
	}
#endif
	return  ret;
}

static int __init
mgcap_init_module(void)
{
	int rc = 0, err;

	pr_info("mgcap (v%s) is loaded\n", MGCAP_VERSION);

	mgc = kmalloc(sizeof(struct mgc_dev), GFP_KERNEL);
	if (mgc == 0) {
		pr_err("fail to kmalloc: *mgc_dev\n");
		goto err;
	}

	mgc->dev = dev_get_by_name(&init_net, ifname);
	if (!mgc->dev) {
		pr_err("Could not find %s\n", ifname);
		goto err;
	}

	mgc->cpu = smp_processor_id();
	pr_info("cpuid: %d\n", mgc->cpu);

	// register
	rtnl_lock();
	err = netdev_rx_handler_register(mgc->dev, mgc_handle_frame, mgc);
	rtnl_unlock();
	if (err) {
		pr_err("%s failed to register rx_handler\n", ifname);
		goto err;
	}

	err = misc_register(&mgcap_dev);
	if (err) {
		pr_err("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		goto err;
	}

	/* tmp: Set receive buffer */

	if ((mgc->rxring.start = vmalloc(RING_SIZE + MAX_PKT_SIZE * NBULK_PKT)) == 0) {
		pr_info("fail to vmalloc\n");
		goto err;
	}
	mgc->rxring.end   = mgc->rxring.start + RING_SIZE - 1;
	mgc->rxring.write = mgc->rxring.start;
	mgc->rxring.read  = mgc->rxring.start;
	mgc->rxring.mask  = RING_SIZE - 1;

	pr_info("st: %p, wr: %p, rd: %p, end: %p\n",
			mgc->rxring.start, mgc->rxring.write, mgc->rxring.read, mgc->rxring.end);

	return rc;

err:
	mgcap_exit();
	return -1;
}
module_init(mgcap_init_module);


void
mgcap_exit(void)
{
	if (mgc->dev) {
		rtnl_lock();
		netdev_rx_handler_unregister(mgc->dev);
		rtnl_unlock();
	}

	misc_deregister(&mgcap_dev);

	/* free rx ring buffer */
	if (mgc->rxring.start) {
		vfree(mgc->rxring.start);
		mgc->rxring.start = NULL;
	}

	if (mgc) {
		kfree(mgc);
		mgc = NULL;
	}
}

static void __exit
mgcap_exit_module(void)
{
	pr_info("mgcap (v%s) is unloaded\n", MGCAP_VERSION);

	mgcap_exit();

	return;
}
module_exit(mgcap_exit_module);

MODULE_AUTHOR("Yohei Kuga <sora@haeena.net>");
MODULE_DESCRIPTION("RX Hook");
MODULE_LICENSE("GPL");
MODULE_VERSION(MGCAP_VERSION);

module_param(ifname, charp, S_IRUGO);
MODULE_PARM_DESC(ifname, "Target network device name");

