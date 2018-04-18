#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>
#include<pthread.h>


#define FILE_NAME_SIZE	256

struct object {
	uint32_t host_id;	//host address(or id) where the real data is stored, 0 means the POSIX base FS 
	ino_t loid;			//local inode number of real data in the host or POSIX file, 0 means hole
//	enum {POSIX, PADO} type;
	struct extent *refs;
};


struct extent {
	uint32_t depth;
	enum {CLEARED, VALID, DELETED, FIXED} status;
	struct object *obj;
	size_t start;		//location of this extent in this file 
	size_t end;
	size_t offset;		//location of this extent in the source object 
	size_t length;
	struct extent *parent;
	struct extent *left;
	struct extent *right;
	struct extent *prev;
	struct extent *next;
	struct extent **pivotp;

	struct extent *pref_ref;
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

	struct object baseobj;

	ino_t shared_ino;

	pthread_rwlock_t rwlock;
};


int init_inode_container(uint32_t);
struct inode *create_inode(const char *,short,ino_t,ino_t,size_t);
struct inode *get_inode(ino_t);


int create_node_pool(size_t);

struct extent *get_extent();
void release_extent(struct extent *);
