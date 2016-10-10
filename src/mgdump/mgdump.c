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

#include <sys/socket.h>
#include <linux/if.h>

#include "hwtstamp_config.h"

#define MGC_HDRLEN       (10)
#define ETH_HDRLEN       (14)
#define MGC_PKTLEN       (128)

#define INTERVAL_100MSEC    100000

/* PCAP-NG header
 * from https://github.com/the-tcpdump-group/libpcap */

/* Section Header Block. */
#define BT_SHB			0x0A0D0D0A
#define BYTE_ORDER_MAGIC	0x1A2B3C4D
struct section_header_block {
	uint32_t	block_type;
	uint32_t	total_length1;
	uint32_t	byte_order_magic;
	uint16_t	major_version;
	uint16_t	minor_version;
	uint64_t	section_length;
	uint32_t	total_length2;
} __attribute__((packed));

/* Interface Description Block. */
#define BT_IDB			0x00000001

#define IF_TSRESOL	9	/* interface's time stamp resolution */
#define IF_FCSLEN	13	/* FCS length for this interface */
struct interface_description_block {
	uint32_t	block_type;
	uint32_t	total_length1;
	uint16_t	linktype;
	uint16_t	reserved;
	uint32_t	snaplen;
	uint16_t	option_code_fcslen;
	uint16_t	option_length_fcslen;
	uint32_t	option_value_fcslen;
	uint16_t	option_code_tsresol;
	uint16_t	option_length_tsresol;
	uint32_t	option_value_tsresol;
	uint16_t	option_code_pad;
	uint16_t	option_length_pad;
	uint32_t	total_length2;
} __attribute__((packed));

/* Enhanced Packet Block. */
#define BT_EPB			0x00000006
struct enhanced_packet_block {
	uint32_t	block_type;
	uint32_t	total_length;
	uint32_t	interface_id;
	uint32_t	timestamp_high;
	uint32_t	timestamp_low;
	uint32_t	caplen;
	uint32_t	origlen;
	/* followed by packet data, options, and trailer */
} __attribute__((packed));

struct enhanced_packet_block_tail {
	uint32_t	total_length;
};


static struct section_header_block pcapng_shb_hdr = {
	.block_type       = 0x0a0d0d0a,
	.total_length1    = 0x1c000000,
	.byte_order_magic = 0x4d3c2b1a,
	.major_version    = 0x0100,
	.minor_version    = 0x0100,
	.section_length   = 0xffffffffffffffff,
	.total_length2    = 0x1c000000,
};

static struct interface_description_block pcapng_idb_hdr = {
	.block_type             = 0x01000000,
	.total_length1          = 0x28000000,
	.linktype               = 0x0100,
	.reserved               = 0x0000,
	.snaplen                = 0x60000000,
	.option_code_fcslen     = 0x0d00,
	.option_length_fcslen   = 0x0100,
	.option_value_fcslen    = 0x04000000,
	.option_code_tsresol    = 0x0900,
	.option_length_tsresol  = 0x0100,
	.option_value_tsresol   = 0x09000000,
	.option_code_pad        = 0x0000,
	.option_length_pad      = 0x0000,
	.total_length2          = 0x28000000,
};


static void usage(void)
{
	fputs("Usage: COMMAND <if_name> <out_file>\n", stderr);
}

int main(int argc, char **argv)
{
	char ifname[IFNAMSIZ];
	
	char ibuf[128*1024];  // number of max input packets: 1024
	char obuf[2*128*1024];  // max output size: 256 KB
	int i;
	unsigned short pktlen;
	unsigned long tstamp;

	int fdi, fdo, count, numpkt;
	char *pi, *po;


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

	fdo = open("output.pkt", O_CREAT|O_WRONLY|O_TRUNC, 0755);
	if (fdo < 0) {
		fprintf(stderr, "cannot open output file\n");
		return 1;
	}

	while (1) {
		count = read(fdi, &ibuf[0], sizeof(ibuf));
		printf("count=%d\n", count);
		if (count < 1) {
			usleep(INTERVAL_100MSEC);
			continue;
		}

		numpkt = count >> 7;
		if ((count & 127) != 0) {
			printf("souteigai: count=%d\n", count);
			exit(EXIT_FAILURE);
		}

		pi = &ibuf[0];
		po = &obuf[0];
		for (i = 0; i < numpkt; i++) {
			pktlen = *(unsigned short *)&pi[0];
			tstamp = *(unsigned long *)&pi[2];
			printf("pktlen=%u, tstamp=%lu\n", pktlen, tstamp);

			// debug
			if ((pktlen < 40) || (pktlen > 9014)) {
				printf("format size: pktlen %X\n", pktlen);
				exit(EXIT_FAILURE);
			}
			memcpy(po, pi, (MGC_HDRLEN + pktlen));
			pi += 128;
			po += MGC_HDRLEN + pktlen;
		}

		// dump to file
		count = write(fdo, obuf, count);
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

	return 0;
}

