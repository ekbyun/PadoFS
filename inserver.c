#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include"inserver.h"
#include"incont.h"
#include"uthash.h"

#define NUM_WORKER_THREADS  4

#define BACKUP_FILE_PATH	"/tmp/inserver_backup.bin"
#define DEFAULT_PORT		3495

void usage() {
	printf("USAGE: inodeserver [OPTION]... &\n");
	printf("  options : \n");
	printf("      -p [Port] : listening port number ranged between 20 and 30000. default = %d \n",DEFAULT_PORT);
	printf("      -f [filepath] : filename with path containing previous mapping contents.\n");
	printf("                      If not set, start with an emtpy map\n");
	printf("      -b [filepath] : filename with path to which mapping contents will be backuped when stopped.\n");
	printf("                      default = " BACKUP_FILE_PATH "\n");
	printf("      -h            : print this message\n");
	exit(1);
}

int main(int argc, char **argv) 
{
	int c;
	uint16_t port = DEFAULT_PORT;
	char outfilename[1014] = BACKUP_FILE_PATH;
	char infilename[1024] = {0};
	int infileexist = 0;

	while( (c = getopt(argc, argv, "p:f:b:h" ))!= -1) {
		switch(c) {
			case 'p':
				port = atoi(optarg);
				if( port < 20 || port > 30000 ) {
					usage();
				}
				break;
			case 'f':
				strncpy(infilename, optarg, strlen(optarg)+1);
				infileexist = 1;
				break;
			case 'b':
				strncpy(outfilename, optarg, strlen(optarg)+1);
				break;
			case 'h':
				usage();
		}
	}

	printf("Initializing PADOFS inode server.....\n");

	int fd;
	if( infileexist ) {
		printf("Restoring inodes from the file %s\n",infilename);
		//restore hash map from file 
		fd = open( infilename, O_RDONLY );
		
		if(fd == -1) {
			printf("The backup file does't exist. Starting with an empty map.\n"); 
		} else {
			// TODO :

			close(fd);
		}
	}

	printf("com = %d\n",WRITE);
	printf("com = %d\n",TRUNCATE);
	printf("com = %d\n",READ_LAYOUT);
	printf("com = %d\n",BACKUP_AND_STOP);

	//TODO : initialize threads, jobs queues, 
	

	int sockfd, clen;
	struct sockaddr_in sa,ca;
	unsigned char com;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(sockfd == -1) {
		perror("socket creation error:");
		exit(0);
	}

	bzero(&sa, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);

	if( bind( sockfd, (struct sockaddr *)&sa, sizeof(sa) ) == -1 ) {
		perror("bind error:");
		exit(0);
	}

	if( listen(sockfd, 5) == -1 ) {
		perror("listen error:");
		exit(0);
	}
	printf("Listening requests... port number = %d\n",port);

	clen = sizeof(ca);

	while(1)
	{
		fd = accept(sockfd, (struct sockaddr *)&ca, (socklen_t *)&clen);

		if(fd == -1) {
			perror("accept error:");
			exit(0);
		}

		if( read(fd, &com, sizeof(com))==0 ) {
			close(fd);
			continue;
		}
		dp("com = %c\n",com);

		if(com == BACKUP_AND_STOP ) {
			close(fd);
			break;
		}

		// TODO : 
		

	}
	close(sockfd);

	// TODO : backup process, 
	// 1. joining threads, handle waiting requests.
	// 2. serialize and backup to file 

	return 0;
}
