#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/if.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#define TX_TYPE_OFF    0
#define RX_FILTER_ALL  1

static void usage(void)
{
	fputs("Usage: COMMAND <if_name>\n", stderr);
}

/* From linux/Documentation/networking/timestamping/hwtstamp_config.c */
static int config_rx_filter_all(const char *name)
{
	struct hwtstamp_config config;
	struct ifreq ifr;
	int ret = 0;
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		ret = -1;
		goto out;
	}

	config.flags = 0;
	config.tx_type = TX_TYPE_OFF;
	config.rx_filter = RX_FILTER_ALL;
	strcpy(ifr.ifr_name, name);
	ifr.ifr_data = (caddr_t)&config;

	if (ioctl(sock, SIOCSHWTSTAMP, &ifr)) {
		perror("ioctl");
		ret = -1;
		goto out;
	}

out:
	return ret;
}


int main(int argc, char **argv)
{
	int ret;

	if (argc != 2 || (strlen(argv[1]) >= IFNAMSIZ)) {
		usage();
		return 2;
	}

	ret = config_rx_filter_all(argv[1]);
	if (ret < 0) {
		fprintf(stderr, "NIC config error\n");
		return 1;
	}
		
	return 0;
}

