#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

struct thdata {
	int cpu;
	int ncpus;
	sem_t ready;
};

int count_online_cpus(cpu_set_t *cpu_set)
{
	int ret, count = 0;

	ret = sched_getaffinity(0, sizeof(cpu_set_t), cpu_set);
	if (ret == 0) {
		count = CPU_COUNT(cpu_set);
	}

	return count;
}

void *rx_thread(void *arg)
{
	struct thdata *priv = (struct thdata *)arg;
	cpu_set_t target_cpu_set;
	pthread_t cur_thread;

	printf ("0: target_cpu=%d, cur_cpu=%d\n", priv->cpu, sched_getcpu());

	CPU_ZERO(&target_cpu_set);
	CPU_SET(priv->cpu, &target_cpu_set);
	cur_thread = pthread_self();

	// set this thread on target cpu core
	pthread_setaffinity_np(cur_thread, sizeof(cpu_set_t), &target_cpu_set);

	printf ("1: target_cpu=%d, cur_cpu=%d\n", priv->cpu, sched_getcpu());

	sleep(1);
	sem_post(&priv->ready);
	
	return NULL;
}

int main(void)
{
	struct thdata *thdata;
	pthread_t *rxth;

	cpu_set_t cpu_set;
	int ncpus, ret, i;

	// count number of online cpus
	CPU_ZERO(&cpu_set);
	ncpus = count_online_cpus(&cpu_set);
	printf("ncpus=%d\n", ncpus);
	
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
			thdata[i].ncpus = ncpus;
			sem_init(&thdata[i].ready, 0, 0);

			ret = pthread_create(&rxth[i], NULL, rx_thread, &thdata[i]);
			if (ret) {
				perror("pthread create");
				return 1;
			}
			sem_wait(&thdata[i].ready);
		}
	}

	// thread join
	for (i = 0; i < CPU_SETSIZE; i++) {
		if (CPU_ISSET(i, &cpu_set)) {
			pthread_join(rxth[i], NULL);
		}
	}

	free(thdata);
	free(rxth);
	
	return 0;
}

