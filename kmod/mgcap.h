#ifndef _MGCAP_H_
#define _MGCAP_H_

#include <linux/miscdevice.h>

#include "mgcap_ring.h"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define func_enter() pr_debug("entering %s\n", __func__);

#define BUF_INFO(X) \
printk("[%s]: start: %p, end: %p, rd: %p, wr: %p\n", \
	__func__, X.start, X.end, X.read, X.write);

#define DRV_NAME     "mgcap"

#define MGC_SNAPLEN  96
#define MAX_CPUS     20

/* ioctl cmd number */
#define MGCTXSYNC    10
#define MGCRXSYNC    11

struct rxring {
	struct mgc_ring buf;
	uint8_t cpuid;
	struct rxring *next;
};

struct mgc_dev {
	struct list_head list;	/* mgcap->dev_list */
	struct rcu_head	rcu;

	struct net_device *dev;

	int capture_mode;	/* CAPTURE_MODE_DROP/PASS */

	uint8_t num_cpus;

	struct rxring *rxrings;

	struct rxring *cur_rxring;

	struct miscdevice mdev;	/* miscdevice structure */
};

struct mgcap {
	struct list_head dev_list;	/* mgc_dev */
};

static inline struct mgc_dev *mgcap_find_dev(struct mgcap *mgcap,
					     char *devname)
{
	struct mgc_dev *mgc;

	list_for_each_entry_rcu(mgc, &mgcap->dev_list, list) {
		if (strncmp(mgc->dev->name, devname, IFNAMSIZ) == 0)
			return mgc;
	}
	return NULL;
}

static inline void mgcap_add_dev(struct mgcap *mgcap, struct mgc_dev *mgc)
{
	list_add_rcu(&mgc->list, &mgcap->dev_list);
}

static inline void mgcap_del_dev(struct mgc_dev *mgc)
{
	list_del_rcu(&mgc->list);
}



/* Global variables */
extern struct mgcap mgcap;


static inline struct rxring *next_rxring(const struct rxring *rx)
{
	return rx->next;
}

#endif /* _MGCAP_H_ */


