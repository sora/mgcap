#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/if.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>

#include "hwtstamp_config.h"

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

