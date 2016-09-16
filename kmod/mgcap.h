#ifndef _MGCAP_H_
#define _MGCAP_H_

#include "mgcap_ring.h"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define func_enter() pr_debug("entering %s\n", __func__);

#define BUF_INFO(X) \
printk("[%s]: start: %p, end: %p, rd: %p, wr: %p\n", \
	__func__, X.start, X.end, X.read, X.write);

#define DRV_NAME     "mgcap"

#define MGC_SNAPLEN  96

/* ioctl cmd number */
#define MGCTXSYNC    10
#define MGCRXSYNC    11

struct rxthread {
	unsigned int cpu;			/* cpu id that the thread is runnig */
	struct task_struct *tsk;		/* xmit kthread */
	struct completion start_done;
	struct mgc_ring buf;
};

struct rxring {
	unsigned int cpu;
	struct mgc_ring buf;
	struct list_head list;
};

struct mgc_dev {
	struct net_device *dev;

	struct rxthread rxth;

	struct rxring *rx;
};

#endif /* _MGCAP_H_ */

