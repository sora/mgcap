#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <linux/if.h>

#include "hwtstamp_config.h"

static void usage(void)
{
	fputs("Usage: COMMAND <if_name>\n", stderr);
}

int main(int argc, char **argv)
{
	char ifname[IFNAMSIZ];
	int ret;

	if (argc != 2 || (strlen(argv[1]) >= IFNAMSIZ)) {
		usage();
		return 2;
	}
	strcpy(ifname, argv[1]);

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
		
//err:
	// config restore
	ret = hwtstamp_config_restore(ifname);
	if (ret < 0) {
		fprintf(stderr, "hwtstamp_config_restore() error\n");
		return 1;
	}

	return 0;
}

