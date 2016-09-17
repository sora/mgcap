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
//	unsigned short ethhdr_len, data_len;
	unsigned short pktlen;
	struct mgc_dev *dev;
	struct mgc_ring *rx;
	struct skb_shared_hwtstamps *hwtstamps = skb_hwtstamps(skb);

	dev = rcu_dereference(skb->dev->rx_handler_data);
	rx = &dev->rx[0].buf;

	pktlen = skb->mac_len + skb->len;

	*(unsigned short *)rx->write = pktlen;
	rx->write += MGC_HDR_PKTLEN_SIZE;
	*(unsigned long *)rx->write = hwtstamps->hwtstamp.tv64;
	rx->write += MGC_HDR_TSTAMP_SIZE;
	memcpy(rx->write, skb_mac_header(skb),
		(pktlen > MGC_SNAPLEN) ? MGC_SNAPLEN : (int)pktlen);
	ring_write_next(rx, MGC_SNAPLEN);

/*
	ethhdr_len = (unsigned short)skb->mac_len;
	data_len = (unsigned short)skb->len;
	pr_info("skb->mac_len: %u, skb->len: %u, hwtstamps->hwtstamp.tv64: %lld\n",
		ethhdr_len, data_len, hwtstamps->hwtstamp.tv64);
	pr_info("skb_mac_header(skb): %p, skb->data: %p\n",
		skb_mac_header(skb), skb->data);
	printk("MAC: %02X %02X %02X %02X\n",
		*(unsigned char *)skb_mac_header(skb),
		*((unsigned char *)skb_mac_header(skb) + 1),
		*((unsigned char *)skb_mac_header(skb) + 2),
		*((unsigned char *)skb_mac_header(skb) + 3));
	printk("DATA: %02X %02X %02X %02X\n",
		*(unsigned char *)skb->data,
		*((unsigned char *)skb->data + 1),
		*((unsigned char *)skb->data + 2),
		*((unsigned char *)skb->data + 3));
*/

	kfree_skb(skb);
	*pskb = NULL;

	return res;
}

