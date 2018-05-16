#include"inserver.h"
#include"uthash.h"


#define DEPTH(x)	( (x) ? (x->depth) : 0 )
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
		pthread_rwlock_destroy(&inode->rwlock);
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

struct extent *create_extent(struct dobject *dobj, size_t off_f, size_t off_do, size_t length) 
{
	struct extent *newone;
	// need pooling
	newone = malloc(sizeof(struct extent));

	memset(newone, 0, sizeof(struct extent));

	newone->depth = 1;
	newone->dobj = dobj;

	newone->off_f = off_f;
	newone->off_do = off_do;
	newone->length = length;

	pthread_mutex_lock(&dobj->reflock);
	newone->next_ref = dobj->refs;
	newone->prev_ref = NULL;
	if( dobj->refs ) {
		dobj->refs->prev_ref = newone;
	}
	dobj->refs = newone;
	pthread_mutex_unlock(&dobj->reflock);

	return newone;
}

void release_extent(struct extent *target, int unref) 
{
	if( unref ) {
		pthread_mutex_lock( &target->dobj->reflock );
		if( target->prev_ref ) {
			target->prev_ref->next_ref = target->next_ref;
		} else {
			target->dobj->refs = target->next_ref;
		}
		if( target->next_ref ) {
			target->next_ref->prev_ref = target->prev_ref;
		}
		pthread_mutex_unlock( &target->dobj->reflock );
	}

	// need pooling
	free(target);
}
void insert_extent_list(struct extent *p, struct extent *t, struct extent *n) 
{
	if( t == NULL ) return;
	t->prev = p;
	t->next = n;
	if( p ) p->next = t;
	if( n ) n->prev = t;
}

void replace_extent(struct extent *orie, struct extent *newe) 
{
	newe->depth = orie->depth;
	
	newe->parent = orie->parent;
	newe->left = orie->left;
	newe->right = orie->right;

	newe->prev = orie->prev;
	newe->next = orie->next;

	newe->pivot = orie->pivot;
	*orie->pivot = newe;

	if( orie->prev ) orie->prev->next = newe;
	if( orie->next ) orie->next->prev = newe;
	if( orie->left ) orie->left->parent = newe;
	if( orie->right ) orie->right->parent = newe;

	release_extent(orie, 1);
}

