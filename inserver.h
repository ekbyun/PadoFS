#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>
#include<pthread.h>

#define FILE_NAME_SIZE	256

struct do_addr {
	uint32_t host_id;	//host address(or id) where the real data is stored, 0 means the POSIX base FS 
	ino_t loid;			//local inode number of real data in the host or POSIX file, 0 means hole
};

struct dobject {
	struct do_addr addr;
	struct extent *refs;
};

struct extent {
	uint32_t depth;
	enum {CLEARED, VALID, DELETED, FIXED} status;
	struct dobject *dobj;
	size_t start;		//location of this extent in this file 
	size_t offset;		//location of this extent in the source object 
	size_t length;
	struct extent *parent;
	struct extent *left;
	struct extent *right;
	struct extent *prev;
	struct extent *next;
	struct extent **pivotp;

	struct extent *prev_ref;
	struct extent *next_ref;
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

struct extent *get_extent(struct dobject *,size_t,size_t,size_t);
void release_extent(struct extent *);

struct dobject *get_dobject(uint32_t, ino_t);
int release_dobject(struct dobject *);

void pado_write(struct inode *, struct dobject *, size_t, size_t, size_t);

struct extent *find_start_extent(struct inode *, size_t loc);
