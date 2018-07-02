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
#ifdef WITH_MAPSERVER
	printf("USAGE: inodeserver [OPTION]... [address]\n");
#else
	printf("USAGE: inodeserver [OPTION]...\n");
#endif
	printf("  options : \n");
	printf("      -p [Port] : listening port number ranged between 20 and 30000. default = %d \n",DEFAULT_PORT);
	printf("      -f [filepath] : filename with path containing previous mapping contents.\n");
	printf("                      If not set, start with an emtpy map\n");
	printf("      -b [filepath] : filename with path to which mapping contents will be backuped when stopped.\n");
	printf("                      default = " BACKUP_FILE_PATH "\n");
#ifdef WITH_MAPSERVER
	printf("      -m [MS addr]  : the address of mapping server in form of IPv4 string. (Default = " MS_ADDR_DEF ")\n");
#endif
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
#ifdef WITH_MAPSERVER
	in_addr_t ms_addr = 0;
#endif
	int infileexist = 0;
	int i;
	int fd;

	while( (c = getopt(argc, argv, "p:f:m:hb:" ))!= -1) {
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
			case 'm':
#ifdef WITH_MAPSERVER
				ms_addr = inet_addr(optarg);
#endif
				break;
			case 'h':
				usage();
		}
	}

#ifdef WITH_MAPSERVER
	uint32_t nodeid = 0;
	if( argc <= optind && infileexist == 0 ) {
		printf("IP address is a mandatory argument.\n");
		usage();
	}

	nodeid = inet_addr(argv[optind]);
	printf("Initializing PADOFS inode server.....nodeid = %d(%s)\n",nodeid, argv[optind]);

	init_inode_container(nodeid, ms_addr);
#endif

	if( infileexist ) {
		printf("Restoring inodes from the file %s\n",infilename);
		//restore hash map from file 
		fd = open( infilename, O_RDONLY );
		if(fd == -1) {
			printf("The backup file does't exist. Starting with an empty map.\n"); 
		} else {
			do_restore(fd);
			close(fd);
		}
#ifndef NODP
		print_all();
#endif
	}
	

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
#ifndef NODP
	int ids[NUM_THREADS];
