#ifndef _MGCAP_RING_H_
#define _MGCAP_RING_H_

/*
 * simple ring buffer
 * by Yohei Kuga <sora@haeena.net>
 */

/*
 * memory management
 *
 *  a         b             c           d
 *  |---------+-------------+-----------|
 *
 *  a: ring->start (start address)
 *  b: ring->read (read pointer)
 *  c: ring->write (write pointer)
 *  d: ring->end (end address)
 *
 *  ring->mask := ring->size - 1
 */

/*
 * Packet format
 * 0                      16 (bit)
 * +-----------------------+
 * |       SKB_LEN         |
 * +-----------------------+
 * |                       |
 * |      Timestamp        |
 * |                       |
 * |                       |
 * +-----------------------+
 * |                       |
 * |    Ethernet frame     |
 * |                       |
 * |~~~~~~~~~~~~~~~~~~~~~~~|
 * +-----------------------+
 */

#define MGC_HDR_PKTLEN_SIZE       2
#define MGC_HDR_TSTAMP_SIZE       8
#define MGC_HDR_SIZE              MGC_HDR_PKTLEN_SIZE + MGC_HDR_TSTAMP_SIZE
#define MGC_PKT_SNAPLEN           96
#define MGC_DATASLOT_SIZE         128
#define RING_SIZE                 (MGC_DATASLOT_SIZE * 4096 * 8)       // 2^n
#define NBULK_PKT                 1

struct mgc_ring {
	uint8_t *p;        /* malloc pointer */
	uint8_t *start;    /* start address */
	uint8_t *write;    /* write pointer */
	uint8_t *read;     /* read pointer */
	uint8_t *end;      /* end address */
	uint32_t mask;     /* bit mask of ring buffer */
};

static inline uint32_t ring_count(const struct mgc_ring *r)
{
	return ((r->write - r->read) & r->mask);
}

static inline uint32_t ring_count_end(const struct mgc_ring *r)
{
	uint32_t readable_len, ring_count;
	const uint8_t *wr = r->write;
	const uint8_t *rd = r->read;

	readable_len = r->end - rd;
	ring_count = (wr - rd) & r->mask;

	return (ring_count > readable_len) ? readable_len : ring_count;
}

static inline uint32_t ring_free_count(const struct mgc_ring *r)
{
	return ((r->read - r->write - 1) & r->mask);
}

static inline bool ring_empty(const struct mgc_ring *r)
{
	return !!(r->read == r->write);
}

static inline bool ring_full(const struct mgc_ring *r)
{
	return !!(ring_free_count(r) == MGC_DATASLOT_SIZE);
}

static inline void ring_write_next(struct mgc_ring *r, size_t size)
{
	uint8_t *wr = r->write;

	wr += size;
	if (wr >= r->end) {
		wr = r->start;
	}
	r->write = wr;
}

static inline void ring_read_next(struct mgc_ring *r, size_t size)
{
	uint8_t *rd = r->read;

	rd += size;
	if (rd >= r->end) {
		rd = r->start;
	}
	r->read = rd;
}

int mgc_ring_malloc(struct mgc_ring *, int cpu);

#endif /* _MGCAP_RING_H_ */