struct dobject *get_dobject(uint32_t host_id, ino_t loid) 
{
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
		pthread_mutex_init( &res->reflock, NULL);

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

int remove_dobject(uint32_t hid, ino_t loid) 
{
	struct do_hash_item *hi;

	struct do_addr key;
	key.host_id = hid;
	key.loid = loid;

    HASH_FIND(hh, do_map, &key, sizeof(struct do_addr), hi);

	if( hi == NULL ) return -3;		

	struct dobject *dobj = hi->value;
	struct extent *ext;

	pthread_mutex_lock( &dobj->reflock );
	while( dobj->refs ) {
		ext = dobj->refs;
		dobj->refs = ext->next_ref;
		remove_extent(ext, 0);
	}
	pthread_mutex_unlock( &dobj->reflock );

	pthread_mutex_destroy( &dobj->reflock);
	free( dobj );

	HASH_DELETE(hh, do_map, hi);
	free( hi );

	return 0;
}

void pado_write(struct inode *inode, struct dobject *dobj, size_t off_f, size_t off_do, size_t len) 
{
	struct extent *new_ext = create_extent(dobj, off_f, off_do, len);

	pthread_rwlock_wrlock(&inode->rwlock);

	inode->size = MAX( inode->size , off_f + len );

	if( inode->flayout ) {
		replace(inode, new_ext, off_f, off_f + len);
	} else {
		inode->flayout = new_ext;
		new_ext->pivot = &inode->flayout;
	}

	pthread_rwlock_unlock(&inode->rwlock);
}

void pado_truncate(struct inode *inode, size_t newsize) 
{
	pthread_rwlock_wrlock(&inode->rwlock);
	size_t oldsize = inode->size;

	inode->size = newsize;
	if( newsize < oldsize ) {
		replace(inode, NULL, newsize, oldsize);
	}

	pthread_rwlock_unlock(&inode->rwlock);
}

#ifndef TEST
void pado_clone(struct inode *inode, int fd, size_t start, size_t end) 
{
	if( end <= start ) {
		return;
	}
#else
struct extent *pado_clone_tmp(struct inode *inode, int fd, size_t start, size_t end) 
{
#endif 
	struct extent *alts = NULL, *tail = NULL;

	uint32_t hid;
	ino_t loid;
	size_t off_f,off_do,length;

	off_f = start;
	while(1) {
		read(fd, &hid, sizeof(uint32_t));
		read(fd, &loid, sizeof(ino_t));

		if( hid == 0 && loid == 0 ) break;

		read(fd, &off_do, sizeof(size_t));
		read(fd, &length, sizeof(size_t));

		if( hid > 0 && loid == 0 ) {
			off_f += length;
			continue;
		}

		if( alts == NULL ) {
			alts = create_extent( get_dobject(hid, loid), off_f, off_do, length);
			tail = alts;
		} else {
			tail->next = create_extent( get_dobject(hid, loid), off_f, off_do, length);
			tail = tail->next;
		}
		off_f += length;
	}
	assert( off_f <= end );
	
#ifdef TEST 
	return alts;
}

void pado_clone(struct inode *inode, int fd, size_t start, size_t end) 
{
	struct extent *alts;
	alts = pado_clone_tmp(inode, fd, start, end);
#endif 

	pthread_rwlock_wrlock(&inode->rwlock);
	inode->size = MAX(inode->size, end);
	replace(inode, alts, start, end);
	pthread_rwlock_unlock(&inode->rwlock);
}

// this function does not acquire lock. Need to acquire lock at the calling function
void replace(struct inode *inode, struct extent* alts, size_t start, size_t end) 
{
	assert( start < end );

	if( inode->flayout == NULL ) {
		if( alts ) {
			inode->flayout = alts;
			alts = alts->next;
			inode->flayout->next = NULL;
			inode->flayout->pivot = &inode->flayout;
		} else {
			return;
		}
	}

	struct extent *dh = inode->flayout, *tmp;
	struct extent *tol = NULL, *tor = NULL;
	
	size_t Cs,Ce;

	// find the first extent greater than of equal ti the start location, with binary search
	while( dh ) {
		Cs = dh->off_f;
		Ce = Cs + dh->length;
		if( Cs == start ) {
			tor = dh->prev;
			break;
		}
		if( Cs < start && start < Ce ) {
			if( end < Ce ) {
				tmp = create_extent( dh->dobj, dh->off_f, dh->off_do, start - Cs );
				tmp->next = alts;
				alts = tmp;
				tor = dh->prev;
				break;
			} else {
				dh->length = start - Cs;
				tor = dh;
				dh = dh->next;
				break;
			}
		}
		if( start < Cs && dh->left ) {
			dh = dh->left;
			continue;
		}
		if( start < Cs && dh->left == NULL ) {
			tor = dh->prev;
			break;
		}
		if( Ce <= start && dh->right ) {
			dh = dh->right;
			continue;
		}
		if( Ce <= start && dh->right == NULL ) {
			tor = dh;
			dh = dh->next;
			break;
		}
	}


	// delete or replace existing extents within the range
	struct extent *alte, *dnext;
	while( dh ) {
		if( end <= dh->off_f ) {
			tol = dh;
			break;
		} else if( end < dh->off_f + dh->length ) {
			size_t diff = end - dh->off_f;
			dh->off_f += diff;
			dh->off_do += diff;
			dh->length -= diff;
			tol = dh;
			break;
		}

		dnext = dh->next;
		if( alts ) {
			alte = alts;
			alts = alts->next;
			replace_extent(dh, alte);
			tor = alte;
		} else {
			remove_extent(dh, 1);
		}
		dh = dnext;
	}

	// insert remaining altenative extents
	while( alts ) {
		alte = alts;
		alts = alts->next;

		if( tor ) {	//insert to right
			insert_extent_list(tor, alte, tor->next);
			if( tor->right ) {
				assert( tor->next && tor->next->left == NULL );
				tor->next->left = alte;
				alte->parent = tor->next;
				alte->pivot = &alte->parent->left;
				rebalance(tor->next);
			} else {
				tor->right = alte;
				alte->parent = tor;
				alte->pivot = &alte->parent->right;
				rebalance(tor);
			}
			rebalance(alte->parent);
		} else if ( tol ) {	//insert to left
			insert_extent_list(tol->prev, alte, tol );
			if( tol->left ) {
				assert( tol->prev && tol->prev->right == NULL );
				tol->prev->right = alte;
				alte->parent = tol->prev;
				alte->pivot = &alte->parent->right;
				rebalance(tol->prev);
			} else {
				tol->left = alte;
				alte->parent = tol;
				alte->pivot = &alte->parent->left;
				rebalance(tol);
			}
		} else {	// both tol and tor are NULL, error
			assert(0);
		}
	}
}

void remove_extent(struct extent *target, int unref) 
{
	// unlink target from prev-next list 
	if( target->prev ) {
		target->prev->next = target->next;
	}
	if( target->next ) {
		target->next->prev = target->prev;
	}

	struct extent *alte = NULL, *rebe = NULL, *tmp;

	if( target->left && target->right ) {
		alte = target->prev;
		assert( alte->right == NULL );
		rebe = alte->parent;
		tmp = target->prev->left;
		if( tmp ) {
			*(alte->pivot) = tmp;
			tmp->parent = rebe;
			tmp->pivot = alte->pivot;
		}
		*(target->pivot) = alte;
		alte->pivot = target->pivot;
		alte->parent = target->parent;
		alte->left = target->left;
		alte->right = target->right;
		alte->depth = target->depth;

		alte->right->pivot = &alte->right;
		alte->left->pivot = &alte->left;
	} else {
		if( target->left && target->right == NULL ) {
			alte = target->left;
		} else if ( target->left == NULL && target->right ) {
			alte = target->right;
		}
		if (alte) {
			alte->parent = target->parent;
			alte->pivot = target->pivot;
		}
		*(target->pivot) = alte;	//alte is NULL in default
		rebe = target->parent;
	}
	rebalance(rebe);
	release_extent(target,unref);
}


struct extent *find_start_extent(struct inode *inode,size_t start){
	struct extent *cur = inode->flayout;

	size_t Cs,Ce;

	// find the first extent greater than of equal ti the start location, with binary search
	while( cur ) {
		Cs = cur->off_f;
		Ce = Cs + cur->length;
		if( Cs <= start && start < Ce ) {
			break;
		}
		if( start < Cs && cur->left != NULL ) {
			cur = cur->left;
			continue;
		}
		if( start < Cs && cur->left == NULL ) {
			break;
		}
		if( Ce <= start && cur->right != NULL ) {
			cur = cur->right;
			continue;
		}
		if( Ce <= start && cur->right == NULL ) {
			cur = cur->next;
			break;
		}
	}
	return cur;
}

// flag = 1 mean include base_ino. 0 means exclude base_ino
void pado_read(struct inode *inode, int fd, int flag, size_t start, size_t end) {
	struct extent *cur = find_start_extent(inode, start);

	uint32_t hid;
	ino_t loid, bino;
	size_t offset, length, dloc, bsize, diff;

	dloc = start;
	bsize = inode->base_size;
	bino = inode->base_ino;
	if( bino == 0 ) flag = 0;

	while( cur && cur->off_f < end ) {
		if( flag && dloc < cur->off_f ) {
			// write hole with base object 
			if( dloc < bsize ) {
				hid = 0;
				loid = bino;
				offset = dloc;
				length = MIN( cur->off_f, MIN(end, bsize) ) - dloc;
			} else {	//write hole with zero file
				hid = 1;
				loid = 0;
				offset = 0;
				length = MIN(end, cur->off_f) - dloc;
			}
		} else {	//write data object
			hid = cur->dobj->addr.host_id;
			loid = cur->dobj->addr.loid;
			diff = MAX( dloc - cur->off_f , 0 );
			offset = cur->off_do + diff;
			length = MIN( end, cur->off_f + cur->length ) - dloc;
			
			cur = cur->next;
		}
		write(fd, &hid, sizeof(uint32_t));
		write(fd, &loid, sizeof(ino_t));
		write(fd, &offset, sizeof(size_t));
		write(fd, &length, sizeof(size_t));

		dloc += length;
	}
	if( flag && dloc < end && dloc < bsize) {
		hid = 0;
		loid = bino;
		offset = dloc;
		length = MIN( end, bsize ) - dloc;

		write(fd, &hid, sizeof(uint32_t));
		write(fd, &loid, sizeof(ino_t));
		write(fd, &offset, sizeof(size_t));
		write(fd, &length, sizeof(size_t));
	}

	hid = 0;
	loid = 0;
	write(fd, &hid, sizeof(uint32_t));
	write(fd, &loid, sizeof(ino_t));
}

void rebalance(struct extent *ext) {
	uint32_t dep_ori, dl, dr, dcl, dcr;
	struct extent *T,*L,*R,*LL,*LR,*RL,*RR;

	while( ext ) 
	{
		dep_ori = ext->depth;

		dl = DEPTH(ext->left);
		dr = DEPTH(ext->right);

		if( ABS(dl-dr) > 1 ) {
			assert(ABS(dl-dr) == 2);

			dcl = (dl > dr) ? DEPTH(ext->left->left) : DEPTH(ext->right->left);
			dcr = (dl > dr) ? DEPTH(ext->left->right) : DEPTH(ext->right->right);

			if( dl > dr && dcl >= dcr ) {
				T = ext->left;
				L = ext->left->left;
				R = ext;
				LL = L->left;
				LR = L->right;
				RL = T->right;
				RR = R->right;
			} else if ( dl > dr && dcl < dcr ) {
				L = ext->left;
				T = L->right;
				R = ext;
				LL = L->left;
				LR = T->left;
				RL = T->right;
				RR = R->right;
			} else if ( dl < dr && dcl >= dcr ) {
				L = ext;
				R = L->right;
				T = R->left;
				LL = L->left;
				LR = T->left;
				RL = T->right;
				RR = R->right;
			} else { // dl < dr && dcl < dcr
				L = ext;
				T = L->right;
				R = T->right;
				LL = L->left;
				LR = T->left;
				RL = R->left;
				RR = R->right;
			}
			
			T->parent = ext->parent;
			T->pivot = ext->pivot;
			*(T->pivot) = T;
			T->left = L;
			T->right = R;
			
			L->parent = T;
			L->pivot = &T->left;
			L->left = LL;
			if(LL) {
				LL->parent = L;
				LL->pivot = &L->left;
			}
			L->right = LR;
			if(LR) {
				LR->parent = L;
				LR->pivot = &L->right;
			}
			L->depth = MAX( DEPTH(LL), DEPTH(LR) ) + 1;

			R->parent = T;
			R->pivot = &T->right;
			R->left = RL;
			if(RL) {
				RL->parent = R;
				RL->pivot = &R->left;
			}
			R->right = RR;
			if(RR) {
				RR->parent = R;
				RR->pivot = &R->right;
			}
			R->depth = MAX( DEPTH(LL), DEPTH(LR) ) + 1;

			T->depth = MAX( L->depth, R->depth ) + 1;

			ext = T;
		} else {
			ext->depth = MAX(dl,dr) + 1;
		}
		if( ext->depth == dep_ori ) break;
		ext = ext->parent;
	}
}
