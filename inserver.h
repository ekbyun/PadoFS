#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<unistd.h>
#include<time.h>
#include<pthread.h>


#define FILE_PATH_SIZE	248

struct object {
	union {
		char posix_path[FILE_PATH_SIZE];
		struct pado_oid { 
			uint32_t host_id;
			uint32_t loid;
		} pado_oid;
	} addr;
	enum {POSIX, PADO} type;
	uint32_t ref_count;
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
};

typedef unsigned long ino_t;

struct inode {	//type may need to be changed defined in kernel /include/linux/types.h 
	ino_t ino;	//
	unsigned short mode;	//umode_t
	unsigned int uid;	//uid_t
	unsigned int gid;	//gid_t
	size_t size;		//loff_t
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;
	
	struct object baseobj;

	struct extent *flayout;

	pthread_rwlock_t rwlock;
};


int init_inode_container(uint32_t);
struct inode *create_inode(void);
struct inode *get_inode(ino_t);


int create_node_pool(size_t);

