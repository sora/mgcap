#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

struct thdata {
	int cpu;
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
	printf ("cpu%d\n", priv->cpu);

	sleep(3);

	sem_post(&priv->ready);
	
	return NULL;
}

#if 0
int stick_this_thread_to_core(int core_id) {
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (core_id < 0 || core_id >= num_cores)
		return EINVAL;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);

	pthread_t current_thread = pthread_self();    

	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}
#endif

int main(void)
{
	pthread_t rxth0, rxth1;
	cpu_set_t cpu_set;
	int ncpus, ret;
	struct thdata th0, th1;

	// initialize
	CPU_ZERO(&cpu_set);
	ncpus = count_online_cpus(&cpu_set);
	
	sem_init(&th0.ready, 0, 0);
	sem_init(&th1.ready, 0, 0);

	printf("ncpus=%d\n", ncpus);

//	for (i = 0; i < CPU_SETSIZE; i++) {
//		if (CPU_ISSET(i, &cpu_set)) {
			th0.cpu = 1;
			ret = pthread_create(&rxth0, NULL, rx_thread, &th0);
			if (ret) {
				perror("pthread create");
				return 1;
			}
			sem_wait(&th0.ready);

			th1.cpu = 2;
			ret = pthread_create(&rxth1, NULL, rx_thread, &th1);
			if (ret) {
				perror("pthread create");
				return 1;
			}
			sem_wait(&th1.ready);

//		}
//	}

	pthread_join(rxth0, NULL);
	pthread_join(rxth1, NULL);

	return 0;
}

