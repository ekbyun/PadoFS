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


struct inode *create_inode() {
	struct inode *new = calloc( 1, sizeof(struct inode) );
	
	pthread_mutex_lock(&ino_mut);
	new->ino = inodebase++;
	pthread_mutex_unlock(&ino_mut);

	clock_gettime(CLOCK_REALTIME, &(new->ctime) );

	struct hash_item *hi = malloc(sizeof(struct hash_item));
	hi->key = new->ino;
	hi->value = new;

	HASH_ADD(hh, inode_map, key, sizeof(ino_t), hi);

	return new;
}

struct inode *get_inode(ino_t ino) {
	struct hash_item *hi;

	HASH_FIND(hh, inode_map, &ino, sizeof(ino_t), hi);

	return hi->value;
}
