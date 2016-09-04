#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/rtnetlink.h>

#define MGCAPMOD_VERSION  "0.0.0"
#define DRV_NAME          "mgc"

#define CAPTURE_PKT_SIZE  (96)
#define RXRING_SIZE       (CAPTURE_PKT_SIZE * 1024)
#define XMIT_BUDGET       (0xFF)

#define func_enter() pr_debug("entering %s\n", __func__);

struct mgc_dev {
	/* target network device */
	struct net_device *dev;

	char dev_name[IFNAMSIZ];

	unsigned int cpu;
};

/* Module parameters, defaults. */
static char *ifname = "eth0";
static int debug = 0;

/* Global variables */
static struct mgc_dev *mgc;
static unsigned int pps[16];


/* note: already called with rcu_read_lock */
static rx_handler_result_t mgc_handle_frame(struct sk_buff **pskb)
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

static int __init mgc_init(void)
{
	int rc = 0, err;
	int i;

	pr_info("mgcapmod (v%s) is loaded\n", MGCAPMOD_VERSION);

	mgc = kmalloc(sizeof(struct mgc_dev), GFP_KERNEL);
	if (mgc == 0) {
		pr_info("fail to kmalloc: *mgc_dev\n");
		rc = -1;
		goto out;
	}

	mgc->dev = dev_get_by_name(&init_net, ifname);
	if (!mgc->dev) {
		pr_err("Could not find %s\n", ifname);
		rc = -1;
		goto out;
	}

	mgc->cpu = smp_processor_id();
	pr_info("cpuid: %d\n", mgc->cpu);

	// register
	rtnl_lock();
	err = netdev_rx_handler_register(mgc->dev, mgc_handle_frame, mgc);
	rtnl_unlock();
	if (err) {
		pr_err("%s failed to register rx_handler\n", ifname);
	}

	for(i = 0; i < 10; i++)
		pps[i] = 0;

out:
	return rc;
}
module_init(mgc_init);


static void __exit mgc_release(void)
{
	unsigned int sum = 0;
	int i;

	pr_info("mgcapmod (v%s) is unloaded\n", MGCAPMOD_VERSION);

	rtnl_lock();
	netdev_rx_handler_unregister(mgc->dev);
	rtnl_unlock();

	kfree(mgc);
	mgc = NULL;

	for(i = 0; i < 10; i++) {
		pr_info("pps[%d] = %d\n", i, pps[i]);
		sum += pps[i];
	}
	pr_info("sum: %u\n", sum);

	return;
}
module_exit(mgc_release);


MODULE_AUTHOR("Yohei Kuga <sora@haeena.net>");
MODULE_DESCRIPTION("A pcap capturing device for Monitoring Metrics");
MODULE_LICENSE("GPL");
MODULE_VERSION(MGCAPMOD_VERSION);

module_param(ifname, charp, S_IRUGO);
MODULE_PARM_DESC(ifname, "Target network device name");
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debug mode");

