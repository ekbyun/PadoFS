#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>
#include<pthread.h>

#include<assert.h>
//#defile NDEBUG 

#define FILE_NAME_SIZE	256
#define	MAX(x,y)	( (x) > (y) ? (x) : (y) )
#define	MIN(x,y)	( (x) < (y) ? (x) : (y) )
#define	ABS(x)		( ( (x) < 0 ) ? -(x) : (x) )

struct do_addr {
	uint32_t host_id;	//host address(or id) where the real data is stored, 0 means the POSIX base FS 
	ino_t loid;			//local inode number of real data in the host or POSIX file, 0 means hole
	ino_t pado_ino;
};

struct dobject {
	struct do_addr addr;
	struct extent *refs;
	pthread_mutex_t reflock;
};

struct extent {
	struct dobject *dobj;
	size_t off_f;		//location of this extent in this file 
	size_t off_do;		//location of this extent in the source object 
	size_t length;

	struct extent *prev_ref;
	struct extent *next_ref;

	uint32_t depth;

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

	pthread_rwlock_t rwlock;
};

void init_inode_container(uint32_t);

struct inode *get_new_inode();
int release_inode(struct inode *);

struct inode *create_inode(const char *,short,ino_t,ino_t,unsigned int,unsigned int, size_t);
struct inode *get_inode(ino_t);

struct extent *create_extent(struct dobject *,size_t,size_t,size_t);
void release_extent(struct extent *, int);
void insert_extent_list(struct extent *,struct extent *,struct extent *);
void replace_extent(struct extent *,struct extent *);

struct dobject *get_dobject(uint32_t, ino_t, ino_t);
int remove_dobject(uint32_t, ino_t, ino_t, int);

void pado_write(struct inode *, struct dobject *, size_t, size_t, size_t);
void pado_truncate(struct inode *, size_t);
void pado_clone(struct inode *, int, size_t, size_t);
#ifdef TEST
struct extent *pado_clone_tmp(struct inode *, int, size_t, size_t, struct extent **); 
#endif

void pado_read(struct inode *,int ,int, size_t, size_t);

void replace(struct inode *, struct extent*, struct extent *,size_t, size_t);
void remove_extent(struct extent *, int);

struct extent *find_start_extent(struct inode *, size_t loc);

void insert_left(struct extent *,struct extent *);

void rebalance(struct extent *);
