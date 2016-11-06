#include "mgdump.h"

/* Global variables */
static int caught_signal = 0;

//static struct mgdump_statistics mgdump_stat = {
//	.packet_count = 0,
//};

// pcap global header
#define PCAP_MAGIC         (0xa1b2c3d4)
#define PCAP_MAGIC_NS      (0xa1b23c4d)
#define PCAP_VERSION_MAJOR (0x2)
#define PCAP_VERSION_MINOR (0x4)
#define PCAP_SNAPLEN       (0xFFFF)
#define PCAP_NETWORK       (0x1)      // linktype_ethernet
static struct pcap_hdr_s pcap_ghdr = {
	.magic_number     = PCAP_MAGIC_NS,
	.version_major    = PCAP_VERSION_MAJOR,
	.version_minor    = PCAP_VERSION_MINOR,
	.thiszone         = 0,
	.sigfigs          = 0,
	.snaplen          = PCAP_SNAPLEN,
	.network          = PCAP_NETWORK,
};

#define _NSEC_PER_SEC 1000000000
static inline int pcap_memcpy(char *po, char *pi, int pktlen, uint64_t ts)
{
	struct pcaprec_hdr_s pcaprec = {0};
	int copy_len;

	copy_len = (pktlen > 96) ? 96 : pktlen;

	// pcaprec header
	if (ts == 0) {
		pcaprec.ts_sec  = 0;
		pcaprec.ts_nsec = 0;
	} else {
		pcaprec.ts_sec  = (unsigned int)(ts / _NSEC_PER_SEC);
		pcaprec.ts_nsec = (unsigned int)(ts % _NSEC_PER_SEC);
	}
	pcaprec.incl_len = copy_len;
	pcaprec.orig_len = pktlen;

	memcpy(po, &pcaprec, sizeof(struct pcaprec_hdr_s));
	memcpy((po + sizeof(struct pcaprec_hdr_s)), (pi + MGC_HDRLEN), (size_t)copy_len);

	return (sizeof(struct pcaprec_hdr_s) + copy_len);
}


/*
 * sig_handler
 * @sig:
 */
void sig_handler(int sig) {
	if (sig == SIGINT)
		caught_signal = 1;
}

/*
 *  set_signal
 *  @sig:
 */
void set_signal(int sig) {
	if (signal(sig, sig_handler) == SIG_ERR) {
		fprintf(stderr, "Cannot set signal\n");
		exit(1);
	}
}

/*
 * count_online_cpus
 * 
 */
int count_online_cpus(cpu_set_t *cpu_set)
{
	int ret, count = 0;

	ret = sched_getaffinity(0, sizeof(cpu_set_t), cpu_set);
	if (ret == 0) {
		count = CPU_COUNT(cpu_set);
	}

	return count;
}

/*
 * rx_thread
 * 
 */
void *rx_thread(void *arg)
{
	struct thdata *priv = (struct thdata *)arg;
	cpu_set_t target_cpu_set;
	pthread_t cur_thread;

	char ibuf[MGC_SLOTLEN * 2048];  // number of max input packets: 1024
	char obuf[MGC_SLOTLEN * 2048 * 2];  // max output size: 256 KB
	char outfile_name[16];
	int i, fdo, count, numpkt, copy_len, copy_sum;
	char *pi, *po;

	unsigned short pktlen;
	unsigned long tstamp;

	// set this thread on target cpu core
	CPU_ZERO(&target_cpu_set);
	CPU_SET(priv->cpu, &target_cpu_set);
	cur_thread = pthread_self();
	pthread_setaffinity_np(cur_thread, sizeof(cpu_set_t), &target_cpu_set);
	printf("thread %d: start. cur_cpu=%d\n", priv->cpu, sched_getcpu());

	// output pcap file
	sprintf(outfile_name, "output%d.pcap", priv->cpu);
	fdo = open(outfile_name, O_CREAT|O_WRONLY|O_TRUNC, 0755);
	if (fdo < 0) {
		fprintf(stderr, "cannot open output file\n");
		return NULL;
	}

	// dump BT_SHB to pcap file
	count = write(fdo, &pcap_ghdr, sizeof(struct pcap_hdr_s));
	if (count != sizeof(struct pcap_hdr_s)) {
		fprintf(stderr, "cannot write output file: pcap_hdr_s\n");
		return NULL;
	}

	sem_post(&priv->ready);

	while (1) {
		if (caught_signal)
			break;
		
		count = read(priv->fdi, &ibuf[0], sizeof(ibuf));
		if (count < 1) {
			usleep(INTERVAL_100USEC);
			continue;
		}

		numpkt = count >> 7;    // count / MGC_SNAPLEN
		if ((count & 127) != 0) {    // count % MGC_SNAPLEN
			printf("souteigai: count=%d\n", count);
			break;
		}

		pi = &ibuf[0];
		po = &obuf[0];
		copy_sum = 0;
		for (i = 0; i < numpkt; i++) {
			pktlen = *(unsigned short *)&pi[0];
			tstamp = *(unsigned long *)&pi[2];

			// debug
			if ((pktlen < 40) || (pktlen > 9014)) {
				printf("format size: pktlen %X\n", pktlen);
				break;
			}
			copy_len = pcap_memcpy(po, pi, pktlen, tstamp);
			pi += MGC_SLOTLEN;
			po += copy_len;
			copy_sum += copy_len;

			++priv->stat_pktcount;
		}

		// dump to file
		count = write(fdo, obuf, copy_sum);

		//mgdump_stat.packet_count += numpkt;
	}
	
	printf("thread %d: finished.\n", priv->cpu);
	close(fdo);

	return NULL;
}

