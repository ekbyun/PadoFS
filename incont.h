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
#define print_all()
#endif

#define FILE_NAME_SIZE	256
#define	MAX(x,y)	( (x) > (y) ? (x) : (y) )
#define	MIN(x,y)	( (x) < (y) ? (x) : (y) )
#define	ABS(x)		( ( (x) < 0 ) ? -(x) : (x) )

#define DEPTH(x)    ( (x) ? (x->depth) : 0 )

struct do_addr {
	uint32_t host_id;	//host address(or id) where the real data is stored, 0 means the POSIX base FS 
	ino_t loid;			//local inode number of real data in the host or POSIX file, 0 means hole
};

struct dobject {
	struct do_addr addr;
	struct extent *refs;
	struct inode *inode;
	pthread_mutex_t reflock;
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

typedef unsigned long ino_t;

struct inode {	//type may need to be changed defined in kernel /include/linux/types.h 
	ino_t ino;	//inode number ino_pado
	unsigned short mode;	//umode_t
	unsigned int uid;	//uid_t
	unsigned int gid;	//gid_t
	size_t size;		//loff_t
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;

	struct extent *flayout;

	char name[FILE_NAME_SIZE];

	ino_t parent_ino;

	ino_t base_ino;
	size_t base_size;

	ino_t shared_ino;

	struct dobject *do_map;
	pthread_rwlock_t dmlock;	//lock for dobject hash-map

	pthread_rwlock_t rwlock;	//lock for read/write operations
	uint32_t num_exts;
};

void init_inode_container(uint32_t, ino_t);

struct inode *get_new_inode();
int release_inode(struct inode *);

struct inode *create_inode(const char *,ino_t, short,ino_t,ino_t,unsigned int,unsigned int, size_t);
struct inode *get_inode(ino_t);
void set_inode_aux(struct inode *,time_t,time_t,time_t,size_t,ino_t);

struct extent *create_extent(struct dobject *,size_t,size_t,size_t);
void release_extent(struct extent *, int);
void insert_extent_list(struct extent *,struct extent *,struct extent *);
void replace_extent(struct extent *,struct extent *);

struct dobject *get_dobject(uint32_t, ino_t, struct inode *);
int remove_dobject(struct dobject *, int);

void pado_write(struct inode *, struct dobject *, size_t, size_t, size_t);
void pado_truncate(struct inode *, size_t);
void pado_del_range(struct inode *, size_t, size_t);
void pado_clone(struct inode *, int, size_t, size_t);

void pado_read(struct inode *,int ,int, size_t, size_t);
void pado_getinode(struct inode *, int);
void pado_getinode_all(struct inode *,int);
void do_backup(int);

void replace(struct inode *, struct extent*, struct extent *,size_t, size_t);
void remove_extent(struct extent *, int);

struct extent *find_start_extent(struct inode *, size_t loc);

void rebalance(struct extent *);

void check_extent(struct extent *,int, int);

void test_main(void);
#ifndef NODP
void print_inode(struct inode *);
void print_all(void);
#endif
