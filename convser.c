#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<netinet/in.h>
#include"uthash.h"

#ifndef NODP
#define dp(fmt,args...)	printf(fmt, ## args)
#else
#define dp(fmt,args...)
#endif

#define BACKUP_FILE_PATH	"/tmp/convser_backup.bin"
#define DEFAULT_PORT		7183

struct hitem {
	ino_t key;
	ino_t value;
	UT_hash_handle hh;
};

struct hitem *map = NULL;

void usage() {
	printf("USAGE: convser [OPTION]... &\n");
	printf("  options : \n");
	printf("      -p [Port] : listening port number ranged between 20 and 30000. default = %d \n",DEFAULT_PORT);
	printf("      -f [filepath] : filename with path containing previous mapping contents.\n");
	printf("                      If not set, start with an emtpy map\n");
	printf("      -b [filepath] : filename with path to which mapping contents will be backuped when stopped.\n");
	printf("                      default = " BACKUP_FILE_PATH "\n");
	printf("      -n            : contents will not be backed up when stopped\n");
	printf("      -h            : print this message\n");
	exit(1);
}

void do_backup(const char *outfilename) {
	int fd;
	struct hitem *cur, *tmp;
	ino_t lino, pino;

	fd = creat(outfilename,0440);
	if( fd < 0 ) {
		perror("opening backup file:");
		return;
	}

	printf("Backing up mapping to the file %s...",outfilename);
	HASH_ITER(hh, map, cur, tmp) {
		lino = cur->key;
		pino = cur->value;
		write(fd, &lino, sizeof(ino_t));
		write(fd, &pino, sizeof(ino_t));
		HASH_DEL(map, cur);
		free(cur);
	}
	close(fd);
	printf("Done\n");
}


int main(int argc, char **argv) 
{
	int c;
	uint16_t port = DEFAULT_PORT;
	char outfilename[1024] = BACKUP_FILE_PATH;
	char infilename[1024] = {0};
	int infileexist = 0;
	int backup_flag = 1;

	while( (c = getopt(argc, argv, "p:f:b:hn" ))!= -1) {
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
			case 'n':
				backup_flag = 0;	
		}
	}

	printf("Initializing inode number converter server.....\n");
	if( backup_flag ) printf("The mapping table will be backed up to %s when this server stops\n",outfilename);

	int fd;
	ino_t lino, pino;
	struct hitem *hi;
	if( infileexist ) {
		printf("Restoring mapping from the file %s\n",infilename);
		//restore hash map from file 
		fd = open( infilename, O_RDONLY );
		
		if(fd == -1) {
			printf("The backup file does't exist. Starting with an empty map.\n"); 
		} else {
			while( read(fd, &lino, sizeof(ino_t) ) ) {
				if( read(fd, &pino, sizeof(ino_t)) == 0 ) {
					printf("The given backup file is invalid. Starting with an empty map.\n");
					struct hitem *cur, *tmp;
					
					HASH_ITER(hh, map, cur, tmp) {
						HASH_DEL(map, cur);
						free(cur);
					}
					break;
				}
				hi = calloc( sizeof(struct hitem), 1);
	
				hi->key = lino;
				hi->value = pino;
				HASH_ADD(hh, map, key, sizeof(ino_t), hi);
			}
			close(fd);
		}
	}
	int sockfd, clen;
	struct sockaddr_in sa,ca;
	char com;

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

		if( read(fd, &com, sizeof(char))==0 ) {
			close(fd);
			continue;
		}
		dp("com = %c\n",com);

		switch(com) {		
		case 'G':
			if( read(fd, &lino, sizeof(ino_t) ) == 0 ) {
				dp("EOF before receive Luster inode number\n");
				pino = 0;
			} else {
				hi = NULL;
				HASH_FIND(hh, map, &lino, sizeof(ino_t), hi);
				pino = (hi) ? hi->value : 0;
			}
			dp("lino = %ld, pino = %ld\n",lino, pino);
			write(fd, &pino, sizeof(ino_t));
			break;
		case 'P':
			if( read(fd, &lino, sizeof(ino_t) ) == 0 || read(fd, &pino, sizeof(ino_t) ) == 0  ) {
				dp("EOF before receive inode numbers\n");
				break;
			}
			dp("lino = %ld, pino = %ld\n",lino, pino);
			hi = NULL;
			HASH_FIND(hh, map, &lino, sizeof(ino_t), hi);
			if( hi ) {
				dp("exist! overwrite!\n");
				hi->value = pino;
			} else {
				dp("not exist! create a new pair\n");
				hi = calloc( sizeof(struct hitem), 1);
	
				hi->key = lino;
				hi->value = pino;
				HASH_ADD(hh, map, key, sizeof(ino_t), hi);
			}
			break;
		}
		close(fd);
		if(com == 'Q') break;
	}
	close(sockfd);

	if( backup_flag )
		do_backup(outfilename);

	return 0;
}