/*
 * usage
 * 
 */
static void usage(void)
{
	//fputs("Usage: COMMAND <if_name> <out_file>\n", stderr);
	fputs("Usage: COMMAND <if_name>\n", stderr);
}

/*
 * main
 * 
 */
int main(int argc, char **argv)
{
	struct thdata *thdata;
	pthread_t *rxth;

	cpu_set_t cpu_set;
	int ncpus, ret, i, fdi[0x40] = {};
	unsigned int sum, pre_sum;

	char ifname[11 + IFNAMSIZ];

	// count number of online cpus
	CPU_ZERO(&cpu_set);
	ncpus = count_online_cpus(&cpu_set);
	printf("ncpus=%d\n", ncpus);

	if (argc != 2 || (strlen(argv[1]) >= (11 + IFNAMSIZ))) {
		usage();
		return 2;
	}
	strcpy(ifname, argv[1]);

	// malloc pthread
	rxth = calloc(sizeof(pthread_t), ncpus);
	if (rxth == NULL) {
		perror("calloc() rxth");
		return 1;
	}

	// malloc thdata
	thdata = calloc(sizeof(struct thdata), ncpus);
	if (thdata == NULL) {
		perror("calloc() thdata");
		return 1;
	}

	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &cpu_set)) {
			fdi[i] = open(ifname, O_RDONLY);
			if (fdi < 0) {
				fprintf(stderr, "cannot open mgcap device\n");
				return 1;
			}
			printf("mgdump: listening on %s from cpu%d\n", ifname, i);

			thdata[i].cpu = i;
			thdata[i].fdi = fdi[i];
			thdata[i].ncpus = ncpus;
			thdata[i].stat_pktcount = 0;
			sem_init(&thdata[i].ready, 0, 0);

			ret = pthread_create(&rxth[i], NULL, rx_thread, &thdata[i]);
			if (ret) {
				perror("pthread create");
				return 1;
			}
			sem_wait(&thdata[i].ready);
		}
	}

	// set signal handler
	set_signal(SIGINT);

	pre_sum = 0;
	while (1) {
		if (caught_signal)
			break;

		sum = 0;
		for (i = 0; i < 20; i++) {
			if (CPU_ISSET(i, &cpu_set)) {
				sum += thdata[i].stat_pktcount;
				printf("%d:%u\t", i, thdata[i].stat_pktcount);
			}
		}
		//printf("recv packets: %u pps\n", sum);
		printf("sum:%u\tpps:%u\n", sum, (sum - pre_sum));
		pre_sum = sum;

		sleep(1);
	}

	// thread join
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &cpu_set)) {
			pthread_join(rxth[i], NULL);
		}
	}

	for (i = 0; i < 0x40; i++) {
		if (fdi[i])
			close(fdi[i]);
	}

	free(thdata);
	free(rxth);
	
	return 0;
}

