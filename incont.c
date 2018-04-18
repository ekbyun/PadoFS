#include"inserver.h"
#include"uthash.h"

ino_t inodebase;

pthread_mutex_t ino_mut = PTHREAD_MUTEX_INITIALIZER;


struct hash_item {
	ino_t key;
	struct inode *value;
	UT_hash_handle hh;
};

struct hash_item *inode_map = NULL;

int init_inode_container(uint32_t nodeid)
{
	inodebase = (ino_t)nodeid <<32;
	return 0;	
}


struct inode *create_inode(const char *name, short mode, ino_t pino, ino_t bino, size_t size) {
	struct inode *new_inode = calloc( 1, sizeof(struct inode) );
	
	pthread_mutex_lock(&ino_mut);
	new_inode->ino = inodebase++;
	pthread_mutex_unlock(&ino_mut);

	clock_gettime(CLOCK_REALTIME, &(new_inode->ctime) );

	strncpy(new_inode->name,name, FILE_NAME_SIZE);
	new_inode->mode = mode;
	new_inode->parent_ino = pino;

	new_inode->baseobj.host_id = 0;
	new_inode->baseobj.loid = bino;
	new_inode->baseobj.refs = NULL;
	new_inode->size = size;

	new_inode->shared_ino = 0;
	
	printf("%s, %d \n",new_inode->name,(int)size );

	pthread_rwlock_init(&new_inode->rwlock, NULL);

	if( size > 0 && bino != 0) {
		new_inode->flayout = get_extent();
		printf("get_extent!!\n");

		new_inode->flayout->depth = 1;
		new_inode->flayout->status = VALID;
		new_inode->flayout->obj = &new_inode->baseobj;
		new_inode->flayout->start = 0;
		new_inode->flayout->end = size;
		new_inode->flayout->offset = 0;
		new_inode->flayout->length = size;
	} else {
		new_inode->flayout = NULL;	
	}

	struct hash_item *hi = malloc(sizeof(struct hash_item));
	hi->key = new_inode->ino;
	hi->value = new_inode;

	HASH_ADD(hh, inode_map, key, sizeof(ino_t), hi);

	return new_inode;
}

struct inode *get_inode(ino_t ino) {
	struct hash_item *hi;

	HASH_FIND(hh, inode_map, &ino, sizeof(ino_t), hi);

	return hi->value;
}
