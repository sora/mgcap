#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/if.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#define TX_TYPE_OFF     0
#define RX_FILTER_NONE  0
#define RX_FILTER_ALL   1

/* From linux/Documentation/networking/timestamping/hwtstamp_config.c */
int hwtstamp_config_set(const char *ifname, int tx_type, int rx_filter)
{
	struct hwtstamp_config config;
	int sock, ret = 0;
	struct ifreq ifr;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		ret = -1;
		goto out;
	}

	config.flags = 0;
	config.tx_type = tx_type;
	config.rx_filter = rx_filter;
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_data = (caddr_t)&config;

	if (ioctl(sock, SIOCSHWTSTAMP, &ifr)) {
		perror("ioctl");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

int hwtstamp_config_get(const char *ifname, int *tx_type, int *rx_filter)
{
	struct hwtstamp_config config;
	int sock, ret = 0;
	struct ifreq ifr;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		ret = -1;
		goto out;
	}

	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_data = (caddr_t)&config;

	if (ioctl(sock, SIOCGHWTSTAMP, &ifr)) {
		perror("ioctl");
		ret = -1;
		goto out;
	}

	*tx_type = config.tx_type;
	*rx_filter = config.rx_filter;

out:
	return ret;
}

static void usage(void)
{
	fputs("Usage: COMMAND <if_name> 1 or 0\n", stderr);
}


int main(int argc, char **argv)
{
	char ifname[IFNAMSIZ];
	int ret, cmd;

	if (argc != 3 || (strlen(argv[1]) >= IFNAMSIZ)) {
		usage();
		return 2;
	}
	strcpy(ifname, argv[1]);

	cmd = atoi(argv[2]);
	if (cmd != 0 && cmd != 1) {
		usage();
		return 2;
	}

	switch (cmd) {
		// OFF
		case 0:
			ret = hwtstamp_config_set(ifname, TX_TYPE_OFF, RX_FILTER_NONE);
			if (ret < 0) {
				fprintf(stderr, "hwtstamp_config_set() error\n");
				return 1;
			}
			break;
		// ON
		case 1:
			ret = hwtstamp_config_set(ifname, TX_TYPE_OFF, RX_FILTER_ALL);
			if (ret < 0) {
				fprintf(stderr, "hwtstamp_config_set() error\n");
				return 1;
			}
			break;
	}
	
	return 0;
}