#endif
	int cret;
	for(i = 0; i < NUM_THREADS; i++ ) {
#ifdef NODP
		cret = pthread_create(&(workers[i]), NULL, worker_thread, NULL);
#else
		ids[i] = i;
		cret = pthread_create(&(workers[i]), NULL, worker_thread, &(ids[i]) );
#endif
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

	ino_t tino;
#ifdef WITH_MAPSERVER
	ino_t bino;
#endif
	size_t off_f, off_do, len;
	uint32_t hid, num;
	loid_t loid;
	struct inode *tinode;
	size_t size;
//	char name[FILE_NAME_SIZE];
	uint8_t flags;
	int free;

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

		free = 0;
		ret = SUCCESS;

		read(fd, &tino, sizeof(ino_t));
		if( com != CREATE_INODE && com != CREATE_OPEN_INODE ) {
			tinode = acquire_inode(tino);
			if( tinode == NULL ) ret = -INVALID_INO;
		} else {
			tinode = NULL;
		}

		switch(com) {
			case WRITE:
				read(fd, &hid, sizeof(hid));
				read(fd, &loid, sizeof(loid));
				read(fd, &off_f, sizeof(off_f));
				read(fd, &off_do, sizeof(off_do));
				read(fd, &len, sizeof(len));
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						ret = pado_write( acquire_dobject(hid, loid, tinode, 1), off_f, off_do, len);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				}
				break;
			case TRUNCATE:
				read(fd, &off_f, sizeof(off_f));
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						ret = pado_truncate(tinode, off_f);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				}
				break;
			case READ_LAYOUT:
				read(fd, &off_f, sizeof(off_f));
				read(fd, &len, sizeof(len));
				read(fd, &flags, sizeof(flags));	//flag=1 means clone source
				if( tinode ) {
					pthread_rwlock_rdlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						ret = pado_read(tinode, fd, flags, off_f, off_f + len);
					}
					pthread_rwlock_unlock( &tinode->rwlock );

					if( ret == -BASE_CLONED ) {
						pthread_rwlock_wrlock( &tinode->rwlock );
						SET_SHARED(tinode);
						pthread_rwlock_unlock( &tinode->rwlock );
						ret = SUCCESS;
					}
				} else {
					hid = 0;
					loid = 0;
					write(fd, &hid, sizeof(uint32_t));
					write(fd, &loid, sizeof(loid_t));
				}
				break;
			case CLONE_TO:
				read(fd, &off_f, sizeof(off_f));
				read(fd, &len, sizeof(len));
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						ret = pado_clone(tinode, fd, off_f, off_f + len);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				}
				break;
			case READ_INODE_META:	
				read(fd, &flags, sizeof(flags));
				if( tinode && flags == 1 ) {	//increase refcount
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						pado_getinode_meta(NULL, fd);
						ret = -IS_DELETED;
					} else {
						tinode->refcount++;
						ret = pado_getinode_meta(tinode, fd);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				} else if( tinode && flags == 0 ) {
					pthread_rwlock_rdlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						pado_getinode_meta(NULL, fd);
						ret = -IS_DELETED;
					} else {
						ret = pado_getinode_meta(tinode, fd);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				} else if( tinode == NULL ) {
					ret = pado_getinode_meta(tinode, fd);
				}
				break;
			case READ_INODE_WHOLE:	
				read(fd, &flags, sizeof(flags));
				if( tinode && flags == 1 ) {	//increase refcount
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						pado_getinode_all(NULL, fd);
						ret = -IS_DELETED;
					} else {
						tinode->refcount++;
						ret = pado_getinode_all(tinode, fd);
					}
					pthread_rwlock_wrlock( &tinode->rwlock );
				} else if( tinode && flags == 0 ) {
					pthread_rwlock_rdlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						pado_getinode_all(NULL, fd);
						ret = -IS_DELETED;
					} else {
						ret = pado_getinode_all(tinode, fd);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				} else if( tinode == NULL ) {
					ret = pado_getinode_all(tinode, fd);
				}
				break;
			case READ_DOBJ:
				read(fd, &hid, sizeof(hid));
				read(fd, &loid, sizeof(loid));
#ifdef WITH_MAPSERVER
				read(fd, &bino, sizeof(bino));
#endif
				
				num = 0;
				if( tinode == NULL ) {
#ifdef WITH_MAPSERVER
					bino = 0;
					write(fd, &bino, sizeof(bino));
#endif
					size = 0;
					write(fd, &size, sizeof(size));
					write(fd, &num, sizeof(num));
					ret = -INVALID_INO;
#ifdef WITH_MAPSERVER
				} else if( tinode->base_ino != bino ) {
					bino = 0;
					write(fd, &bino, sizeof(bino));
					size = 0;
					write(fd, &size, sizeof(size));
					write(fd, &num, sizeof(num));
					ret = -CHANGED_MAPPING;
#endif
				} else {
					pthread_rwlock_wrlock( &tinode->rwlock );
#ifdef WITH_MAPSERVER
					bino = tinode->base_ino;
					write(fd, &bino, sizeof(bino));
#endif
					size = tinode->size;
					write(fd, &size, sizeof(size));

					ret = read_dobject( acquire_dobject(hid, loid, tinode, 0), fd );
					if ( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else if ( IS_SHARED(tinode) ) {
						ret = -SHARED_BASE;
					} else if ( IS_DLOCKED(tinode) ) {
						ret = -IS_DLOCKED;
					} else {
						SET_DLOCKED(tinode);
					}

					pthread_rwlock_unlock( &tinode->rwlock );
				}
				break;
			case RELEASE_DRAIN_LOCK:
				if ( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					dp("acquire write lock for release drain lock tino=%lu\n",tino);
					UNSET_DLOCKED(tinode);
					if( IS_DELETED(tinode) ) ret = -IS_DELETED;
					pthread_rwlock_unlock( &tinode->rwlock );
				} else {
					ret = -INVALID_INO;
				}
				break;
			case REMOVE_DOBJ:	
				read(fd, &hid, sizeof(hid));
				read(fd, &loid, sizeof(loid));
				read(fd, &flags, sizeof(flags));	//flag for unlink
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						if( hid == 0 && loid == 0 ) {
							ret = IS_SHARED(tinode) ? SUCCESS : -INVALID_DOBJECT;
							UNSET_SHARED(tinode);
						} else {
							ret = remove_dobject( acquire_dobject(hid, loid, tinode, 0), flags);
						}
						free = release_inode(tinode);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				}
				break;
			case GET_INODE_DOBJ:
				if( tinode ) {
					pthread_rwlock_rdlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						ret = get_inode_dobj(tinode, fd);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				} else {
					num = 0;
					write(fd, &num, sizeof(uint32_t));
					ret = -INVALID_INO;
				}
				break;
