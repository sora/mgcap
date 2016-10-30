#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>

#define RXHOOK_VERSION  "0.0.0"

struct rh_dev {
	struct net_device *dev;
	char dev_name[IFNAMSIZ];

	unsigned int cpu;
};

/* Module parameters, defaults. */
//static int debug = 0;
static char *ifname = "eth0";

/* Global variables */
static struct rh_dev *rh;
static unsigned int pps[16];


/* note: already called with rcu_read_lock */
static rx_handler_result_t rh_handle_frame(struct sk_buff **pskb)
{
	rx_handler_result_t res = RX_HANDLER_CONSUMED;
	struct sk_buff *skb = *pskb;
	//const unsigned char *dest = eth_hdr(skb)->h_dest;
	//pr_info("Mac address: %pM\n", dest);

	++pps[smp_processor_id()];

	kfree_skb(skb);
	*pskb = NULL;

	return res;
}

static int __init rh_init(void)
{
	int rc = 0, err;
	int i;

	pr_info("rxhook (v%s) is loaded\n", RXHOOK_VERSION);

	rh = kmalloc(sizeof(struct rh_dev), GFP_KERNEL);
	if (rh == 0) {
		pr_info("fail to kmalloc: *rh_dev\n");
		rc = -1;
		goto out;
	}

	rh->dev = dev_get_by_name(&init_net, ifname);
	if (!rh->dev) {
		pr_err("Could not find %s\n", ifname);
		rc = -1;
		goto out;
	}

	rh->cpu = smp_processor_id();
	pr_info("cpuid: %d\n", rh->cpu);

	// register
	rtnl_lock();
	err = netdev_rx_handler_register(rh->dev, rh_handle_frame, rh);
	rtnl_unlock();
	if (err) {
		pr_err("%s failed to register rx_handler\n", ifname);
	}

	for(i = 0; i < 10; i++)
		pps[i] = 0;

out:
	return rc;
}
module_init(rh_init);


static void __exit rh_release(void)
{
	int i;
	unsigned int sum = 0;

	pr_info("rxhook (v%s) is unloaded\n", RXHOOK_VERSION);

	rtnl_lock();
	netdev_rx_handler_unregister(rh->dev);
	rtnl_unlock();

	kfree(rh);
	rh = NULL;

	for(i = 0; i < 10; i++) {
		pr_info("pps[%d] = %d\n", i, pps[i]);
		sum += pps[i];
	}
	pr_info("sum: %u\n", sum);

	return;
}
module_exit(rh_release);


MODULE_AUTHOR("Yohei Kuga <sora@haeena.net>");
MODULE_DESCRIPTION("RX Hook");
MODULE_LICENSE("GPL");
MODULE_VERSION(RXHOOK_VERSION);

module_param(ifname, charp, S_IRUGO);
MODULE_PARM_DESC(ifname, "Target network device name");

