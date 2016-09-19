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
#define MAX_CPUS     20

/* ioctl cmd number */
#define MGCTXSYNC    10
#define MGCRXSYNC    11

struct rxring {
	struct mgc_ring buf;
	uint8_t cpuid;
};

struct mgc_dev {
	struct net_device *dev;

	uint8_t num_cpus;

	struct rxring *rx;

	struct rxring *cur_rxring;
};

#endif /* _MGCAP_H_ */


