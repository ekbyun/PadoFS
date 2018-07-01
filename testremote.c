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
#define NUM_INODE 1

int main(int argc, char **argv) 
{
	pid_t pid;
	int client_len;
	int sockfd;
	struct sockaddr_in clientaddr; 
	int i,j;
	int created = 0, opened = 0, suc = 0, fail = 0, fail2 = 0, c = 0, busy = 0;
	int port = 3495;

	printf("./testremote 127.0.0.1 3495 in\n");
	if( argc > 2 ) {
		port = atoi(argv[1]);
	}

	clientaddr.sin_family = AF_INET;

	if( argc < 2 ) {
		clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	} else {
		clientaddr.sin_addr.s_addr = inet_addr(argv[1]);
	}
	clientaddr.sin_port = htons(port);

	client_len = sizeof(clientaddr);

	srand(time(NULL));
	pid = getpid();

	unsigned char com;
	int ret;
	ino_t tino, tinos[NUM_INODE];

	char ci[4] = "W";
	uint32_t hid, lhid[2], _ne;
	loid_t loid;
	size_t start, end;
	
	lhid[0] = inet_addr("127.0.0.1");
	lhid[1] = inet_addr("192.168.0.101");
	printf("%u %u\n", lhid[0], lhid[1]); 

	ino_t _bino, _pino;
	loid_t _loid;
	size_t _size, _f, _do, _len;
	mode_t _mode;
	uid_t _uid;
	gid_t _gid;
	time_t _at, _mt;
	uint8_t _flags;
	uint32_t _hid;

	ino_t pino = 10, bino, binos[NUM_INODE];
	mode_t mode = 0666;
	uid_t uid = 7;
	gid_t gid = 17;
	size_t size = 4096;

	for(i = 0 ; i < NUM_INODE; i++ ) {
		com = CREATE_INODE;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			fail++;
			continue;
		}
		bino = i+100;
		binos[i] = bino;
		uid++;
		gid++;
		write(sockfd, &com, sizeof(com));
		write(sockfd, &bino, sizeof(bino));
		write(sockfd, &pino, sizeof(pino));
		write(sockfd, &size, sizeof(size));
		write(sockfd, &mode, sizeof(mode));
		write(sockfd, &uid, sizeof(uid));
		write(sockfd, &gid, sizeof(gid));

		read(sockfd,&ret,sizeof(ret));
		if(	 ret == SERVER_BUSY ) {
			fail++;
		} else if ( ret == QUEUED ) {
			read(sockfd,&(tinos[i]),sizeof(ino_t));
			read(sockfd,&ret,sizeof(ret));
			if( ret == -ALREADY_CREATED ) {
				close(sockfd);
				sockfd = socket(AF_INET, SOCK_STREAM, 0);
				if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
					fail++;
					continue;
				}
				com = READ_INODE_WHOLE;
				_flags = 1;
				write(sockfd, &com, sizeof(com));
				write(sockfd, &tinos[i], sizeof(ino_t));
				write(sockfd, &_flags, sizeof(_flags));

				read(sockfd,&ret,sizeof(ret));
				read(sockfd, &_bino , sizeof(ino_t));
				read(sockfd, &_pino , sizeof(ino_t));
				read(sockfd, &_size , sizeof(size_t));
				read(sockfd, &_mode , sizeof(mode_t));
				read(sockfd, &_uid , sizeof(uid_t));
				read(sockfd, &_gid , sizeof(gid_t));
				read(sockfd, &_at , sizeof(time_t));
				read(sockfd, &_mt , sizeof(time_t));
				read(sockfd, &_flags , sizeof(uint8_t));
				read(sockfd, &_ne , sizeof(uint32_t));

				printf("Opened[%lu] ret=%s, bino=%lu , pino=%lu, size=%lu, mode=%o, u/d=%d/%d, a/m=%lu/%lu, flags=%o, num_exts=%d\n",tinos[i], retstr[ABS(ret)], _bino,_pino,_size,_mode,_uid,_gid,_at,_mt,_flags,_ne);
				for(j = 0 ; j < _ne ; j++ ) {
					read(sockfd, &_hid , sizeof(uint32_t));
					read(sockfd, &_loid , sizeof(loid_t));
					read(sockfd, &_f , sizeof(size_t));
					read(sockfd, &_do , sizeof(size_t));
					read(sockfd, &_len , sizeof(size_t));
					printf(" ino=%lu, do=[%u,%lu],range=[%lu,%lu,%lu]\n",tinos[i],_hid,_loid,_f,_do,_len);
				}

				read(sockfd,&ret,sizeof(ret));
				if( ret == SUCCESS ) {
					opened++;
				} else {
					fail++;
				}
			} else if( ret < 0 && ret != -ALREADY_CREATED ) {
				fail++;
			} else if (ret == SUCCESS) {
				created++;
			}
		}
		printf("create inode %ld returns %s\n", tinos[i], retstr[ABS(ret)]);
		close(sockfd);
	}

	printf("[%d] Created = %d/%d, opened = %d/%d, fail = %d\n",pid,created,NUM_INODE,opened,NUM_INODE, fail );
	if( fail > 0 ) {
		printf("stop due to create/open failure\n");
		exit(0);
	}


	for(i = 0; i < NUM_COM; i++) {
		if( argc > 3 && argv[3][0] == 'i' ) { 
			scanf("%s %d %ld %ld %ld",ci,&hid,&loid,&start,&end);

			if( ci[0] == '#' ) continue;
			if( ci[0] == 'q' || ci[0] == 'Q' ) break;

			if( ci[0] == 'W' ) com = WRITE;
			if( ci[0] == 'T' ) com = TRUNCATE;
			if( ci[0] == 'R' ) com = READ_LAYOUT;
			if( ci[0] == 'C' ) com = WRITE;
			if( ci[0] == 'r' ) com = READ_INODE_WHOLE;
		} else {
			com = WRITE;
			hid = lhid[i%2];
			loid = rand() % 4;
			start = rand() % 10000;
			end = 10 + rand()%1000;
		}

		c++;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			printf("[%d]fail to connect to server\n",pid);
			fail++;
			close(sockfd);
			continue;
		}

		tino = tinos[i%NUM_INODE];

		write(sockfd, &com, sizeof(com) );
		write(sockfd, &tino, sizeof(tino) );
		read(sockfd,&ret,sizeof(ret));
		if(	ret == SERVER_BUSY ) {
			busy++;
			printf("[%d]SERVER is busy!\n",pid);
			close(sockfd);
			continue;
		} else if ( ret != QUEUED ) {
			printf("[%d]fail2 occurred ! %s\n",pid, retstr[ABS(ret)]);
			fail2++;
		}

		switch(com) {
			case WRITE:
				write(sockfd, &hid, sizeof(hid));
				write(sockfd, &loid, sizeof(loid));
				write(sockfd, &start, sizeof(start));
				write(sockfd, &start, sizeof(start));
				write(sockfd, &end, sizeof(end));
				break;

		}

		read(sockfd,&ret,sizeof(ret));
		if( ret < 0 ) {
			fail++;
		} else {
			suc++;
		}
		dp("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
		close(sockfd);
	}
	printf("[%d]live test success = %d/%d, fail = %d/%d fail2=%d busy = %d (%s)\n",pid,suc,c,fail,c, fail2, busy, ci);

