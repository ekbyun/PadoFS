#include"inserver.h"

ino_t inodebase;

pthread_mutex_t ino_mut = PTHREAD_MUTEX_INITIALIZER;

struct inode_hash_item {
	ino_t key;
	struct inode *value;
	UT_hash_handle hh;
};

struct inode_hash_item *inode_map = NULL;
pthread_rwlock_t imlock = PTHREAD_RWLOCK_INITIALIZER;

struct dobject *do_map = NULL;
pthread_rwlock_t dmlock = PTHREAD_RWLOCK_INITIALIZER;

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

	struct inode_hash_item *hi = calloc(sizeof(struct inode_hash_item),1);
	hi->key = new_inode->ino;
	hi->value = new_inode;

	pthread_rwlock_wrlock(&imlock);
	HASH_ADD(hh, inode_map, key, sizeof(ino_t), hi);
	pthread_rwlock_unlock(&imlock);

	return new_inode;
}

struct inode *get_inode(ino_t ino) {
	struct inode_hash_item *hi;
	pthread_rwlock_rdlock(&imlock);
	HASH_FIND(hh, inode_map, &ino, sizeof(ino_t), hi);
	pthread_rwlock_unlock(&imlock);
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

	printf("create_extent depth=%d, do=[%lx,%d,%ld,%ld],off=[%ld,%ld,%ld],ref=[%lx,%lx]\n",newone->depth,(long)newone->dobj,newone->dobj->addr.host_id,newone->dobj->addr.loid,newone->dobj->addr.pado_ino,newone->off_f,newone->off_do,newone->length,(long)newone->prev_ref,(long)newone->next_ref);
	return newone;
}

void release_extent(struct extent *target, int unref) 
{
	printf("releasing extent %lx, flag=%d, pn=[%lx,%lx]\n",(long)target,unref,(long)target->prev_ref,(long)target->next_ref );
	struct dobject *dobj = NULL;
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
		if( target->dobj->refs == NULL ) {
			dobj = target->dobj;
		}
		pthread_mutex_unlock( &target->dobj->reflock );
	}
	// need pooling
	free(target);
	
	if( dobj ) {
		printf(" trigger removing_dobject %lx\n",(long)dobj);
		remove_dobject( dobj, 0 ); 
	}
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

struct dobject *get_dobject(uint32_t host_id, ino_t loid, ino_t pino) 
{
	struct dobject *res, *hi;

	res = malloc(sizeof(struct dobject));
	
	memset(res,0x00,sizeof(struct dobject));
	res->addr.host_id = host_id;
	res->addr.loid = loid;
	res->addr.pado_ino = pino;

	pthread_rwlock_wrlock(&dmlock);
	HASH_FIND(hh, do_map, &res->addr, sizeof(struct do_addr), hi);

	if( hi == NULL ) {
		printf("Hash not found hid=%d, loid=%ld, pino=%ld\n", host_id, loid, pino);

		res->refs = NULL;
		pthread_mutex_init( &res->reflock, NULL);

		printf(" add new dobject %lx into hashtable\n",(long)res);
		HASH_ADD_KEYPTR(hh, do_map, &res->addr, sizeof(struct do_addr), res);
		pthread_rwlock_unlock(&dmlock);
	} else {
		pthread_rwlock_unlock(&dmlock);
		printf("Hash found!! %lx, hid=%d, loid=%ld, pino=%ld\n", (long)hi, host_id, loid, pino);
		free(res);
		res = hi;
	}

	return res;
}

int remove_dobject(struct dobject *dobj, int force) 
{
	printf("removing dobject %lx, h=%d l=%ld p=%ld force=%d\n",(long)dobj,dobj->addr.host_id,dobj->addr.loid,dobj->addr.pado_ino,force);

	struct dobject *hi;
    HASH_FIND(hh, do_map, &dobj->addr, sizeof(struct do_addr), hi);
	if( hi == NULL ) {
		printf(" there is no such dobject\n");
		return -3;
	}
	assert( hi );

	struct extent *ext;

	pthread_mutex_lock( &dobj->reflock );
	
	if( force != 1 && dobj->refs) {
		pthread_mutex_unlock( &dobj->reflock );
		printf("there are existing extents\n");
		return -1;
	}

	while( dobj->refs ) {
		printf(" removing extent %lx force!!\n",(long)dobj->refs);
		ext = dobj->refs;
		dobj->refs = ext->next_ref;
		remove_extent(ext, 0);
	}
	
	pthread_mutex_unlock( &dobj->reflock );

	pthread_mutex_destroy( &dobj->reflock);

	pthread_rwlock_wrlock(&dmlock);
	printf("HASH_DELETE dobj=%lx\n",(long)dobj);
	HASH_DELETE(hh, do_map, dobj);
	pthread_rwlock_unlock(&dmlock);

	free( dobj );

	// TODO : send unlink message to data object server

	return 0;
}

