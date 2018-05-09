#include"inserver.h"
#include"uthash.h"

ino_t inodebase;

pthread_mutex_t ino_mut = PTHREAD_MUTEX_INITIALIZER;


struct inode_hash_item {
	ino_t key;
	struct inode *value;
	UT_hash_handle hh;
};

struct inode_hash_item *inode_map = NULL;

struct do_hash_item {
	struct do_addr key;
	struct dobject *value;
	UT_hash_handle hh;
};

struct do_hash_item *do_map = NULL;

void init_inode_container(uint32_t nodeid) {
	inodebase = ((ino_t)nodeid <<32) + 1;
}

struct inode *get_new_inode() {
	return (struct inode *)calloc(1, sizeof(struct inode));
}

int release_inode(struct inode *inode) {
    if( inode->flayout == NULL ) {
		free(inode);
		return 1;
	} else {
		return -1;
	}
}

struct inode *create_inode(const char *name, short mode, ino_t pino, ino_t bino, 
                           unsigned int uid, unsigned int gid, size_t size) 
{
	struct inode *new_inode = get_new_inode();
	
	pthread_mutex_lock(&ino_mut);
	new_inode->ino = inodebase++;
	pthread_mutex_unlock(&ino_mut);

	clock_gettime(CLOCK_REALTIME, &(new_inode->ctime) );
	new_inode->mtime = new_inode->ctime;
	new_inode->atime = new_inode->ctime;

	strncpy(new_inode->name,name, FILE_NAME_SIZE);
	new_inode->mode = mode;

	new_inode->uid = uid;
	new_inode->gid = gid;

	new_inode->parent_ino = pino;

	new_inode->base_ino = bino;
	new_inode->base_size = size;

	new_inode->shared_ino = 0;
	
	new_inode->size = size;

	pthread_rwlock_init(&new_inode->rwlock, NULL);

	new_inode->flayout = NULL;	

	struct inode_hash_item *hi = malloc(sizeof(struct inode_hash_item));
	hi->key = new_inode->ino;
	hi->value = new_inode;

	HASH_ADD(hh, inode_map, key, sizeof(ino_t), hi);

	return new_inode;
}

struct inode *get_inode(ino_t ino) {
	struct inode_hash_item *hi;

	HASH_FIND(hh, inode_map, &ino, sizeof(ino_t), hi);

	return hi->value;
}

struct extent *get_extent(struct dobject *dobj, size_t start, size_t offset, size_t length) {
	struct extent *newone;
	newone = calloc(sizeof(struct extent),1);

	newone->depth = 1;
	newone->status = VALID;
	newone->dobj = dobj;

	newone->start = start;
	newone->offset = offset;
	newone->length = length;

	newone->next_ref = dobj->refs;
	newone->prev_ref = NULL;
	if( dobj->refs ) {
		dobj->refs->prev_ref = newone;
	}
	dobj->refs = newone;

	return newone;
}

void release_extent(struct extent *target) {
	if( target->prev_ref ) {
		target->prev_ref->next_ref = target->next_ref;
	} else {
		target->dobj->refs = target->next_ref;
	}
	if( target->next_ref ) {
		target->next_ref->prev_ref = target->prev_ref;
	}

	target->status = DELETED;
	free(target);
}

struct dobject *get_dobject(uint32_t host_id, ino_t loid) {
	struct dobject *res;
	struct do_hash_item *hi;

	struct do_addr key;
	key.host_id = host_id;
	key.loid = loid;

	HASH_FIND(hh, do_map, &key, sizeof(struct do_addr), hi);

	if( hi == NULL ) {
		res = malloc(sizeof(struct dobject));
		res->addr.host_id = host_id;
		res->addr.loid = loid;
		res->refs = NULL;

		hi = malloc(sizeof(struct do_hash_item));
		hi->key.host_id = host_id;
		hi->key.loid = loid;
		hi->value = res;

		HASH_ADD(hh, do_map, key, sizeof(struct do_addr), hi);
	} else {
		res = hi->value;
	}

	return res;
}

int release_dobject(struct dobject *dobj) {
	if ( dobj == NULL ) return -1;
	if ( dobj->refs != NULL ) return -2;

	struct do_hash_item *hi;

	struct do_addr key;
	key.host_id = dobj->addr.host_id;
	key.loid = dobj->addr.loid;

    HASH_FIND(hh, do_map, &key, sizeof(struct do_addr), hi);

	if( hi == NULL ) return -3;		

	HASH_DELETE(hh, do_map, hi);
	return 0;
}

void pado_write(struct inode *inode, struct dobject *dobj, size_t start, size_t off, size_t len) {
	pthread_rwlock_wrlock(&inode->rwlock);

	struct extent *new_ext = get_extent(dobj, start, off, len);

	if( off + len > inode->size ) {
		inode->size = off + len;
	}

	if( inode->flayout == NULL ) {
		inode->flayout = new_ext;
		pthread_rwlock_unlock(&inode->rwlock);
		return;
	}
}

struct extent *find_start_extent(struct inode *inode, size_t loc) {
	pthread_rwlock_rdlock(&inode->rwlock);
	struct extent *cur = inode->flayout;

	size_t start,end;

	while( cur != NULL ) {
		start = cur->start;
		end = cur->start + cur->length;
		if( start <= loc && loc < end ) {
			break;
		}
		if( loc < start && cur->left != NULL ) {
			cur = cur->left;
			continue;
		}
		if( loc < start && cur->left == NULL ) {
			break;
		}
		if( end <= loc && cur->right != NULL ) {
			cur = cur->right;
			continue;
		}
		if( end <=loc && cur->right == NULL ) {
			cur = cur->next;
			break;
		}
	}
	pthread_rwlock_unlock(&inode->rwlock);
	return cur;
}