/*
//	TEST for GET_INODE_DOBJ, READ_DOBJECT, RELEASE_DRAIN_LOCK
	uint32_t _hids[1024];
	loid_t _loids[1024];
	uint32_t _num;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = GET_INODE_DOBJ;
	tino = tinos[0];
	bino = binos[0];

	write(sockfd, &com, sizeof(com));
	write(sockfd, &tino, sizeof(tino));

	read(sockfd, &ret, sizeof(ret));
	read(sockfd, &_ne, sizeof(uint32_t));
	printf("GET_INODE_DOBJ on %lu returns num = %u\n", tino, _ne);
	for(i = 0 ; i < _ne ; i++) {
		read(sockfd, &_hids[i], sizeof(uint32_t));
		read(sockfd, &_loids[i], sizeof(loid_t));
		printf("    hid = %u, loid = %lu\n",_hids[i], _loids[i]);
	}
	read(sockfd, &ret, sizeof(ret));
	close(sockfd);
	printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);

	for(i = 0 ; i < _ne ; i++) {
		printf("READ_DOBJECT on ino=%lu, hid = %u, loid = %lu\n",tino, _hids[i], _loids[i]);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
		com = READ_DOBJ;
		hid = _hids[i];
		loid = _loids[i];

		write(sockfd, &com, sizeof(com));
		write(sockfd, &tino, sizeof(tino));
		write(sockfd, &hid, sizeof(hid));
		write(sockfd, &loid, sizeof(loid));
		write(sockfd, &bino, sizeof(bino));

		read(sockfd, &ret, sizeof(ret));

		read(sockfd, &_bino, sizeof(_bino));
		read(sockfd, &_size, sizeof(_size));
		read(sockfd, &_num, sizeof(_num));
		printf("bino = %lu, size=%lu, num=%u\n",_bino, _size, _num);
		
		for(j=0;j<_num;j++){
			read(sockfd, &_f, sizeof(size_t));
			read(sockfd, &_do, sizeof(size_t));
			read(sockfd, &_len, sizeof(size_t));
			printf("off_f = %lu, _off_do= %lu ,len = %lu\n",_f, _do, _len);
		}
		read(sockfd, &ret, sizeof(ret));
		close(sockfd);
		printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
//	}
//	for(i = 0 ; i < _ne ; i++) {
		printf("RELEASE_DRAIN_LOCK on ino=%lu\n",tino);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
		com = RELEASE_DRAIN_LOCK;
		write(sockfd, &com, sizeof(com));
		write(sockfd, &tino, sizeof(tino));
		read(sockfd, &ret, sizeof(ret));
		read(sockfd, &ret, sizeof(ret));
		close(sockfd);
		printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
	}

	// TEST for REMOVE_DOBJ
	for(i = 0 ; i < _ne ; i++) {
		hid = _hids[i];
		loid = _loids[i];
		printf("REMOVE_DOBJ on ino=%lu ,hid=%u ,loid=%lu\n",tino, hid, loid);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
		com = REMOVE_DOBJ;
		_flags = 1;	//unlink flag
		write(sockfd, &com, sizeof(com));
		write(sockfd, &tino, sizeof(tino));
		write(sockfd, &hid, sizeof(hid));
		write(sockfd, &loid, sizeof(loid));
		write(sockfd, &_flags, sizeof(_flags));
		read(sockfd, &ret, sizeof(ret));
		read(sockfd, &ret, sizeof(ret));
		close(sockfd);
		printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
	}
*/
	// TEST for DELETE_INODE before UNREF
	for(i = 0 ; i < NUM_INODE ; i++ ) {
		tino = tinos[i];
		com = DELETE_INODE;
		printf("DELETE_INODE on ino=%lu\n",tino);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
		write(sockfd, &com, sizeof(com));
		write(sockfd, &tino, sizeof(tino));
		read(sockfd, &ret, sizeof(ret));
		read(sockfd, &ret, sizeof(ret));
		close(sockfd);
		printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
	}

	// TEST for UNREF
	for(i = 0 ; i < NUM_INODE ; i++ ) {
		tino = tinos[i];
		com = UNREF;
		printf("UNREF on ino=%lu\n",tino);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
		write(sockfd, &com, sizeof(com));
		write(sockfd, &tino, sizeof(tino));
		read(sockfd, &ret, sizeof(ret));
		read(sockfd, &ret, sizeof(ret));
		close(sockfd);
		printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
	}

	// TEST for DELETE_INODE after UNREF
	for(i = 0 ; i < NUM_INODE ; i++ ) {
		tino = tinos[i];
		com = DELETE_INODE;
		printf("DELETE_INODE on ino=%lu\n",tino);
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
		write(sockfd, &com, sizeof(com));
		write(sockfd, &tino, sizeof(tino));
		read(sockfd, &ret, sizeof(ret));
		read(sockfd, &ret, sizeof(ret));
		close(sockfd);
		printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tino, retstr[ABS(ret)]);
	}


/*
	// TEST for BACKUP_AND_STOP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = BACKUP_AND_STOP;
	write(sockfd, &com, sizeof(com));
	read(sockfd, &ret, sizeof(ret));
	close(sockfd);
*/	
	return 0;
}
