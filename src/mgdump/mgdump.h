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

/* PCAP header
 * from https://wiki.wireshark.org/Development/LibpcapFileFormat */

/* pcap v2.4 global header */
struct pcap_hdr_s {
	unsigned int   magic_number;   /* magic number */
	unsigned short version_major;  /* major version number */
	unsigned short version_minor;  /* minor version number */
	int            thiszone;       /* GMT to local correction */
	unsigned int   sigfigs;        /* accuracy of timestamps */
	unsigned int   snaplen;        /* max length of captured packets, in octets */
	unsigned int   network;        /* data link type */
} __attribute__((packed));

/* pcap v2.4 packet header */
struct pcaprec_hdr_s {
	unsigned int ts_sec;         /* timestamp seconds */
	union {
		unsigned int ts_usec;        /* timestamp microseconds */
		unsigned int ts_nsec;        /* timestamp nanoseconds */
	};
	unsigned int incl_len;       /* number of octets of packet saved in file */
	unsigned int orig_len;       /* actual length of packet */
} __attribute__((packed));

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

