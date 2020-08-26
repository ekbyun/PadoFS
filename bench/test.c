#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>

#define FILENAME "/dev/bench0"

#define NUM_THREAD	4

struct data {
	uint16_t cpuid;
	uint64_t address;
	long wt; 
	void *userpage;
};


void unittest(struct data *cont) {
	struct timeval start, end;
	int fd, ret;
	
	fd = open( FILENAME, O_RDWR);
	if( fd < 0 ) {
		perror("device open error");
		exit(-1);
	}
	gettimeofday(&start,NULL);

	ret = ioctl(fd, 0, cont);
	
	//printf("ioctl ret = %d, user cpuid= %d\n",ret, sched_getcpu() );
	//cont->address = 0x2345
	//cont->cpuid = sched_getcpu();

	gettimeofday(&end,NULL);

	close(fd);
	cont->wt = (end.tv_usec-start.tv_usec) + 100000 * (end.tv_sec-start.tv_sec) ;
};

void *thread_func(void *param)
{
	struct data cont;
	unsigned long mask;
	int i, j, affi;
	uint8_t *userdata;

	userdata = malloc(4096);

	affi = *((int *)param);
	mask = 1 << affi;

	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), (cpu_set_t *)&mask) <0) perror("pthread_setaffinity_np"); 

	for(i = 0 ; i < 1 ;i++) {
		for(j = 0 ; j < 4096 ; j++) userdata[j] = i%256;
	//	cont.cpuid = i + 5;
	//	cont.address = i + 0x1230;
		cont.userpage = userdata;
		unittest(&cont);	
		printf("%d\t%llx\t%ld\t%d\t%d\n",cont.cpuid,cont.address,cont.wt, cont.address - (unsigned long int)userdata, cont.address % 4096 );
	}

	free(userdata);
};

int main(int argc, char **argv) {
	pthread_t threads[NUM_THREAD];
	int affi[NUM_THREAD];
	int i;

	for(i = 0 ; i < NUM_THREAD ;i++) {
		affi[i] = i;
		if(pthread_create(&threads[i], NULL, thread_func, &affi[i]) != 0 )
			perror("pthread_create");
	}

	for(i = 0 ; i < NUM_THREAD ;i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
