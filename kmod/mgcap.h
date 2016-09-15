#ifndef _MGCAP_H_
#define _MGCAP_H_

#include "mgcap_ring.h"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MGC_SNAPLEN  96

/* ioctl cmd number */
#define MGCTXSYNC    10
#define MGCRXSYNC    11

struct mgc_dev {
	struct net_device *dev;
	char dev_name[IFNAMSIZ];

	struct mgc_ring txring;

	struct mgc_ring rxring;

	unsigned int cpu;
};

#endif /* _MGCAP_H_ */

