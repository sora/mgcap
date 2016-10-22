#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>
#include <linux/wait.h>

#include "mgcap.h"
#include "mgcap_rx.h"

#define MGCAP_VERSION  "0.0.0"

#define IOCTL_DEBUG_ENABLE    10
#define IOCTL_DEBUG_DISABLE   11

/* Module parameters, defaults. */
static int debug = 0;
static char *ifname = "eth0";
static int rxbuf_size = 19;

/* Global variables */
struct mgc_dev *mgc;


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
	.read = mgcap_read,
	.poll = mgcap_poll,
	.unlocked_ioctl = mgcap_ioctl,
	.release = mgcap_release,
};

static struct miscdevice mgcap_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DRV_NAME,
	.fops = &mgcap_fops,
};


static int
mgcap_open(struct inode *inode, struct file *filp)
{
	func_enter();
	return 0;
}

static int
mgcap_release(struct inode *inode, struct file *filp)
{
	func_enter();
	return 0;
}

static unsigned int
mgcap_poll(struct file* filp, poll_table* wait)
{
	func_enter();
	return 0;
}

static ssize_t
mgcap_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int copy_len, read_count;
	struct rxring *rx = mgc->cur_rxring;

	uint8_t ring_budget = mgc->num_cpus;

	while(ring_budget--) {
		if (ring_empty(&rx->buf)) {
			rx = next_rxring(rx);
			if (debug)
				pr_info("ring_empty\n");
			continue;
		}

		read_count = ring_count_end(&rx->buf);
		if (count > read_count)
			copy_len = read_count;
		else
			copy_len = count;

		if (copy_to_user(buf, rx->buf.read, copy_len)) {
			pr_err("copy_to_user failed\n");
			return -EFAULT;
		}
		ring_read_next(&rx->buf, copy_len);
//		if (debug) {
//			pr_info("2 read_end: copy_len=%u, read_count=%u, count=%u\n",
//				copy_len, read_count, (unsigned int)count);
//			pr_info("2 read_end: start=%p, read=%p, write=%p, end=%p\n",
//				rx->buf.start, rx->buf.read, rx->buf.write, rx->buf.end);
//		}

		return copy_len;
	}
	mgc->cur_rxring = rx;

	return 0;
}

static long
mgcap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	//struct mgc_ring txbuf, rxbuf;

	pr_info("entering %s\n", __func__);

	switch(cmd) {
		case IOCTL_DEBUG_ENABLE:
			pr_info("IOCTL_DEBUG_ENABLE\n");
			debug = 1;
			break;
		case IOCTL_DEBUG_DISABLE:
			pr_info("IOCTL_DEBUG_DISABLE\n");
			debug = 0;
			break;
#if 0
		case MGCTXSYNC:
			copy_from_user(&txbuf, (void *)arg, sizeof(struct mgc_ring));
			pr_info("MGCTXSYNC: txbuf->head: %p", txbuf.head);
			break;
		case MGCRXSYNC:
			copy_from_user(&rxbuf, (void *)arg, sizeof(struct mgc_ring));
			pr_info("MGCRXSYNC: rxbuf->head: %p", rxbuf.head);
			break;
#endif
		default:
			break;
	}

	return  ret;
}

static int __init
mgcap_init_module(void)
{
	char pathdev[IFNAMSIZ];
	int rc, cpu, i;

	func_enter();

	pr_info("mgcap (v%s) is loaded\n", MGCAP_VERSION);

	/* malloc mgc_dev */
	mgc = kmalloc(sizeof(struct mgc_dev), GFP_KERNEL);
	if (mgc == 0) {
		pr_err("fail to kmalloc: *mgc_dev\n");
		goto err;
	}

	mgc->num_cpus = num_online_cpus();
	pr_info("mgc->num_cpus: %d\n", mgc->num_cpus);

	/* malloc mgc_dev->rx */
	mgc->rxrings = kmalloc(sizeof(struct rxring) * mgc->num_cpus, GFP_KERNEL);
	if (mgc->rxrings == 0) {
		pr_err("fail to kmalloc: *mgc_dev->rx\n");
		goto err;
	}
	
	/* malloc mgc_dev->rx->buf */
	i = 0;
	for_each_online_cpu(cpu) {
		mgc->rxrings[i].cpuid = cpu;
		rc = mgc_ring_malloc(&mgc->rxrings[i].buf, cpu);
		if (rc < 0) {
			pr_err("fail to kmalloc: *mgc_ring[%d], cpu=%d\n", i, cpu);
			goto err;
		}
		mgc->rxrings[i].next = &mgc->rxrings[(i + 1) % mgc->num_cpus];
		pr_info("cpu=%d, rxbuf[%d], st: %p, wr: %p, rd: %p, end: %p\n",
			cpu, i,
			mgc->rxrings[i].buf.start, mgc->rxrings[i].buf.write,
			mgc->rxrings[i].buf.read,  mgc->rxrings[i].buf.end);

		++i;
	}

	/* mgc_dev->cur_rxring */
	mgc->cur_rxring = &mgc->rxrings[0];
	pr_info("mgc->cur_ring: %p\n", mgc->cur_rxring);


	/* mgc->dev */
	mgc->dev = dev_get_by_name(&init_net, ifname);
	if (!mgc->dev) {
		pr_err("Could not find %s\n", ifname);
		goto err;
	}

	/* register character device */
	sprintf(pathdev, "%s/%s", DRV_NAME, ifname);
	mgcap_dev.name = pathdev;
	rc = misc_register(&mgcap_dev);
	if (rc) {
		pr_err("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		goto err;
	}

	// netdev_rx_handler_register
	rtnl_lock();
	rc = netdev_rx_handler_register(mgc->dev, mgc_handle_frame, mgc);
	rtnl_unlock();
	if (rc) {
		pr_err("%s failed to register rx_handler\n", ifname);
		goto err;
	}

	return rc;

err:
	mgcap_exit();
	return -1;
}
module_init(mgcap_init_module);


void
mgcap_exit(void)
{
	int i;

	if (mgc->dev) {
		rtnl_lock();
		netdev_rx_handler_unregister(mgc->dev);
		rtnl_unlock();
	}

	misc_deregister(&mgcap_dev);

	/* free rx ring buffer */
	for (i = 0; i < mgc->num_cpus; i++) {
		if (mgc->rxrings[i].buf.start) {
			kfree(mgc->rxrings[i].buf.start);
			mgc->rxrings[i].buf.start = NULL;
		}
	}

	/* free rx ring buffers */
	if (mgc->rxrings) {
		kfree(mgc->rxrings);
		mgc->rxrings = NULL;
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
MODULE_DESCRIPTION("MGCAP device");
MODULE_LICENSE("GPL");
MODULE_VERSION(MGCAP_VERSION);

module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "debug flag");
module_param(ifname, charp, S_IRUGO);
MODULE_PARM_DESC(ifname, "Target network device name");
module_param(rxbuf_size, int, S_IRUGO);
MODULE_PARM_DESC(rxbuf_size, "RX ring size on each received packet (1<<rxbuf_size)");

