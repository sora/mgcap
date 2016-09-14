#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/err.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>

#include "mgcap.h"
#include "mgcap_rx.h"

/* note: already called with rcu_read_lock */
rx_handler_result_t mgc_handle_frame(struct sk_buff **pskb)
{
	rx_handler_result_t res = RX_HANDLER_CONSUMED;
	struct sk_buff *skb = *pskb;
	unsigned short ethhdr_len, data_len;
	struct mgc_dev *dev;
	struct mgc_ring *rx;
	struct skb_shared_hwtstamps *hwtstamps = skb_hwtstamps(skb);

	dev = rcu_dereference(skb->dev->rx_handler_data);
	rx = &dev->rxring;

	ethhdr_len = (unsigned short)skb->mac_len;
	data_len = (unsigned short)skb->len;

/*
	*(unsigned short *)rx->write = ethhdr_len + data_len;
	rx->write += 2;
	memcpy(rx->write, skb_mac_header(skb), (int)ethhdr_len);
	rx->write += ethhdr_len;
	memcpy(rx->write, skb->data, (int)data_len);
	ring_write_next(rx, (int)data_len);
*/

	pr_info("skb->mac_len: %u, skb->len: %u, hwtstamps->hwtstamp.tv64: %lld\n",
		ethhdr_len, data_len, hwtstamps->hwtstamp.tv64);

	kfree_skb(skb);
	*pskb = NULL;

	return res;
}

