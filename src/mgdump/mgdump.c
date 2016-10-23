#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if.h>

#include "hwtstamp_config.h"

#define MGC_HDRLEN       (10)
#define ETH_HDRLEN       (14)
#define MGC_SNAPLEN      (128)

#define INTERVAL_100MSEC    100000


/* PCAP-NG header
 * from https://github.com/the-tcpdump-group/libpcap */

/* Section Header Block. */
struct section_header_block {
	uint32_t block_type;
	uint32_t total_length1;
	uint32_t byte_order_magic;
	uint16_t major_version;
	uint16_t minor_version;
	uint64_t section_length;
	uint32_t total_length2;
} __attribute__((packed));

/* Interface Description Block. */
struct interface_description_block {
	uint32_t block_type;
	uint32_t total_length1;
	uint16_t linktype;
	uint16_t reserved;
	uint32_t snaplen;
	uint16_t option_code_fcslen;
	uint16_t option_length_fcslen;
	uint32_t option_value_fcslen;
	uint16_t option_code_tsresol;
	uint16_t option_length_tsresol;
	uint32_t option_value_tsresol;
	uint16_t option_code_pad;
	uint16_t option_length_pad;
	uint32_t total_length2;
} __attribute__((packed));

/* Enhanced Packet Block. */
#define BT_EPB			0x00000006
struct enhanced_packet_block_head {
	uint32_t block_type;
	uint32_t total_length;
	uint32_t interface_id;
	uint32_t timestamp_high;
	uint32_t timestamp_low;
	uint32_t caplen;
	uint32_t origlen;
	/* followed by packet data, options, and trailer */
} __attribute__((packed));

struct enhanced_packet_block_tail {
	uint32_t total_length;
};

struct mgdump_statistics {
	uint32_t packet_count;
};


/* Global variables */
static int caught_signal = 0;

static struct mgdump_statistics mgdump_stat = {
	.packet_count = 0,
};

// SHB
#define BT_SHB                  0x0A0D0D0A
#define BYTE_ORDER_MAGIC        0x1A2B3C4D
#define PCAP_NG_VERSION_MAJOR	1
#define PCAP_NG_VERSION_MINOR	0
static struct section_header_block pcapng_shb_hdr = {
	.block_type       = BT_SHB,
	.total_length1    = sizeof(struct section_header_block),
	.byte_order_magic = BYTE_ORDER_MAGIC,
	.major_version    = PCAP_NG_VERSION_MAJOR,
	.minor_version    = PCAP_NG_VERSION_MINOR,
	.section_length   = 0xffffffffffffffff,
	.total_length2    = sizeof(struct section_header_block),
};

// IDB
#define BT_IDB            0x00000001
#define IF_TSRESOL        9         /* interface's time stamp resolution */
#define IF_FCSLEN         13        /* FCS length for this interface */
static struct interface_description_block pcapng_idb_hdr = {
	.block_type             = BT_IDB,
	.total_length1          = sizeof(struct interface_description_block),
	.linktype               = 0x01,
	.reserved               = 0,
	.snaplen                = 0xffff,
	.option_code_fcslen     = IF_FCSLEN,
	.option_length_fcslen   = 1,
	.option_value_fcslen    = 4,
	.option_code_tsresol    = IF_TSRESOL,
	.option_length_tsresol  = 1,
	.option_value_tsresol   = 9,
	.option_code_pad        = 0,
	.option_length_pad      = 0,
	.total_length2          = sizeof(struct interface_description_block),
};


static inline int pcapng_epb_memcpy(char *po, char *pi, int pktlen, uint64_t ts)
{
	size_t epb_head_size = sizeof(struct enhanced_packet_block_head);
	size_t epb_tail_size = sizeof(struct enhanced_packet_block_tail);
	struct enhanced_packet_block_head epb_head;
	struct enhanced_packet_block_tail epb_tail;
	uint32_t epb_len, pad;
	int copy_len;

	copy_len = (pktlen > 96) ? 96 : pktlen;

	pad = 4 - (copy_len % 4);
	if (pad == 4)
		pad = 0;

	epb_len = epb_head_size + epb_tail_size + copy_len + pad;

	//printf("epb_len=%d, snaplen=%d, pktlen=%d, copy_len=%d\n", epb_len, MGC_SNAPLEN, pktlen, copy_len);
	//printf("pad=%d, epb_head_size=%d, epb_tail_size=%d\n", pad, (int)epb_head_size, (int)epb_tail_size);

	// epb_head
	epb_head.block_type      = BT_EPB;
	epb_head.total_length    = epb_len;
	epb_head.interface_id    = 0;
	epb_head.timestamp_high  = (uint32_t)(ts >> 32);
	epb_head.timestamp_low   = (uint32_t)(ts & 32);
	epb_head.caplen          = copy_len;
	epb_head.origlen         = pktlen;

	// epb_tail
	epb_tail.total_length = epb_len;

	memcpy(po, &epb_head, epb_head_size);
	memcpy((po + epb_head_size), (pi + MGC_HDRLEN), (size_t)copy_len);
	memcpy((po + epb_head_size + (size_t)copy_len), &epb_tail, epb_tail_size);

	return epb_len;
}

