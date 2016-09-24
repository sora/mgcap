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

//	struct mgc_dev *dev = rcu_dereference(skb->dev->rx_handler_data);
	struct mgc_ring *rxbuf = &mgc->rxrings[smp_processor_id()].buf;

	struct skb_shared_hwtstamps *hwtstamps = skb_hwtstamps(skb);
	uint16_t pktlen = skb->mac_len + skb->len;
	size_t copylen = (pktlen > MGC_PKT_SNAPLEN) ? MGC_PKT_SNAPLEN : (size_t)pktlen;

	*(uint16_t *)rxbuf->write = pktlen;
	*(uint64_t *)(rxbuf->write + MGC_HDR_PKTLEN_SIZE) = hwtstamps->hwtstamp.tv64;
	memcpy((rxbuf->write + MGC_HDR_SIZE), skb_mac_header(skb), copylen);

	ring_write_next(rxbuf, MGC_DATASLOT_SIZE);

	kfree_skb(skb);
	*pskb = NULL;

	return res;
}

