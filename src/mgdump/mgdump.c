#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/if.h>

#include "hwtstamp_config.h"

#define ETH_HDRLEN       (14)
#define MGC_PKTLEN       (128)

#define INTERVAL_100MSEC    100000

static void usage(void)
{
	fputs("Usage: COMMAND <if_name> <out_file>\n", stderr);
}

int main(int argc, char **argv)
{
	char ifname[IFNAMSIZ];
	
	char ibuf[128*32], obuf[128*32];
	int ret, i, j;
	unsigned short pktlen;
	unsigned long tstamp;
	int olen;

	int fd, count;
	char *p;


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
		count = read(fd, &ibuf[0], sizeof(ibuf));
		printf("count=%d\n", count);
		if (count < 1) {
			usleep(INTERVAL_100MSEC);
			continue;
		} else if ((count & 127) != 0) {
			printf("souteigai: count=%d\n", count);
			exit(EXIT_FAILURE);
		}

		p = &ibuf[0];

		for (i = 0; i < (count << 7); i++) {
			printf("count=%d, i=%d\n", count, i);
			pktlen = *(unsigned short *)&p[0];
			tstamp = *(unsigned long *)&p[2];
			printf("pktlen=%u, tstamp=%lu\n", pktlen, tstamp);

			if ((pktlen < 40) || (pktlen > 9014)) {
				printf("format size: pktlen %X\n", pktlen);
				exit(EXIT_FAILURE);
			}

			sprintf(obuf, "%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X %02X%02X",
					p[10], p[11], p[12], p[13], p[14], p[15],
					p[16], p[17], p[18], p[19], p[20], p[21],
					p[22], p[23]);
			olen = strlen(obuf);
			for (j = ETH_HDRLEN+10; j < pktlen; j++) {
				sprintf(obuf + olen + ((j - (ETH_HDRLEN+10)) * 3), " %02X", p[j]);
			}
			strcat(obuf, "\n");
			ret = write(1, obuf, strlen(obuf));
			if (ret != strlen(obuf)) {
				printf("ret isn't strlen(obuf)\n");
				return 1;
			}
			p += 128;
		}
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

