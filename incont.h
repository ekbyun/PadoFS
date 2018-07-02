#include<sys/socket.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>
#include<pthread.h>
#include"uthash.h"

#include<assert.h>

#ifndef NODP
#define dp(fmt,args...) printf( fmt, ## args )
#define dlp(fmt,args...) printf( "[%s %d]" fmt, __FILE__,__LINE__, ## args )
#else
#define dp(fmt,args...)
#define dlp(fmt,args...)
#endif

#define FILE_NAME_SIZE	255
#define	MAX(x,y)	( (x) > (y) ? (x) : (y) )
#define	MIN(x,y)	( (x) < (y) ? (x) : (y) )
#define	ABS(x)		( ( (x) < 0 ) ? -(x) : (x) )

#define DEPTH(x)    ( (x) ? (x->depth) : 0 )

#define MAX_CLI_SOCKET	4096
#define MS_ADDR_DEF		"127.0.0.1"
#define MS_PORT_DEF		7183
#define DOS_ETH_PORT	1915
#define DOS_ADDR_DEF	"127.0.0.1"

#define SHARED	0x01
#define DELETED	0x02	
#define DLOCKED	0x04

#define IS_SHARED(x)	((x)->flags & SHARED)
#define SET_SHARED(x)	((x)->flags = (x)->flags | SHARED)
#define UNSET_SHARED(x)	((x)->flags = (x)->flags & ~SHARED)

#define IS_DELETED(x)	((x)->flags & DELETED)
#define SET_DELETED(x)	((x)->flags = (x)->flags | DELETED)

#define IS_DLOCKED(x)	((x)->flags & DLOCKED)
#define SET_DLOCKED(x)	((x)->flags = (x)->flags | DLOCKED)
#define UNSET_DLOCKED(x)	((x)->flags = (x)->flags & ~DLOCKED)

typedef ino_t	loid_t;

struct do_addr {
	uint32_t host_id;	//host address(or id) where the real data is stored, 0 means the POSIX base FS 
	loid_t loid;			//local inode number of real data in the host or POSIX file, 0 means hole
};

struct dobject {
	struct do_addr addr;
	struct inode *inode;

	struct extent *refs;
//	pthread_mutex_t reflock;
	uint32_t num_refs;

	UT_hash_handle hh;
};

struct extent {
	struct dobject *dobj;
	size_t off_f;		//location of this extent in this file 
	size_t off_do;		//location of this extent in the source object 
	size_t length;

	struct extent *prev_ref;
	struct extent *next_ref;

	int depth;

	struct extent *parent;
	struct extent **pivot;

	struct extent *left;
	struct extent *right;

	struct extent *prev;
	struct extent *next;
};

//typedef unsigned long ino_t;

struct inode {	//type may need to be changed defined in kernel /include/linux/types.h 
	ino_t ino;	//inode number ino_pado
#ifdef WITH_MAPSERVER
	ino_t base_ino;
#endif
	size_t size;		//loff_t
	
	struct timespec atime;
	struct timespec mtime;

	struct extent *flayout;
	uint32_t num_exts;
	int32_t refcount;

	struct dobject *do_map;

	pthread_rwlock_t alive;	//lock to guarantee the inode being kept alive 
	pthread_rwlock_t rwlock;	//lock for read/write operations

	UT_hash_handle hh;

	uint8_t flags;	// SHARED, 
};

#ifdef WITH_MAPSERVER
void init_inode_container(uint32_t, in_addr_t /*, ino_t*/);
struct inode *create_inode_withms(ino_t *, ino_t, size_t, int *);
#endif
struct inode *create_inode(ino_t, size_t, int *);
struct inode *acquire_inode(ino_t);
int release_inode(struct inode *);
int delete_inode(struct inode *);
void free_inode(struct inode *);

struct extent *create_extent(struct dobject *,size_t,size_t,size_t);
void release_extent(struct extent *,int);		//remove extent from ref list of dobject and free memory
void insert_extent_list(struct extent *,struct extent *,struct extent *);
void replace_extent(struct extent *,struct extent *);
void remove_extent(struct extent *,int);		//remove extent from flayout

struct dobject *acquire_dobject(uint32_t, loid_t, struct inode *, int);
int get_inode_dobj(struct inode *,int);
int read_dobject(struct dobject *, int);
int remove_dobject(struct dobject *, /*int,*/ int);

int pado_write(struct dobject *, size_t, size_t, size_t);
int pado_truncate(struct inode *, size_t);
int pado_del_range(struct inode *, size_t, size_t);
int pado_clone(struct inode *, int, size_t, size_t);
int pado_read(struct inode *,int ,int, size_t, size_t);
int pado_getinode_meta(struct inode *, int);
int pado_getinode_all(struct inode *,int);

void replace(struct inode *, struct extent*, struct extent *,size_t, size_t);
struct extent *find_start_extent(struct inode *, size_t loc);
void rebalance(struct extent *);

void stageout_all(void);
int stageout(struct inode *);

void do_backup(int);

void check_dobject(struct dobject *);
void check_extent(struct extent *,int, int);
void test_main(void);

void get_dos_addr(struct sockaddr_in *);

//#ifndef NODP
void print_inode(struct inode *);
void print_all(void);
//#endif