#ifdef WITH_MAPSERVER
			case CREATE_INODE:
				read(fd, &size, sizeof(size_t));
				bino = tino;
				tino = 0;
				tinode = create_inode_withms(&tino, bino, size, &ret);
				write(fd, &tino, sizeof(ino_t));
				break;
#endif
			case CREATE_OPEN_INODE:
				read(fd, &size, sizeof(size_t));
				tinode = create_inode(tino, size, &ret);
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					tinode->refcount++;
					pthread_rwlock_unlock( &tinode->rwlock );
				}
				pado_getinode_all(tinode, fd);
				break;
			case DELETE_INODE:
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else if ( tinode->refcount > 0 ) {
						ret = -IS_USED;
					} else {
						free = delete_inode(tinode);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				} else {
					ret = -INVALID_INO;
				}
				break;
			case UNREF:
				if( tinode ) {
					pthread_rwlock_wrlock( &tinode->rwlock );
					if( IS_DELETED(tinode) ) {
						ret = -IS_DELETED;
					} else {
						tinode->refcount--;
						if( tinode->refcount < 0 ) { 
							ret = -INVALID_UNREF;
							tinode->refcount = 0;
						}
						free = release_inode(tinode);
					}
					pthread_rwlock_unlock( &tinode->rwlock );
				}
				break;
			default:
				ret = -INVALID_COMMAND;
		}

#ifndef NODP
		dp("return of command %s on file %ld is %s\n", comstr[com], tino, retstr[ABS(ret)]);
		print_inode(tinode);
#endif

		write(fd, &ret, sizeof(ret));
		close(fd);

		if(tinode) {
			pthread_rwlock_unlock( &(tinode->alive) );
		}

		if(tinode && free) {
			free_inode(tinode);
		}
	}

	dp("Thread #%d finished!\n",tid);
	return NULL;
}

void do_restore(int fd) {
	ino_t ino = 0;
#ifdef WITH_MAPSERVER
	ino_t bino;
#endif
	loid_t loid;
	size_t size, off_f, off_do, len;
	time_t at,mt/*,ct*/;
	uint32_t ne, hid;
	uint8_t flags;
	struct inode *inode;
	int i, ret;

	dp("Restoring inode map\n");
	while( read(fd, &ino, sizeof(ino_t)) ) 
	{
#ifdef WITH_MAPSERVER
		bino = ino;
#endif
		read(fd, &size, sizeof(size_t));
		read(fd, &at, sizeof(time_t));
		read(fd, &mt, sizeof(time_t));
		read(fd, &flags, sizeof(uint8_t));

#ifdef WITH_MAPSERVER
		inode = create_inode_withms(&ino, bino, size, &ret);
#else
		inode = create_inode(ino, size, &ret);
#endif
		if( inode ) {
			inode->flags = flags;
			inode->atime.tv_sec = at;
			inode->mtime.tv_sec = mt;
			inode->refcount = 0;
			pthread_rwlock_unlock( &inode->alive );
		} else if( ret != SUCCESS ) {
			dp("Restoring inode for linode #%lu failed due to %s\n", ino, retstr[ABS(ret)]);
		}

		read(fd, &ne, sizeof(uint32_t));
		for(i = 0 ; i < ne ; i++) {
			read(fd, &hid, sizeof(uint32_t));
			read(fd, &loid, sizeof(loid_t));
			read(fd, &off_f, sizeof(size_t));
			read(fd, &off_do, sizeof(size_t));
			read(fd, &len, sizeof(size_t));

			pado_write(acquire_dobject(hid, loid, inode, 1), off_f, off_do, len);
		}
#ifndef NODP
		print_inode(inode);
#endif
	}
}
