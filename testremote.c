#include"incont.h"
#include"inserver.h"
#include<sys/types.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <fcntl.h>
#include"errno.h"
#include"test.h"

#define NUM_COM	10
#define NUM_INODE 2

int main(int argc, char **argv) 
{
	pid_t pid;
	int client_len;
	int sockfd;
	struct sockaddr_in clientaddr; 
	int i;
	int suc = 0, fail = 0, fail2 = 0, c = 0, busy = 0;
	int port = 3495;

	if( argc > 1 ) {
		port = atoi(argv[1]);
	}

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr("192.168.0.7");
	clientaddr.sin_port = htons(port);

	client_len = sizeof(clientaddr);

	srand(time(NULL));
	pid = getpid();

	unsigned char com;
	int ret;
	ino_t tino;
	ino_t tinos[NUM_INODE];

	char ci[4] = "W";
	uint32_t hid, lhid[2];
	loid_t loid;
	size_t start, end;
	
	lhid[0] = ntohl(inet_addr("127.0.0.1"));
	lhid[1] = ntohl(inet_addr("192.168.0.7"));

	tinos[0] = 504588700852682753;
	tinos[1] = 504588700852682754;
//	/*	
	ino_t pino = 10, bino;
	mode_t mode = 0666;
	uid_t uid = 7;
	gid_t gid = 17;
	char name[FILE_NAME_SIZE] = "testfile";
	size_t size = 4096;

	for(i = 0 ; i < NUM_INODE; i++ ) {
		com = CREATE_INODE;
		sprintf(name,"testfile%02d",i);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			exit(0);
		}
		bino = i+100;
		uid++;
		gid++;
		write(sockfd, &com, sizeof(com));
		write(sockfd, &bino, sizeof(bino));
		write(sockfd, &pino, sizeof(pino));
		write(sockfd, &size, sizeof(size));
		write(sockfd, &mode, sizeof(mode));
		write(sockfd, &uid, sizeof(uid));
		write(sockfd, &gid, sizeof(gid));
//		write(sockfd, name, FILE_NAME_SIZE);

		read(sockfd,&ret,sizeof(ret));
		if(	 ret == SERVER_BUSY ) {
			fail++;
		} else if ( ret == QUEUED ) {
			read(sockfd,&(tinos[i]),sizeof(ino_t));
			read(sockfd,&ret,sizeof(ret));
			if( ret < 0 && ret != -ALREADY_CREATED ) {
				fail++;
			}
		}
//		printf("create inode %s is done. ino = %ld,  ret = %s\n", name, tinos[i], retstr[ABS(ret)]);
		printf("create inode %ld returns %s\n", tinos[i], retstr[ABS(ret)]);
		close(sockfd);
	}
	if( fail >= 0 ) exit(0);
//	*/
	for(i = 0; i < NUM_COM; i++) {
	/*
		scanf("%s %d %ld %ld %ld",ci,&hid,&loid,&start,&end);

		if( ci[0] == '#' ) continue;
		if( ci[0] == 'q' || ci[0] == 'Q' ) break;
		
		if( ci[0] == 'W' ) com = WRITE;
		else 
			continue;
	*/
		com = WRITE;
		hid = lhid[i%2];
//		hid = rand() % 5;
//		loid = rand() % 8;
		loid = 2;
		start = rand() % 10000;
		end = 10 + rand()%1000;

		c++;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			printf("[%d]fail to connect to server\n",pid);
			fail++;
			close(sockfd);
			continue;
		}

		tino = tinos[i%NUM_INODE];
	//	tino = tinos[0];

		write(sockfd, &com, sizeof(com) );
		write(sockfd, &tino, sizeof(tino) );
	
		switch(com) {
			case WRITE:
				write(sockfd, &hid, sizeof(hid));
				write(sockfd, &loid, sizeof(loid));
				write(sockfd, &start, sizeof(start));
				write(sockfd, &start, sizeof(start));
				write(sockfd, &end, sizeof(end));

				read(sockfd,&ret,sizeof(ret));
				if(	 ret == SERVER_BUSY ) {
					busy++;
				} else if ( ret == QUEUED ) {
					read(sockfd,&ret,sizeof(ret));
					if( ret < 0 ) {
						printf("[%d]fail occurred ! %s\n",pid, retstr[ABS(ret)]);
						fail++;
					} else {
						suc++;
					}
				} else {
					printf("[%d]fail2 occurred ! %s\n",pid, retstr[ABS(ret)]);
					fail2++;
				}
		//		dp("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
				break;
		}

		close(sockfd);
	}

//`*/		
	printf("[%d]success = %d/%d, fail = %d/%d fail2=%d busy = %d (%s)\n",pid,suc,c,fail,c, fail2, busy, ci);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = READ_DOBJ;
	tino = tinos[0];
	hid = lhid[0];
	loid = 2;
	size_t base_ino;
	uint8_t shared;

	write(sockfd, &com, sizeof(com));
	write(sockfd, &tino, sizeof(tino));

	write(sockfd, &hid, sizeof(hid));
	write(sockfd, &loid, sizeof(loid));

	read(sockfd, &ret, sizeof(ret));

	read(sockfd, &base_ino, sizeof(base_ino));
	read(sockfd, &shared, sizeof(shared));

	printf("bino = %ld, shared=%d\n",base_ino, shared);

	while(1) {
		read(sockfd, &start, sizeof(size_t));
		read(sockfd, &start, sizeof(size_t));
		read(sockfd, &end, sizeof(size_t));
		if( start == 0 && end == 0 ) break;
		printf("off = %ld, len = %ld\n",start,end);
	}

	read(sockfd, &ret, sizeof(ret));
	close(sockfd);

	printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
	/*
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = BACKUP_AND_STOP;
	write(sockfd, &com, sizeof(com));
	read(sockfd, &ret, sizeof(ret));
	close(sockfd);
	*/
	return 0;
}