/*
 * sig_handler
 * @sig:
 */
void sig_handler(int sig) {
	if (sig == SIGINT)
		caught_signal = 1;
}

/*
 *  set_signal
 *  @sig:
 */
void set_signal(int sig) {
	if (signal(sig, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Cannot set signal\n");
		exit(1);
	}
}

/*
 *  dump_stat
 *  
 */
void dump_stat(void) {
	printf("\n%u packets captured\n", mgdump_stat.packet_count);
}

/*
 * usage
 * 
 */
static void usage(void)
{
	fputs("Usage: COMMAND <if_name> <out_file>\n", stderr);
}

/*
 * main
 * 
 */
int main(int argc, char **argv)
{
	char ifname[IFNAMSIZ];
	
	char ibuf[MGC_SNAPLEN*1024];  // number of max input packets: 1024
	char obuf[2*MGC_SNAPLEN*1024];  // max output size: 256 KB
	unsigned short pktlen;
	unsigned long tstamp;

	int fdi, fdo, count, numpkt;
	char *pi, *po;
	int i;
	int copy_len;


	if (argc != 2 || (strlen(argv[1]) >= IFNAMSIZ)) {
		usage();
		return 2;
	}
	strcpy(ifname, argv[1]);

#if 0
	// config get
	ret = hwtstamp_config_save(ifname);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_save() error\n");
		return 1;
	}

	// config set
	ret = hwtstamp_config_set_hwtstamp(ifname);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_set_hwtstamp() error\n");
		return 1;
	}
#endif

	fdi = open("/dev/mgcap/lo", O_RDONLY);
	if (fdi < 0) {
		fprintf(stderr, "cannot open mgcap device\n");
		return 1;
	}

	fdo = open("output.pcap", O_CREAT|O_WRONLY|O_TRUNC, 0755);
	if (fdo < 0) {
		fprintf(stderr, "cannot open output file\n");
		return 1;
	}

	printf("mgdump: listening on lo\n");

	// BT_SHB
	count = write(fdo, &pcapng_shb_hdr, sizeof(struct section_header_block));
	if (count != sizeof(struct section_header_block)) {
		fprintf(stderr, "cannot write output file: BT_SHB\n");
		return 1;
	}

	// BT_IDB
	count = write(fdo, &pcapng_idb_hdr, sizeof(struct interface_description_block));
	if (count != sizeof(struct interface_description_block)) {
		fprintf(stderr, "cannot write output file: BT_IDB\n");
		return 1;
	}

	// signal handler
	set_signal(SIGINT);

	while (1) {
		if (caught_signal)
			break;

		count = read(fdi, &ibuf[0], sizeof(ibuf));
		//printf("count=%d\n", count);
		if (count < 1) {
			usleep(INTERVAL_100MSEC);
			continue;
		}

		numpkt = count >> 7;    // count / MGC_SNAPLEN
		if ((count & 127) != 0) {    // count % MGC_SNAPLEN
			printf("souteigai: count=%d\n", count);
			exit(EXIT_FAILURE);
		}

		pi = &ibuf[0];
		po = &obuf[0];
		for (i = 0; i < numpkt; i++) {
			pktlen = *(unsigned short *)&pi[0];
			tstamp = *(unsigned long *)&pi[2];
			//printf("pktlen=%u, tstamp=%lu\n", pktlen, tstamp);

			// debug
			if ((pktlen < 40) || (pktlen > 9014)) {
				printf("format size: pktlen %X\n", pktlen);
				exit(EXIT_FAILURE);
			}
			copy_len = pcapng_epb_memcpy(po, pi, pktlen, tstamp);
			pi += MGC_SNAPLEN;
			po += copy_len;
		}

		// dump to file
		count = write(fdo, obuf, count);

		mgdump_stat.packet_count += numpkt;
	}
		
#if 0
//err:
	// config restore
	ret = hwtstamp_config_restore(ifname);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_restore() error\n");
		return 1;
	}
#endif

	close(fdi);
	close(fdo);

	dump_stat();

	return 0;
}