void pado_write(struct inode *inode, struct dobject *dobj, size_t off_f, size_t off_do, size_t len) 
{
	struct extent *new_ext = create_extent(dobj, off_f, off_do, len);

	pthread_rwlock_wrlock(&inode->rwlock);

	inode->size = MAX( inode->size , off_f + len );

	if( inode->flayout ) {
		replace(inode, new_ext, new_ext, off_f, off_f + len);
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
	inode->base_size = MIN(inode->base_size, newsize);
	if( newsize < oldsize ) {
		replace(inode, NULL, NULL, newsize, oldsize);
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
struct extent *pado_clone_tmp(struct inode *inode, int fd, size_t start, size_t end, struct extent **tailp) 
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
			alts = create_extent( get_dobject(hid, loid, inode->ino), off_f, off_do, length);
			tail = alts;
		} else {
			tail->next = create_extent( get_dobject(hid, loid, inode->ino), off_f, off_do, length);
			tail = tail->next;
		}
		off_f += length;
	}
	assert( off_f <= end );
	
	if( tailp ) *tailp = tail;

	if( alts ) assert( start <= alts->off_f );
	if( tail ) assert( end <= tail->off_f + tail->length );

#ifdef TEST 
	return alts;
}

void pado_clone(struct inode *inode, int fd, size_t start, size_t end) 
{
	struct extent *alts, *tail;
	alts = pado_clone_tmp(inode, fd, start, end, &tail);
#endif 

	pthread_rwlock_wrlock(&inode->rwlock);
	inode->size = MAX(inode->size, end);
	replace(inode, alts, tail, start, end);
	pthread_rwlock_unlock(&inode->rwlock);
}

// this function does not acquire lock. Need to acquire lock at the calling function
void replace(struct inode *inode, struct extent* alts, struct extent *tail, size_t start, size_t end) 
{
	assert( start < end );
	printf("entered replace ino=%ld, alts=%lx, tail=%lx, start=%ld, end=%ld\n",inode->ino, (long)alts,(long)tail, start,end);

	if( inode->flayout == NULL ) {
		printf("empty inode\n");
		if( alts ) {
			printf("set %lx to the root extent\n",(long)alts);
			inode->flayout = alts;
			alts = alts->next;
			inode->flayout->next = NULL;
			inode->flayout->pivot = &inode->flayout;
			start = inode->flayout->off_f + inode->flayout->length;
		} else {
			printf("does noting");
			return;
		}
	}

	if( start >= end ) return;

	struct extent *dh = inode->flayout, *tmp;
	struct extent *tol = NULL, *tor = NULL;
	
	size_t Cs,Ce,head_len;

	int ast = 1;
	printf("finding first_start_ext\n");
	// find the first extent greater than of equal ti the start location, with binary search
	while( dh ) {
		Cs = dh->off_f;
		Ce = Cs + dh->length;
		printf("  cur_dh = %lx, depth=%d, Cs=%ld, Ce=%ld, left=%lx, right=%lx\n",(long)dh,dh->depth,Cs,Ce,(long)dh->left,(long)dh->right);
		if( Cs == start ) {
			printf("     Cs = start!! stop loop\n");
			tor = dh->prev;
			break;
		}
		if( Cs < start && start < Ce ) {
			printf("     Cs < start < Ce , \n");
			head_len = start - Cs;
			if( end < Ce ) {
				printf("  end < Ce, all replaces are taken place in one existing extent. altheadobj=%lx dhobj=%lx\n",(alts)?(long)alts->dobj:0, (long)dh->dobj);
				if( alts && dh->dobj == alts->dobj &&
	    		    dh->off_do + head_len == alts->off_do && start == alts->off_f ) {
					printf("      the first new extent is merged\n");
					alts->off_f = dh->off_f;
					alts->off_do = dh->off_do;
					alts->length += head_len;
				} else {
					printf("      the head part of the existing extent is added to a new extent\n");
					tmp = create_extent( dh->dobj, dh->off_f, dh->off_do, start - Cs );
					tmp->next = alts;
					alts = tmp;
				}
				ast = 0;
				tor = dh->prev;
				break;
			} else {
				printf("   Ce < end, resize the dh extent and dh survive. The next extent of dh is the first_start_ext\n");
				dh->length = start - Cs;
				tor = dh;
				dh = dh->next;
				break;
			}
		}
		if( start < Cs && dh->left ) {
			printf("   deep into left\n");
			dh = dh->left;
			continue;
		}
		if( start < Cs && dh->left == NULL ) {
			printf("    stop! no more left\n");
			tor = dh->prev;
			break;
		}
		if( Ce <= start && dh->right ) {
			printf("   deep into right\n");
			dh = dh->right;
			continue;
		}
		if( Ce <= start && dh->right == NULL ) {
			printf("    stop! no more right\n");
			tor = dh;
			dh = dh->next;
			break;
		}
	}
	printf("found first_start_ext=%lx",(long)dh);
	if( dh ) printf(", [%ld,%ld,%ld]\n",dh->off_f,dh->off_do,dh->length);
	else printf("\n");

	struct extent *alte, *dnext;

	// check mergeableility of the first extent in the insert list
	if( tor && alts && tor->dobj == alts->dobj && 
	    tor->off_do + tor->length == alts->off_do && tor->off_f + tor->length == alts->off_f ) {
		printf("   first ext is merged!!\n");
		tor->length += alts->length;
		if( alts == tail ) tail = tor;
		alts = alts->next;
	}

	// delete or replace existing extents within the range
	while( dh ) {
		assert( dh->off_f + dh->length > start );
		printf("check to replace or remove extent %lx, C=[%ld,%ld],se=[%ld,%ld]\n",(long)dh,dh->off_f, dh->off_f+dh->length, start,end);

		// chech boundary condition
		if( end <= dh->off_f ) {
			printf(" this extent is out of range. it survives.\n");
			tol = dh;
			break;
		} else if( end < dh->off_f + dh->length ) {	// boundary extents are overlalled. resize it
			size_t diff = end - dh->off_f;
			dh->off_f += diff;
			dh->off_do += diff;
			dh->length -= diff;
			printf(" this boundary extent %lx is partially overlapped. resize it and not delete. now [%ld,%ld,%ld]\n",(long)dh,dh->off_f,dh->off_do,dh->length);
			tol = dh;
			break;
		}

		printf(" execute !!\n");
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
			printf("insert %lx to right of %lx\n",(long)alte,(long)tor);
			if( tor->right ) {
				assert( tor->next && tor->next->left == NULL );
				printf(" having child. insert %lx to left of %lx\n",(long)alte,(long)tor->next);
				tor->next->left = alte;
				alte->parent = tor->next;
				alte->pivot = &alte->parent->left;
			} else {
				tor->right = alte;
				alte->parent = tor;
				alte->pivot = &alte->parent->right;
			}
			insert_extent_list(tor, alte, tor->next);
			tor = alte;
		} else if ( tol ) {	//insert to left
			printf("insert %lx to left of %lx\n",(long)alte,(long)tor);
			if( tol->left ) {
				assert( tol->prev && tol->prev->right == NULL );
				printf(" having child. insert %lx to right of %lx\n",(long)alte,(long)tor->prev);
				tol->prev->right = alte;
				alte->parent = tol->prev;
				alte->pivot = &alte->parent->right;
			} else {
				tol->left = alte;
				alte->parent = tol;
				alte->pivot = &alte->parent->left;
			}
			insert_extent_list(tol->prev, alte, tol );
		} else {	// both tol and tor are NULL, error
			assert(0);
		}
		rebalance(alte->parent);
	}

	// check mergeability of the last extent added
	if( tail && tail->next && tail->dobj == tail->next->dobj ) {
		if( tail->off_f + tail->length == tail->next->off_f &&
		    tail->off_do + tail->length == tail->next->off_do ) {
			printf("   last ext is merged!!\n");
			tail->length += tail->next->length;
			remove_extent(tail->next, 1);
		}
	}
	printf("%d\n",ast);
