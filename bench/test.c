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

#define FILENAME "/dev/bench0"

#define DEF_NUM_THREAD 4
#define MAX_CORE	32
#define DEL_LOOP	1

struct data {
	void *address;
	uint64_t wt; 
	int cpuid;
};

long loop = 1;
int sfd = 0;

uint64_t wts[MAX_CORE] = {0,};
int cnts[MAX_CORE] = {0,};
void *saddr;

void unittest(struct data *cont) {
//	struct timeval start, end;
//	int fd;
	
/*	fd = open( FILENAME, O_RDWR);
	if( fd < 0 ) {
		perror("device open error");
		exit(-1);
	}*/
//	gettimeofday(&start,NULL);

	ioctl(sfd, 0, cont);

//	gettimeofday(&end,NULL);
//	cont->wt = (end.tv_usec-start.tv_usec) + 100000 * (end.tv_sec-start.tv_sec) ;

//	close(fd);
};

void *thread_func(void *param)
{
	struct data cont;
	unsigned long mask;
	int i, j, idx;
	char *userdata;

	userdata = aligned_alloc(4096, 4096 * 2);

	mask = 1 << ((unsigned long)param % MAX_CORE); 
	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), (cpu_set_t *)&mask) <0) perror("pthread_setaffinity_np"); 

	for(i = 0 ; i < loop ;i++) {
		for(j = 0 ; j < 4096 ; j++) {
			userdata[j] = i%256;
			userdata[j+4096] = (i+1)%256;
		}
		cont.address = (void *) userdata;
		cont.wt = 0;
		unittest(&cont);
		idx = cont.cpuid;
		saddr = cont.address;
		if(idx < MAX_CORE) {
			wts[idx] = (wts[idx] * cnts[idx] + cont.wt) / (cnts[idx] + 1);
			cnts[idx]++;
		} else {
			printf("[%3ld] to %llx takes %ld ns\n", cont.cpuid, cont.address, cont.wt );
		}
	}

	free(userdata);
};

int main(int argc, char **argv) {
	int nt = DEF_NUM_THREAD;
	pthread_t *threads;
	long i;

	if(argc > 1) {
		nt = atoi(argv[1]);
	}
	if(argc > 2) {
		loop = atoi(argv[2]);
	}	

	printf("num threads = %d , loops = %d\n", nt, loop);

	threads = (pthread_t *)malloc(nt * sizeof(pthread_t));

	sfd = open( FILENAME, O_RDWR);
	if( sfd < 0 ) {
		perror("device open error");
		exit(-1);
	}

	for(i = 0 ; i < nt ;i++) {
		if(pthread_create(&threads[i], NULL, thread_func, (void *)i ) != 0 )
			perror("pthread_create");
	}

	for(i = 0 ; i < nt ;i++) {
		pthread_join(threads[i], NULL);
	}

	printf("to %lx (pfn = %ld , %d, %d)\n", saddr, (unsigned long)saddr/4096, ((unsigned long)saddr/4096) % 8, (unsigned long)saddr % 36 );
	for(i = 0 ; i < MAX_CORE;i++) {
		printf("[%2d]%8ld",i,wts[i],cnts[i]);
		if(i % 8 == 7) printf("\n");
		else printf("    ");
	}

	close(sfd);
	return 0;
}
