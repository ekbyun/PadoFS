#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <errno.h>

#define FILENAME "/dev/bench0"

#define DEF_NUM_THREAD 1
#define MAX_CORE	64

struct data {
	void *address;
	uint64_t wt; 
	int cpuid;
};

long loop = 1;
int sfd = 0;

uint64_t wts[MAX_CORE] = {0,};
int cnts[MAX_CORE] = {0,};
cpu_set_t masks[MAX_CORE];
int page_size = 4096;

void *thread_func(void *param)
{
	struct data cont;
	int i, j, idx, ret;
	long affi;
	char *userdata;
	int fd = sfd;
	void *kaddr;
	cpu_set_t mask;
	
	affi = (long)param;
	CPU_ZERO(&mask);
	CPU_SET(affi, &mask);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	if (ret<0) perror("pthread_setaffinity_np"); 

#if defined (OPEN_PER_THREAD) || defined (OPEN_PER_LOOP)
	fd = open( FILENAME, O_RDWR);
	if( fd < 0 ) perror("device open error");
#endif

	userdata = aligned_alloc(page_size, page_size * 2);

	for(i = 0 ; i < loop ;i++) {
		for(j = 0 ; j < page_size ; j++) {
			userdata[j] = i%page_size;
			userdata[j+page_size] = (i+1)%256;
		}
		cont.address = (void *) userdata;

		ioctl(sfd, 0, &cont);
	
		idx = cont.cpuid;
		kaddr = cont.address;
		if(idx < MAX_CORE) {
			wts[idx] = (wts[idx] * cnts[idx] + cont.wt) / (cnts[idx] + 1);
			cnts[idx]++;
		} else {
			fprintf(stderr,"invalid CPU number is returned from ioctl : %d\n", cont.cpuid );
		}
#ifdef OPEN_PER_LOOP
		close(fd);
		fd = open( FILENAME, O_RDWR);
		if( fd < 0 ) perror("device open error");
#endif
	}

#if defined (OPEN_PER_THREAD) || defined (OPEN_PER_LOOP)
	close(fd);
#endif

	free(userdata);

	printf("[%d] to %lx (pfn= %ld ,%%34= %d ,%%36= %d ,%%38= %d ) takes %ld\n", affi, kaddr, (unsigned long)kaddr/page_size, ((unsigned long)kaddr/64) % 34, ((unsigned long)kaddr/64) % 36, ((unsigned long)kaddr/64) % 38, wts[affi]);
};

/* usage : ./test <num thread> <cpu number offset> <loop>
 */
int main(int argc, char **argv) {
	int nt = DEF_NUM_THREAD;
	int cpuoffset = 0;
	pthread_t *threads;
	int i;

	if(argc > 1) {
		nt = atoi(argv[1]);
	}
	if(argc > 2) {
		cpuoffset = atoi(argv[2]);
	}
	if(argc > 3) {
		loop = atoi(argv[3]);
	}	

	printf("num threads = %d , offset = %d , loops = %d\n", nt, cpuoffset, loop);
	page_size = getpagesize();

	threads = (pthread_t *)malloc(nt * sizeof(pthread_t));

	sfd = open( FILENAME, O_RDWR);
	if( sfd < 0 ) {
		perror("device open error");
		exit(-1);
	}

	for(i = cpuoffset ; i < nt + cpuoffset ;i++) {
		if(pthread_create(&threads[i-cpuoffset], NULL, thread_func, (void *)(long)(i%MAX_CORE) ) != 0 )
			perror("pthread_create");
	}

	for(i = 0 ; i < nt ;i++) {
		pthread_join(threads[i], NULL);
		if( cnts[(i+cpuoffset)%MAX_CORE] == 0 ) printf("!!! affinity error!! cpuid=%d\n",i+cpuoffset );
	}


#ifdef PRINT_TIME_PER_CPU
	for(i = 0 ; i < MAX_CORE;i++) {
		printf("[%2d]%8ld",i,wts[i],cnts[i]);
		if(i % 8 == 7) printf("\n");
		else printf("    ");
	}
#endif

	close(sfd);
	return 0;
}
