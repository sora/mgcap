#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if.h>

#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#define MGC_HDRLEN          10
#define MGC_SLOTLEN         128
#define INTERVAL_100USEC    100

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

struct thdata {
	int cpu;
	int ncpus;
	int fdi;
	sem_t ready;
	unsigned int stat_pktcount;
};

