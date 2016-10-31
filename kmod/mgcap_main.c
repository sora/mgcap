#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>
#include <linux/wait.h>
#include <net/genetlink.h>


#include "mgcap.h"
#include "mgcap_rx.h"
#include "mgcap_netlink.h"

#define MGCAP_VERSION  "0.0.3"

#define IOCTL_DEBUG_ENABLE    10
#define IOCTL_DEBUG_DISABLE   11

/* Module parameters, defaults. */
static int debug = 0;
static char *ifname = NULL;
static int rxbuf_size = 19;

/* Global variables */
struct mgcap mgcap;


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


static int
mgcap_open(struct inode *inode, struct file *filp)
{
	struct mgc_dev *mgc;
	char devname[IFNAMSIZ];
	strncpy (devname, filp->f_path.dentry->d_name.name, IFNAMSIZ);

	mgc = mgcap_find_dev(&mgcap, devname);
	if (!mgc) {
		pr_err("%s: no device found \"%s\"\n",
		       __func__, filp->f_path.dentry->d_name.name);
		return -ENOENT;
	}

	filp->private_data = mgc;

	func_enter();
	return 0;
}

static int
mgcap_release(struct inode *inode, struct file *filp)
{
	func_enter();

	filp->private_data = NULL;

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
	struct mgc_dev *mgc = (struct mgc_dev *)filp->private_data;
	unsigned int copy_len, read_count;
	struct rxring *rx;

	rx = &mgc->rxrings[smp_processor_id()];

	if (ring_empty(&rx->buf)) {
		return 0;
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

	return copy_len;
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

int register_mgc_dev(char *ifname)
{
	char pathdev[IFNAMSIZ];
	int rc, i;
	struct net_device *dev;
	struct mgc_dev *mgc;

	pr_info("register device \"%s\" for mgcap\n", ifname);

	dev = dev_get_by_name(&init_net, ifname);
	if (!dev) {
		pr_err("Could not find %s\n", ifname);
		goto err;
	}

	if (mgcap_find_dev(&mgcap, ifname)) {
		pr_err("dev \"%s\" is already registered for mgcap\n", ifname);
		goto err;
	}

	/* malloc mgc_dev */
	mgc = kmalloc(sizeof(struct mgc_dev), GFP_KERNEL);
	if (mgc == 0) {
		pr_err("fail to kmalloc: *mgc_dev\n");
		goto err;
	}

	mgc->num_cpus = num_online_cpus();
	pr_info("mgc->num_cpus: %d\n", mgc->num_cpus);

	/* malloc mgc_dev->rx */
	mgc->rxrings = kmalloc(sizeof(struct rxring) * MAX_CPUS, GFP_KERNEL);
	if (mgc->rxrings == 0) {
		pr_err("fail to kmalloc: *mgc_dev->rx\n");
		goto err;
	}
	
	/* malloc mgc_dev->rx->buf */
	for (i = 0; i < MAX_CPUS; i++) {
		if (cpu_online(i)) {
			mgc->rxrings[i].cpuid = i;
			rc = mgc_ring_malloc(&mgc->rxrings[i].buf, i);
			if (rc < 0) {
				pr_err("fail to kmalloc: *mgc_ring[%d]\n", i);
				goto err;
			}
			pr_info("cpu=%d, p: %p, st: %p, wr: %p, rd: %p, end: %p\n",
				i, mgc->rxrings[i].buf.p,
				mgc->rxrings[i].buf.start, mgc->rxrings[i].buf.write,
				mgc->rxrings[i].buf.read,  mgc->rxrings[i].buf.end);
		} else {
			pr_info("cpu%d is offline.", i);

			// set cpu 0 when this cpu is offline
			mgc->rxrings[i].buf.start = mgc->rxrings[0].buf.start;
			mgc->rxrings[i].buf.end   = mgc->rxrings[0].buf.end;
			mgc->rxrings[i].buf.write = mgc->rxrings[0].buf.write;
			mgc->rxrings[i].buf.read  = mgc->rxrings[0].buf.read;
			mgc->rxrings[i].buf.mask  = mgc->rxrings[0].buf.mask;
		}
	}
	

	/* mgc_dev->cur_rxring */
	mgc->cur_rxring = &mgc->rxrings[0];
	pr_info("mgc->cur_ring: %p\n", mgc->cur_rxring);

	/* mgc->dev */
	mgc->dev = dev;

	/* register character device */
	sprintf(pathdev, "%s/%s", DRV_NAME, ifname);
	memset(&mgc->mdev, 0, sizeof(mgc->mdev));
	mgc->mdev.minor = MISC_DYNAMIC_MINOR;
	mgc->mdev.fops = &mgcap_fops;
	mgc->mdev.name = pathdev;

	rc = misc_register(&mgc->mdev);
	if (rc) {
		pr_err("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		goto err;
	}

	// netdev_rx_handler_register
	mgc->capture_mode = MGCAP_CAPTURE_MODE_MIRROR;	/* default */
	rtnl_lock();
	rc = netdev_rx_handler_register(mgc->dev, mgc_handle_frame, mgc);
	rtnl_unlock();
	if (rc) {
		pr_err("%s failed to register rx_handler\n", ifname);
		goto err;
	}

	// save mgcap_dev to mgcap->dev_list
	mgcap_add_dev(&mgcap, mgc);

	return 0;

err:
	return -1;

}

int unregister_mgc_dev(struct mgc_dev *mgc)
{
	int i;

	pr_info("unregister device \"%s\" from mgcap\n", mgc->dev->name);

	mgcap_del_dev(mgc);

	if (mgc->dev) {
		rtnl_lock();
		netdev_rx_handler_unregister(mgc->dev);
		rtnl_unlock();
	}

	misc_deregister(&mgc->mdev);

	/* free rx ring buffer */
	for (i = 0; i < MAX_CPUS; i++) {
		if (mgc->rxrings[i].buf.p != NULL) {
			pr_info("kfree: cpu%d\n", i);
			kfree(mgc->rxrings[i].buf.p);
			mgc->rxrings[i].buf.p = NULL;
		}
	}

	/* free rx ring buffers */
	if (mgc->rxrings) {
		kfree(mgc->rxrings);
		mgc->rxrings = NULL;
	}

	if (mgc) {
		kfree_rcu(mgc, rcu);
		mgc = NULL;
	}

	return 0;
}

/* generic netlinkl operations */

static struct genl_family mgcap_nl_family = {
	.id		= GENL_ID_GENERATE,
	.name		= MGCAP_GENL_NAME,
	.version	= MGCAP_GENL_VERSION,
	.maxattr	= MGCAP_ATTR_MAX,
	.hdrsize	= 0,
};

static struct nla_policy mgcap_nl_policy[MGCAP_ATTR_MAX + 1] = {
	[MGCAP_ATTR_DEVICE]		= { .type = NLA_U32 },
	[MGCAP_ATTR_CAPTURE_MODE]	= { .type = NLA_U32 },
};

static int
mgcap_nl_cmd_start(struct sk_buff *skb, struct genl_info *info)
{
	int rc;
	u32 ifindex;
	struct net_device *dev;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return -EINVAL;
	}

	if (!info->attrs[MGCAP_ATTR_DEVICE]) {
		pr_err("%s: device is not specified.\n", __func__);
		return -EINVAL;
	}

	ifindex = nla_get_u32(info->attrs[MGCAP_ATTR_DEVICE]);
	dev = __dev_get_by_index(&init_net, ifindex);

	if (!dev) {
		pr_err("%s: ifindex \"%u\" does not exist.\n",
		       __func__, ifindex);
		return -ENODEV;
	}

	if (mgcap_find_dev(&mgcap, dev->name)) {
		pr_err("%s: device \"%s\" is already mgcaped.\n",
		       __func__, dev->name);
		return -EBUSY;
	}

	rc = register_mgc_dev(dev->name);
	if (rc < 0) {
		pr_err("%s: failed to register \"%s\" for mgcap.\n",
		       __func__, dev->name);
		return -ENOEXEC;
	}

	return 0;
}

static int
mgcap_nl_cmd_stop(struct sk_buff *skb, struct genl_info *info)
{
	u32 ifindex;
	struct net_device *dev;
	struct mgc_dev *mgc;

	if (!info->attrs[MGCAP_ATTR_DEVICE]) {
		pr_err("%s: device is not specified.\n", __func__);
		return -EINVAL;
	}

	ifindex = nla_get_u32(info->attrs[MGCAP_ATTR_DEVICE]);
	dev = __dev_get_by_index(&init_net, ifindex);

	if (!dev) {
		pr_err("%s: ifindex \"%u\" does not exist.\n",
		       __func__, ifindex);
		return -ENODEV;
	}

	mgc = mgcap_find_dev(&mgcap, dev->name);
	if (!mgc) {
		pr_err("%s: device \"%s\" is not mgcaped.\n",
		       __func__, dev->name);
		return -ENOENT;
	}

	unregister_mgc_dev(mgc);

	return 0;
}

static int
mgcap_nl_cmd_set_capture_mode(struct sk_buff *skb, struct genl_info *info)
{
	u32 ifindex;
	u32 capture_mode;
	struct net_device *dev;
	struct mgc_dev *mgc;

	if (!info->attrs[MGCAP_ATTR_DEVICE]) {
		pr_err("%s: device is not specified.\n", __func__);
		return -EINVAL;
	}

	ifindex = nla_get_u32(info->attrs[MGCAP_ATTR_DEVICE]);
	dev = __dev_get_by_index(&init_net, ifindex);

	if (!dev) {
		pr_err("%s: ifindex \"%u\" does not exist.\n",
		       __func__, ifindex);
		return -ENODEV;
	}

	if (!info->attrs[MGCAP_ATTR_CAPTURE_MODE]) {
		pr_err("%s: capture mode is not specified.\n", __func__);
		return -EINVAL;
	}

	capture_mode = nla_get_u32(info->attrs[MGCAP_ATTR_CAPTURE_MODE]);


	mgc = mgcap_find_dev(&mgcap, dev->name);
	if (!mgc) {
		pr_err("%s: device \"%s\" is not mgcaped.\n",
		       __func__, dev->name);
		return -ENOENT;
	}

	switch (capture_mode) {
	case MGCAP_CAPTURE_MODE_DROP :
		pr_info("set device \"%s\" capture mode to drop\n",
			dev->name);
		break;
	case MGCAP_CAPTURE_MODE_MIRROR :
		pr_info("set device \"%s\" capture mode to mirror\n",
			dev->name);
		break;
	default:
		pr_info("invalid capture mode \"%u\" for device \"%s\"\n",
			capture_mode, dev->name);
		return -EINVAL;
	}

	mgc->capture_mode = capture_mode;

	return 0;
}

static struct genl_ops mgcap_nl_ops[] = {
	{
		.cmd	= MGCAP_CMD_START,
		.doit	= mgcap_nl_cmd_start,
		.policy	= mgcap_nl_policy,
		.flags	= GENL_ADMIN_PERM,
	},
	{
		.cmd	= MGCAP_CMD_STOP,
		.doit	= mgcap_nl_cmd_stop,
		.policy	= mgcap_nl_policy,
		.flags	= GENL_ADMIN_PERM,
	},
	{
		.cmd	= MGCAP_CMD_SET_CAPTURE_MODE,
		.doit	= mgcap_nl_cmd_set_capture_mode,
		.policy	= mgcap_nl_policy,
		.flags	= GENL_ADMIN_PERM,
	},
};


static void mgcap_init(struct mgcap *mgcap)
{
	INIT_LIST_HEAD(&mgcap->dev_list);
}

static int __init
mgcap_init_module(void)
{
	int rc;

	func_enter();

	pr_info("mgcap (v%s) is loaded\n", MGCAP_VERSION);
	mgcap_init(&mgcap);

	rc = genl_register_family_with_ops(&mgcap_nl_family, mgcap_nl_ops);
	if (rc != 0)
		goto genl_failed;

	if (ifname) {
		rc = register_mgc_dev(ifname);
		if (rc < 0)
			goto modparam_err;
	}

	return rc;

modparam_err:
	mgcap_exit();
	genl_unregister_family(&mgcap_nl_family);
genl_failed:
	return rc;
}
module_init(mgcap_init_module);


void
mgcap_exit(void)
{
	struct list_head *p, *tmp;
	struct mgc_dev *mgc;

	list_for_each_safe(p, tmp, &mgcap.dev_list) {
		mgc = list_entry(p, struct mgc_dev, list);
		unregister_mgc_dev(mgc);
	}
}

static void __exit
mgcap_exit_module(void)
{
	genl_unregister_family(&mgcap_nl_family);
	mgcap_exit();
	pr_info("mgcap (v%s) is unloaded\n", MGCAP_VERSION);

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

