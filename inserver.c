#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include"inserver.h"
#include"incont.h"
#include"uthash.h"
#include"pthread.h"
#include"errno.h"

#ifndef NODP
#include"test.h"
#endif

#define NUM_THREADS  4
#define WQ_LENGTH	128

#define BACKUP_FILE_PATH	"/tmp/pado_inode_server_backup.bin"
#define DEFAULT_PORT		3495

struct wqentry{
	unsigned char com;
	int fd;
	struct wqentry *next;
};

struct wqentry *wqhead = NULL;
struct wqentry *rqhead = NULL;

pthread_mutex_t wq_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rq_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wq_cond = PTHREAD_COND_INITIALIZER;
int cont;

void usage() {
	printf("USAGE: inodeserver [OPTION]... [address]&\n");
	printf("  options : \n");
	printf("      -p [Port] : listening port number ranged between 20 and 30000. default = %d \n",DEFAULT_PORT);
	printf("      -f [filepath] : filename with path containing previous mapping contents.\n");
	printf("                      If not set, start with an emtpy map\n");
	printf("      -b [filepath] : filename with path to which mapping contents will be backuped when stopped.\n");
	printf("                      default = " BACKUP_FILE_PATH "\n");
	printf("      -n [nodeid]   : the node ID which will be used as the offset of inode number created in this server\n"); 
	printf("      -h            : print this message\n");
	printf("  argument :\n");
	printf("      address : IPv4 address print this message. can be ommitted and ignored when a -f option is given.\n");
	exit(1);
}

void do_restore(int);

void *worker_thread(void *);

int main(int argc, char **argv) 
{
	int c;
	uint16_t port = DEFAULT_PORT;
	char outfilename[1024] = BACKUP_FILE_PATH;
	char infilename[1024] = {0,};
	int infileexist = 0;
	uint32_t nodeid = 0;
	int i;

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

	if( argc <= optind && infileexist == 0 ) {
		printf("IP address is a mandatory argument.\n");
		usage();
	}

	nodeid = inet_addr(argv[optind]);

	printf("Initializing PADOFS inode server.....nodeid = %d(%s)\n",nodeid, argv[optind]);

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
	
//	print_all();

	int sockfd, clen;
	struct sockaddr_in sa,ca;

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

	if( listen(sockfd, 32) == -1 ) {
		perror("listen error:");
		exit(0);
	}
	printf("Listening requests... port number = %d\n",port);

	clen = sizeof(ca);

	cont = 1;
	pthread_t workers[NUM_THREADS];
	int ids[NUM_THREADS];
	int cret;
	for(i = 0; i < NUM_THREADS; i++ ) {
	//	cret = pthread_create(&(workers[i]), NULL, worker_thread, NULL);
		ids[i] = i;
		cret = pthread_create(&(workers[i]), NULL, worker_thread, &(ids[i]) );
		if( cret != 0 ) {
			errno = cret;
			perror("pthread_create:");
			close(sockfd);
			exit(0);
		}
	}

	struct wqentry *wqtail = NULL;
	struct wqentry wqepool[WQ_LENGTH];
	struct wqentry *entry = NULL;

	for(i = 0; i < WQ_LENGTH ; i++) {
		wqepool[i].next = rqhead;
		rqhead = &(wqepool[i]);
	}
	
	unsigned char com;
	int ret;

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

		if(com == BACKUP_AND_STOP || com == STAGEOUT_ALL) {
			ret = SUCCESS;
			write(fd, &ret, sizeof(ret));
			close(fd);
			break;
		}

		pthread_mutex_lock(&rq_mut);
		if( rqhead == NULL ) {
			pthread_mutex_unlock(&rq_mut);
			ret = SERVER_BUSY;
			dp("SERVER_BUSY! command %s is rejected\n", comstr[com]);
			write(fd, &ret, sizeof(ret));
			close(fd);
			continue;
		}
		entry = rqhead;
		rqhead = rqhead->next;
		pthread_mutex_unlock(&rq_mut);

		entry->fd = fd;
		entry->com = com;
		entry->next = NULL;

		ret = QUEUED;
		write(fd, &ret, sizeof(ret));
		dp("com = %s is queued\n",comstr[com]);

		pthread_mutex_lock(&wq_mut);
		if( wqhead ) {
			wqtail->next = entry;
			wqtail = entry;	
		} else {
			wqhead = entry;
			wqtail = entry;
		}
		pthread_cond_signal(&wq_cond);
		pthread_mutex_unlock(&wq_mut);
	}
	close(sockfd);

	dp("closing server...waiting to complete jobs in the queue by thraeds\n");

	pthread_mutex_lock(&wq_mut);
	cont = 0;
	pthread_cond_broadcast(&wq_cond);
	pthread_mutex_unlock(&wq_mut);

	for( i = 0 ; i < NUM_THREADS ; i++ ) {
		pthread_join( workers[i], NULL );
	}

	dp("All threads completed!\n");

	dp("Starting backup to %s\n",outfilename);

	if( com == BACKUP_AND_STOP ) {
		fd = creat(outfilename, 0440);
		do_backup(fd);
		close(fd);
	} else if( com == STAGEOUT_ALL ) {
		stageout_all();
	}

	return 0;
}

