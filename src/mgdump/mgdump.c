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
static int hwtstamp_config_set(const char *ifname, const int tx_type, const int rx_filter)
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

static int hwtstamp_config_get(const char *ifname, int *tx_type, int *rx_filter)
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


int main(int argc, char **argv)
{
	int tx_type_save, rx_filter_save;
	char ifname[IFNAMSIZ];
	int ret;

	if (argc != 2 || (strlen(argv[1]) >= IFNAMSIZ)) {
		usage();
		return 2;
	}
	strcpy(ifname, argv[1]);

	// config get
	ret = hwtstamp_config_get(ifname, &tx_type_save, &rx_filter_save);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_get() get error\n");
		return 1;
	}

	// config set
	ret = hwtstamp_config_set(ifname, TX_TYPE_OFF, RX_FILTER_ALL);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_set() save error\n");
		return 1;
	}
		
//err:
	// config restore
	ret = hwtstamp_config_set(ifname, tx_type_save, rx_filter_save);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_set() restore error\n");
		return 1;
	}

	return 0;
}

