#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <linux/if.h>

#include "hwtstamp_config.h"

#define ETH_HDRLEN       (14)
#define MGC_PKTLEN       (128)

static void usage(void)
{
	fputs("Usage: COMMAND <if_name>\n", stderr);
}

int main(int argc, char **argv)
{
	char ifname[IFNAMSIZ];
	int ret;
	
	char ibuf[2000], obuf[6000];
	int fd, i;
	unsigned short pktlen;
	unsigned long tstamp;
	int olen;


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

	fd = open("/dev/mgcap/lo", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "cannot open mgcap device\n");
		return 1;
	}
	while (1) {
		if (read(fd, ibuf, MGC_PKTLEN) <= 0)
			continue;
		pktlen = *(unsigned short *)&ibuf[0];
		tstamp = *(unsigned long *)&ibuf[2];
		printf("pktlen=%u, tstamp=%lu\n", pktlen, tstamp);

		if ((pktlen < 40) || (pktlen > 9014)) {
			printf("format size: pktlen %X\n", pktlen);
			return 1;
		}

		sprintf(obuf, "%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X %02X%02X",
				ibuf[10], ibuf[11], ibuf[12], ibuf[13], ibuf[14], ibuf[15],
				ibuf[16], ibuf[17], ibuf[18], ibuf[19], ibuf[20], ibuf[21],
				ibuf[22], ibuf[23]);
		olen = strlen(obuf);
		for (i = ETH_HDRLEN+10; i < pktlen; i++) {
			sprintf(obuf + olen + ((i - (ETH_HDRLEN+10)) * 3), " %02X", ibuf[i]);
		}
		strcat(obuf, "\n");
		ret = write (1, obuf, strlen(obuf));
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

	close(fd);

	return 0;
}