void *worker_thread(void *arg) {
	int fd;
	unsigned char com;
	struct wqentry *entry;

	ino_t tino, pino, bino;
	size_t off_f, off_do, len;
	uint32_t hid;
	ino_t loid;
	struct inode *tinode;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	size_t size;
	char name[FILE_NAME_SIZE];

#ifndef NODP
	int tid;
	tid = *((int *)arg);
	dp("Thread #%d started!\n",tid);
#endif

	int ret;

	while( 1 ) {
		pthread_mutex_lock(&wq_mut);
		while ( wqhead == NULL && cont == 1 ) {
			dp("Work queue is empty and server is not down. Thread #%d waiting..\n",tid);
			pthread_cond_wait(&wq_cond, &wq_mut);
		}
		dp("Thread #%d is awaken now. cont = %d\n", tid, cont);
		if( wqhead == NULL && cont == 0 ) {
			pthread_mutex_unlock(&wq_mut);
			break;
		}

		entry = wqhead;
		wqhead = wqhead->next;
		pthread_mutex_unlock(&wq_mut);

		fd = entry->fd;
		com = entry->com;

		pthread_mutex_lock(&rq_mut);
		entry->next = rqhead;
		rqhead = entry;
		pthread_mutex_unlock(&rq_mut);

		dp("[%d] fd = %d , com = %s\n", tid, fd, comstr[com]);

		if( com != CREATE_INODE ) {
			read(fd, &tino, sizeof(ino_t));
			tinode = get_inode(tino);
			if( tinode == NULL ) {
				ret = -INVALID_INO;
				write(fd, &ret, sizeof(ret));
				dp("There is no inode with ino %ld\n", tino);
				close(fd);
				continue;
			}
		}

		ret = SUCCESS;

		// TODO : 
		switch(com) {
			case WRITE:
				read(fd, &hid, sizeof(hid));
				read(fd, &loid, sizeof(loid));
				read(fd, &off_f, sizeof(off_f));
				read(fd, &off_do, sizeof(off_do));
				read(fd, &len, sizeof(len));
				ret = pado_write( tinode, get_dobject(hid, loid, tinode, 1), off_f, off_do, len); 
				break;
			case TRUNCATE:
				read(fd, &off_f, sizeof(off_f));
				pado_truncate(tinode, off_f);
				break;
			case READ_LAYOUT:
				read(fd, &off_f, sizeof(off_f));
				read(fd, &len, sizeof(len));
				ret = pado_read(tinode, fd, 0, off_f, off_f + len);
				break;
			case CLONE_FROM:
				read(fd, &off_f, sizeof(off_f));
				read(fd, &len, sizeof(len));
				ret = pado_read(tinode, fd, 1, off_f, off_f + len);
				if( ret == BASE_CLONED ) {
					ret = SUCCESS;
					tinode->is_shared = 1;
				}
				break;
			case CLONE_TO:
				read(fd, &off_f, sizeof(off_f));
				read(fd, &len, sizeof(len));
				ret = pado_clone(tinode, fd, off_f, off_f + len);
				break;
			case DELETE_RANGE:
				read(fd, &off_f, sizeof(off_f));	//start offset
				read(fd, &len, sizeof(len));		//end offset
				if( off_f >= len ) ret = -INVALID_RANGE;
				else pado_del_range(tinode, off_f, len);
				break;
			case GET_INODE:
				pado_getinode(tinode, fd);
				break;
			case GET_INODE_WHOLE:
				pado_getinode_all(tinode, fd);
				break;
			case SET_INODE:
				read(fd, &(tinode->mode), sizeof(mode_t));
				read(fd, &(tinode->uid), sizeof(uid_t));
				read(fd, &(tinode->gid), sizeof(gid_t));
				read(fd, &(tinode->parent_ino), sizeof(ino_t));
				read(fd, &(tinode->base_ino), sizeof(ino_t));
				read(fd, tinode->name, FILE_NAME_SIZE);
				clock_gettime(CLOCK_REALTIME_COARSE, &(tinode->ctime));
				break;
			case CREATE_INODE:
				read(fd, &mode, sizeof(mode_t));
				read(fd, &uid, sizeof(uid_t));
				read(fd, &gid, sizeof(gid_t));
				read(fd, &pino, sizeof(ino_t));
				read(fd, &bino, sizeof(ino_t));
				read(fd, name, FILE_NAME_SIZE);
				read(fd, &size, sizeof(size_t));
				tinode = create_inode( name, 0, mode, pino, bino, uid, gid, size);
				if( tinode == NULL ) {
					ret = -CREATE_INODE_FAIL;
					tino = 0;
				} else {
					tino = tinode->ino;
				}
				write(fd, &tino, sizeof(ino_t));
				break;
			case RELEASE_INODE:
				ret = release_inode(tinode);
				break;
			case READ_DOBJ:
				read(fd, &hid, sizeof(hid));
				read(fd, &loid, sizeof(loid));
				write(fd, &(tinode->base_ino), sizeof(ino_t));
				write(fd, &(tinode->is_shared), sizeof(uint8_t));
				ret = read_dobject( get_dobject(hid, loid, tinode, 0), fd );
				break;
			case REMOVE_DOBJ:
				read(fd, &hid, sizeof(hid));
				read(fd, &loid, sizeof(loid));
				if( hid == 0 && loid == 0 ) {
					ret = tinode->is_shared ? SUCCESS : -INVALID_DOBJECT;
					tinode->is_shared = 0;
				} else {
					ret = remove_dobject( get_dobject(hid, loid, tinode, 0), 1, 0);
				}
				break;
			case GET_INODE_DOBJ:
				ret = get_inode_dobj(tinode, fd);
				break;
			case DELETE_INODE:
				ret = delete_inode(tinode);
				break;
			case STAGEOUT:
				ret = stageout(tinode);
				break;
		}
		write(fd, &ret, sizeof(ret));
		dp("return of command %s on file %s[%ld] is %s\n", comstr[com], tinode->name, tinode->ino, retstr[ABS(ret)]);
#ifndef NODP
		print_inode(tinode);
#endif
		close(fd);
	}

	dp("Thread #%d finished!\n",tid);
	return NULL;
}

