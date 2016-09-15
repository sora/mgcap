#ifndef _MGCAP_RING_H_
#define _MGCAP_RING_H_

/*
 * simple ring buffer
 * by Yohei Kuga <sora@haeena.net>
 */

/*
 * memory management
 *
 *  a         b             c           d       e
 *  |---------+-------------+-----------|-------|
 *
 *  a: ring->start (start address)
 *  b: ring->read (read pointer)
 *  c: ring->write (write pointer)
 *  d: ring->end (end address)
 *  e: malloc size (ring->end + MAX_PKT_SIZE * (NBULK + 1))
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
#define MAX_PKT_SIZE              96
#define RING_SIZE                 (1<<19)        // 2^n
#define NBULK_PKT                 1
#define RING_ALMOST_FULL          (MAX_PKT_SIZE*2)

//#define ALIGN(x,a) __ALIGN_MASK(x,(typeof(x))(a)-1)
//#define __ALIGN_MASK(x,mask) (((x)+(mask))&~(mask))

struct mgc_ring {
	uint8_t *write;
	uint8_t *read;
	uint8_t *start;
	uint8_t *end;
	uint32_t mask;
};

static inline uint32_t ring_count(const struct mgc_ring *r)
{
	return ((r->write - r->read) & r->mask);
}

static inline uint32_t ring_free_count(const struct mgc_ring *r)
{
	return ((r->read - r->write - 1) & r->mask);
}

static inline bool ring_empty(const struct mgc_ring *r)
{
	return !!(r->read == r->write);
}

static inline bool ring_almost_full(const struct mgc_ring *r)
{
	return !!(ring_free_count(r) < RING_ALMOST_FULL);
}

static inline void ring_write_next(struct mgc_ring *r, uint32_t size)
{
	r->write += size;
	if (r->write > r->end) {
		r->write = r->start;
	}
}

static inline void ring_read_next(struct mgc_ring *r, uint32_t size)
{
	r->read += size;
	if (r->read > r->end) {
		r->read = r->start;
	}
}


int ring_init(struct mgc_ring *);
void ring_exit(struct mgc_ring *);

#endif /* _MGCAP_RING_H_ */

