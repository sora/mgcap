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

#define MGC_SLOTLEN         128
#define INTERVAL_100USEC    100

/* Global variables */
static int caught_signal = 0;

struct thdata {
	int cpu;
	int ncpus;
	int fdi;
	sem_t ready;
	unsigned int stat_pktcount;
};

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
	int i, count, numpkt;
	char *pi;

	unsigned short pktlen;
	//unsigned long tstamp;

	CPU_ZERO(&target_cpu_set);
	CPU_SET(priv->cpu, &target_cpu_set);
	cur_thread = pthread_self();

	// set this thread on target cpu core
	pthread_setaffinity_np(cur_thread, sizeof(cpu_set_t), &target_cpu_set);
	printf("thread %d: start. cur_cpu=%d\n", priv->cpu, sched_getcpu());

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
		for (i = 0; i < numpkt; i++) {
			pktlen = *(unsigned short *)&pi[0];
			//tstamp = *(unsigned long *)&pi[2];

			// debug
			if ((pktlen < 40) || (pktlen > 9014)) {
				printf("format size: pktlen %X\n", pktlen);
				break;
			}
			//copy_len = pcapng_epb_memcpy(po, pi, pktlen, tstamp);
			pi += MGC_SLOTLEN;
			++priv->stat_pktcount;
		}

		// dump to file
		//count = write(fdo, obuf, copy_sum);

		//mgdump_stat.packet_count += numpkt;
	}
	
	printf("thread %d: finished.\n", priv->cpu);
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
	int ncpus, ret, i, fdi;
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

	fdi = open(ifname, O_RDONLY);
	if (fdi < 0) {
		fprintf(stderr, "cannot open mgcap device\n");
		return 1;
	}
	printf("rxcount: listening on %s\n", ifname);
	
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
			thdata[i].cpu = i;
			thdata[i].fdi = fdi;
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

	close(fdi);

	free(thdata);
	free(rxth);
	
	return 0;
}

