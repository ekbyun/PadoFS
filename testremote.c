#include"incont.h"
#include"inserver.h"
#include<sys/types.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <fcntl.h>
#include"errno.h"
#include"test.h"

#define NUM_COM	16
#define NUM_INODE 2

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
	lhid[1] = inet_addr("192.168.0.7");
	printf("%u %u\n", lhid[0], lhid[1]); 

#ifdef WITH_MAPSERVER
	ino_t _bino;
#endif
	loid_t _loid;
	size_t _size, _f, _do, _len, _ver;
	time_t _at, _mt;
	uint8_t _flags;
	uint32_t _hid;

	ino_t bino, binos[NUM_INODE];
	size_t size = 4096;

	for(i = 0 ; i < NUM_INODE; i++ ) {
#ifdef WITH_MAPSERVER
		com = CREATE_INODE;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			fail++;
			continue;
		}
		bino = i+100;
		binos[i] = bino;
		write(sockfd, &com, sizeof(com));
		write(sockfd, &bino, sizeof(bino));
		write(sockfd, &size, sizeof(size));

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
				_ver = 0;
				write(sockfd, &com, sizeof(com));
				write(sockfd, &tinos[i], sizeof(ino_t));
				write(sockfd, &_flags, sizeof(_flags));
				write(sockfd, &_ver, sizeof(_ver));

				read(sockfd,&ret,sizeof(ret));
				read(sockfd, &_bino , sizeof(ino_t));
				read(sockfd, &_size , sizeof(size_t));
				read(sockfd, &_at , sizeof(time_t));
				read(sockfd, &_mt , sizeof(time_t));
				read(sockfd, &_flags , sizeof(uint8_t));
				read(sockfd, &_ne , sizeof(uint32_t));

				printf("Opened[%lu] ret=%s, bino=%lu , size=%lu, a/m=%lu/%lu, flags=%o, num_exts=%d\n",tinos[i], retstr[ABS(ret)], _bino,_size,_at,_mt,_flags,_ne);
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
		printf("create inode %lu(%lu) returns %s\n", tinos[i], binos[i], retstr[ABS(ret)]);
		close(sockfd);
#else
		com = CREATE_OPEN_INODE;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			fail++;
			continue;
		}
		bino = i+100;
		binos[i] = bino;
		tinos[i] = bino;
		_flags = 2;
		write(sockfd, &com, sizeof(com));
		write(sockfd, &bino, sizeof(bino));
		write(sockfd, &size, sizeof(size));
		write(sockfd, &_flags, sizeof(uint8_t));

		read(sockfd,&ret,sizeof(ret));
		if(	 ret == SERVER_BUSY ) {
			fail++;
		} else if ( ret == QUEUED ) {
			read(sockfd, &_size , sizeof(size_t));
			read(sockfd, &_at , sizeof(time_t));
			read(sockfd, &_mt , sizeof(time_t));
			read(sockfd, &_ver , sizeof(size_t));
			read(sockfd, &_flags , sizeof(uint8_t));

			read(sockfd, &_ne , sizeof(uint32_t));

			printf("Opened[%lu] size=%lu, a/m=%lu/%lu, flags=%o, num_exts=%d\n",binos[i], _size,_at,_mt,_flags,_ne);
			for(j = 0 ; j < _ne ; j++ ) {
				read(sockfd, &_hid , sizeof(uint32_t));
				read(sockfd, &_loid , sizeof(loid_t));
				read(sockfd, &_f , sizeof(size_t));
				read(sockfd, &_do , sizeof(size_t));
				read(sockfd, &_len , sizeof(size_t));
				printf(" ino=%lu, do=[%u,%lu],range=[%lu,%lu,%lu]\n",tinos[i],_hid,_loid,_f,_do,_len);
			}
		}
		read(sockfd,&ret,sizeof(ret));
		close(sockfd);
		printf("create inode %lu(%lu) returns %s\n", tinos[i], binos[i], retstr[ABS(ret)]);
		if(ret == SUCCESS ) {
			created++;
		} else if( ret == -ALREADY_CREATED ) {
			opened++;
		} else {
			fail++;
		}
#endif
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
			hid = lhid[rand()%2];
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
		tino = tinos[0];

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

//	TEST for READ_LAYOUT and CLONE_TO
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = READ_LAYOUT;
	write(sockfd, &com, sizeof(com));
	write(sockfd, &tinos[0], sizeof(tino));
	read(sockfd, &ret, sizeof(ret));
	_f = 1000;
	_len = 5000;
	_flags = 1;
	
	write(sockfd, &_f, sizeof(size_t));
	write(sockfd, &_len, sizeof(size_t));
	write(sockfd, &_flags, sizeof(_flags));

	int fd2 = socket(AF_INET, SOCK_STREAM, 0);
	connect(fd2, (struct sockaddr *)&clientaddr, client_len);
	unsigned char com2 = CLONE_TO;
	write(fd2, &com2, sizeof(com));
	write(fd2, &tinos[1], sizeof(tino));
	read(fd2, &ret, sizeof(ret));

	_do = rand()% 5000;
	write(fd2, &_do, sizeof(size_t));
	write(fd2, &_len, sizeof(size_t));
	printf("CLONE %lu,(%lu~%lu) to %lu,(%lu,%lu)\n",tinos[0],_f,_f+_len,tinos[1],_do,_do+_len);

    while(1) {
		read(sockfd, &_hid, sizeof(uint32_t));
		read(sockfd, &_loid, sizeof(loid_t));

		write(fd2, &_hid, sizeof(uint32_t));
		write(fd2, &_loid, sizeof(loid_t));

		if( _hid == 0 && _loid == 0 ) break;  //end of list

		read(sockfd, &_f, sizeof(size_t));
		read(sockfd, &_do, sizeof(size_t));
		read(sockfd, &_len, sizeof(size_t));

		write(fd2, &_f, sizeof(size_t));
		write(fd2, &_do, sizeof(size_t));
		write(fd2, &_len, sizeof(size_t));

		printf(" layout read is dobj=[%u,%lu] (%lu ~ %lu , %lu)  off_do = %lu\n",_hid, _loid, _f, _f + _len, _len, _do);
	}
	read(sockfd, &ret, sizeof(ret));
	printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com], tinos[0], retstr[ABS(ret)]);
	close(sockfd);
	read(fd2, &ret, sizeof(ret));
	printf("[%d]%s on %ld is done. ret = %s\n", pid,comstr[com2], tinos[1], retstr[ABS(ret)]);
	close(fd2);
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
#ifdef WITH_MAPSERVER
		write(sockfd, &bino, sizeof(bino));
#endif

		read(sockfd, &ret, sizeof(ret));

#ifdef WITH_MAPSERVER
		read(sockfd, &_bino, sizeof(_bino));
#endif
		read(sockfd, &_size, sizeof(_size));
		read(sockfd, &_num, sizeof(_num));
		printf("size=%lu, num=%u\n", _size, _num);
		
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
	*/
	/*
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

*/
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
