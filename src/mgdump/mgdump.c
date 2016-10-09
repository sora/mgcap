#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/if.h>

#include "hwtstamp_config.h"

#define MGC_HDRLEN       (10)
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
			printf("numpkt: %d, i: %d\n", numpkt, i);
			printf("count=%d, i=%d\n", count, i);
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