//	assert(!ast);
}

void remove_extent(struct extent *target, int unref) 
{
	printf("removing extent %lx dep=%d, plr=[%lx,%lx,%lx], pn=[%lx,%lx]\n",(long)target,target->depth,(long)target->parent,(long)target->left,(long)target->right,(long)target->prev,(long)target->next );
	// unlink target from prev-next list 
	if( target->prev ) {
		target->prev->next = target->next;
	}
	if( target->next ) {
		target->next->prev = target->prev;
	}

	struct extent *alte = NULL, *rebe = NULL, *tmp;

	if( target->left && target->right ) {
		printf(" it has both left and right children. take prev one as another victim\n");
		alte = target->prev;
		assert( alte->right == NULL );

		tmp = alte->left;
		*(alte->pivot) = tmp;
		if( tmp ) {
			printf(" the prev one is not a leaf having left child.\n");
			tmp->parent = alte->parent;
			tmp->pivot = alte->pivot;
		}

		rebe = alte->parent;
		if( rebe == target ) {
			rebe = alte;	
		}

		*(target->pivot) = alte;
		alte->pivot = target->pivot;
		alte->parent = target->parent;
		alte->left = target->left;
		alte->right = target->right;
		alte->depth = target->depth;

		alte->left->parent = alte;
		alte->left->pivot = &alte->left;
		alte->right->parent = alte;
		alte->right->pivot = &alte->right;

	} else {
		if( target->left && target->right == NULL ) {
			printf(" it has only left child, pull it up\n");
			alte = target->left;
		} else if ( target->left == NULL && target->right ) {
			printf(" it has only right child, pull it up\n");
			alte = target->right;
		} else {
			printf(" it is a leaf!!!!\n");
		}
		if (alte) {
			printf("  it is not a leaf\n");
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
	pthread_rwlock_rdlock(&inode->rwlock);
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

	pthread_rwlock_unlock(&inode->rwlock);
}

void rebalance(struct extent *ext) {
	int dep_ori, dl, dr, dcl, dcr;
	struct extent *T,*L,*R,*LL,*LR,*RL,*RR;

	while( ext ) 
	{
		dep_ori = ext->depth;

		dl = DEPTH(ext->left);
		dr = DEPTH(ext->right);

		printf(" rebalancing %lx, dep_ori=%d, dl=%d, dr=%d \n",(long)ext, dep_ori, dl,dr);

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
