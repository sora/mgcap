#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "mgcap_ring.h"

int mgc_ring_malloc(struct mgc_ring *buf, int cpu)
{
	int ret = 0;

	if ((buf->start = kmalloc_node(RING_SIZE, GFP_KERNEL, cpu_to_node(cpu))) == 0) {
		ret = -1;
		goto err;
	}
	buf->end   = buf->start + RING_SIZE;
	buf->write = buf->start;
	buf->read  = buf->start;
	buf->mask  = RING_SIZE - 1;

err:
	return ret;
}

