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

#define BACKUP_FILE_PATH	"/tmp/pado_inode_server_backup.bin"
#define DEFAULT_PORT		3495

void usage() {
	printf("USAGE: inodeserver [OPTION]... &\n");
	printf("  options : \n");
	printf("      -p [Port] : listening port number ranged between 20 and 30000. default = %d \n",DEFAULT_PORT);
	printf("      -f [filepath] : filename with path containing previous mapping contents.\n");
	printf("                      If not set, start with an emtpy map\n");
	printf("      -b [filepath] : filename with path to which mapping contents will be backuped when stopped.\n");
	printf("                      default = " BACKUP_FILE_PATH "\n");
	printf("      -n [nodeid]   : the node ID which will be used as the offset of inode number created in this server\n"); 
	printf("      -h            : print this message\n");
	exit(1);
}

void do_restore(int);

int main(int argc, char **argv) 
{
	int c;
	uint16_t port = DEFAULT_PORT;
	char outfilename[1024] = BACKUP_FILE_PATH;
	char infilename[1024] = {0,};
	int infileexist = 0;
	uint32_t nodeid = 0;

	while( (c = getopt(argc, argv, "p:f:n:hb:" ))!= -1) {
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
			case 'n':
				nodeid = atoi(optarg);
				break;
			case 'h':
				usage();
		}
	}

	printf("Initializing PADOFS inode server.....\n");


	int fd;
	ino_t base;
	if( infileexist ) {
		printf("Restoring inodes from the file %s\n",infilename);
		//restore hash map from file 
		fd = open( infilename, O_RDONLY );
		if(fd == -1) {
			printf("The backup file does't exist. Starting with an empty map.\n"); 
		} else {
			read(fd, &base, sizeof(ino_t));
			init_inode_container(nodeid, base);
			do_restore(fd);
			close(fd);
		}
	} else {
		init_inode_container(nodeid,0);
	}
	
	print_all();

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


	//TODO : initialize threads, jobs queues, 

	while(0)
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

		// TODO : enqueue or return BUSY MESSAGE
		
		close(fd);
	}
	close(sockfd);

	test_main();

	// TODO : backup process, 
	// 1. joining threads, handle waiting requests.
	
	fd = creat(outfilename, 0440);
	do_backup(fd);
	close(fd);

	return 0;
}

void do_restore(int fd) {
	char fname[FILE_NAME_SIZE];
	unsigned short mode;
	ino_t ino, pino, bino, sino, loid;
	unsigned int uid, gid;
	size_t size, bsize, off_f, off_do, len;
	time_t at,mt,ct;
	uint32_t ne, hid;
	struct inode *inode;
	struct dobject *dobj;
	int i;

	dp("Restoring inode map\n");
	while( read(fd, &ino, sizeof(ino_t)) ) 
	{
		read(fd, &mode, sizeof(unsigned short));
		read(fd, &uid, sizeof(unsigned int));
		read(fd, &gid, sizeof(unsigned int));
		read(fd, &size, sizeof(size_t));
		read(fd, &at, sizeof(time_t));
		read(fd, &mt, sizeof(time_t));
		read(fd, &ct, sizeof(time_t));
		read(fd, fname, FILE_NAME_SIZE);
		read(fd, &pino, sizeof(ino_t));
		read(fd, &bino, sizeof(ino_t));
		read(fd, &bsize, sizeof(size_t));
		read(fd, &sino, sizeof(ino_t));

		inode = create_inode(fname, ino, mode, pino, bino, uid, gid, size);
		
		read(fd, &ne, sizeof(uint32_t));

		for(i = 0 ; i < ne ; i++) {
			read(fd, &hid, sizeof(uint32_t));
			read(fd, &loid, sizeof(ino_t));
			read(fd, &off_f, sizeof(size_t));
			read(fd, &off_do, sizeof(size_t));
			read(fd, &len, sizeof(size_t));

			dobj = get_dobject(hid, loid, inode);
			pado_write(inode, dobj, off_f, off_do, len);
		}

		set_inode_aux(inode, at, mt, ct, bsize, sino);
	}
}
