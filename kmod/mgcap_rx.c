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

	struct mgc_dev *dev = rcu_dereference(skb->dev->rx_handler_data);
	struct mgc_ring *rxbuf = &dev->rx[0].buf;

	struct skb_shared_hwtstamps *hwtstamps = skb_hwtstamps(skb);
	unsigned short pktlen = skb->mac_len + skb->len;

	*(unsigned short *)rxbuf->write = pktlen;
	*(unsigned long *)(rxbuf->write + MGC_HDR_PKTLEN_SIZE) = hwtstamps->hwtstamp.tv64;
	memcpy((rxbuf->write + MGC_HDR_PKTLEN_SIZE + MGC_HDR_TSTAMP_SIZE), skb_mac_header(skb),
		(pktlen > MGC_SNAPLEN) ? MGC_SNAPLEN : (int)pktlen);

	ring_write_next(rxbuf, MGC_HDR_PKTLEN_SIZE + MGC_HDR_TSTAMP_SIZE + MGC_SNAPLEN);

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