void do_restore(int fd) {
	char fname[FILE_NAME_SIZE];
	unsigned short mode;
	ino_t ino, pino, bino, loid;
	unsigned int uid, gid;
	size_t size, /*bsize,*/ off_f, off_do, len;
	time_t at,mt,ct;
	uint32_t ne, hid;
	uint8_t is_shared;
	struct inode *inode;
	struct dobject *dobj;
	int i;

	dp("Restoring inode map\n");
	while( read(fd, &ino, sizeof(ino_t)) ) 
	{
		read(fd, &mode, sizeof(mode_t));
		read(fd, &uid, sizeof(uid_t));
		read(fd, &gid, sizeof(gid_t));
		read(fd, &size, sizeof(size_t));
		read(fd, &at, sizeof(time_t));
		read(fd, &mt, sizeof(time_t));
		read(fd, &ct, sizeof(time_t));
		read(fd, fname, FILE_NAME_SIZE);
		read(fd, &pino, sizeof(ino_t));
		read(fd, &bino, sizeof(ino_t));
	//	read(fd, &bsize, sizeof(size_t));
		read(fd, &is_shared, sizeof(uint8_t));

		inode = create_inode(fname, ino, mode, pino, bino, uid, gid, size);
		
		read(fd, &ne, sizeof(uint32_t));

		for(i = 0 ; i < ne ; i++) {
			read(fd, &hid, sizeof(uint32_t));
			read(fd, &loid, sizeof(ino_t));
			read(fd, &off_f, sizeof(size_t));
			read(fd, &off_do, sizeof(size_t));
			read(fd, &len, sizeof(size_t));

			dobj = get_dobject(hid, loid, inode, 1);
			pado_write(inode, dobj, off_f, off_do, len);
		}

		set_inode_aux(inode, at, mt, ct, /* bsize,*/ is_shared);
#ifndef NODP
		print_inode(inode);
#endif
	}
}
