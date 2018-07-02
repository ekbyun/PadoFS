#include"incont.h"
#include"inserver.h"

struct inode *inode_map = NULL;
pthread_rwlock_t imlock = PTHREAD_RWLOCK_INITIALIZER;

#ifdef WITH_MAPSERVER
ino_t inodebase;
ino_t inodemax;
#define INBASE_LOW_MAX	0xFFFFFFFF
pthread_mutex_t ino_mut = PTHREAD_MUTEX_INITIALIZER;

in_addr_t ms_addr_base;

void init_inode_container(uint32_t nodeid, in_addr_t ms_addr/*ino_t base*/) {
//	if( base >0 )
//		inodebase = base;
//	else
		inodebase = ((ino_t)nodeid <<32) + 1;
	inodemax = inodebase | INBASE_LOW_MAX;

	if( ms_addr > 0 ) {
		ms_addr_base = ms_addr;
	} else {
		ms_addr_base = inet_addr(MS_ADDR_DEF);
	}
	dp("inode_container is created! base inode number = %lu max=%lu dos_addr_base=%u\n", inodebase, inodemax,ms_addr);
}

void get_ms_addr(struct sockaddr_in *addr, ino_t lino){
	addr->sin_family = AF_INET;
	addr->sin_port = htons(MS_PORT_DEF);
	addr->sin_addr.s_addr = ms_addr_base + lino % 1;	//TODO : change
}

struct inode *create_inode_withms(ino_t *ino, ino_t bino, size_t size, int *ret) 
{
	struct inode *new_inode = calloc(1, sizeof(struct inode));
	if( new_inode == NULL ) {
		*ret = -INTERNAL_ERROR;
		return NULL;
	}

	new_inode->base_ino = bino;
	new_inode->size = size;

	pthread_rwlock_init(&new_inode->alive, NULL);
	pthread_rwlock_init(&new_inode->rwlock, NULL);
	new_inode->flags = 0;
	new_inode->do_map = NULL;
	new_inode->flayout = NULL;	
	new_inode->num_exts = 0;
	new_inode->refcount = 1;

	pthread_rwlock_wrlock(&imlock);
	//	try to connect to mapping server
	struct sockaddr_in msaddr;
	int sockfd;
	unsigned char com = 'P';
	get_ms_addr(&msaddr, bino);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( connect(sockfd, (struct sockaddr *)&msaddr, sizeof(msaddr)) < 0 ) {
		pthread_rwlock_unlock(&imlock);
		*ret = -MS_CON_ERROR;
		dp("Creating inode ino = %lu for bino = %lu....failed. MS_CON_ERROR\n", new_inode->ino, bino);
		close(sockfd);
		free(new_inode);
		return NULL;
	}

	// get new ino
	if( inodebase == inodemax ) {
		dp("No more ino in this server\n");
		*ret = -INO_FULL;
		close(sockfd);
		free(new_inode);
		pthread_rwlock_unlock(&imlock);
		return NULL;
	}
	new_inode->ino = inodebase;

	// try to register in mapserver. if already exist, previous ino is returned
	write(sockfd, &com, sizeof(com));
	write(sockfd, &bino, sizeof(ino_t));
	write(sockfd, &new_inode->ino, sizeof(ino_t));
	read(sockfd, ino, sizeof(ino_t));
	close(sockfd);
	if( *ino != new_inode->ino ) {
		pthread_rwlock_unlock(&imlock);
		*ret = -ALREADY_CREATED;
		free(new_inode);
		dp("Creating inode ino = %lu for bino = %lu....failed. ALREADY_CREATED\n", new_inode->ino, bino);
		return NULL;
	}

	inodebase++;
	// insert to the inode hash-map in this server
	HASH_ADD_KEYPTR(hh, inode_map, &(new_inode->ino), sizeof(ino_t), new_inode);
	dp(".(inode_map size=%d)....\n", HASH_COUNT(inode_map) );
	pthread_rwlock_rdlock( &new_inode->alive );
	pthread_rwlock_unlock(&imlock);

	dp("Creating inode ino = %lu for bino = %lu....success\n", new_inode->ino, bino);
	return new_inode;
}
#endif

struct inode *create_inode(ino_t ino, size_t size, int *ret) {
	struct inode *new_inode = NULL;
	pthread_rwlock_wrlock(&imlock);

	HASH_FIND(hh, inode_map, &ino, sizeof(ino_t), new_inode);
	if( new_inode && IS_DELETED(new_inode) ) {
		HASH_DELETE(hh, inode_map, new_inode);
	} else if (new_inode) {
		pthread_rwlock_rdlock(&new_inode->alive);
		pthread_rwlock_unlock(&imlock);
		dp("Create_open inode ino = %lu ....OPENED\n", new_inode->ino);
		*ret = -ALREADY_CREATED;
		return new_inode;
	}	//create new one

	new_inode = calloc(1, sizeof(struct inode));

	if( new_inode == NULL ) {
		*ret = -INTERNAL_ERROR;
		pthread_rwlock_unlock(&imlock);
		return NULL;
	}

	new_inode->ino = ino;
	new_inode->size = size;

	pthread_rwlock_init(&new_inode->alive, NULL);
	pthread_rwlock_init(&new_inode->rwlock, NULL);
	new_inode->flags = 0;
	new_inode->do_map = NULL;
	new_inode->flayout = NULL;	
	new_inode->num_exts = 0;
	new_inode->refcount = 0;

	HASH_ADD_KEYPTR(hh, inode_map, &(new_inode->ino), sizeof(ino_t), new_inode);
	dp(".(inode_map size=%d)....\n", HASH_COUNT(inode_map) );

	pthread_rwlock_rdlock(&new_inode->alive);
	pthread_rwlock_unlock(&imlock);
	dp("Create_open inode ino = %lu ....CREATED\n", new_inode->ino);
	*ret = SUCCESS;
	return new_inode;
}

struct inode *acquire_inode(ino_t ino) {
	struct inode *ret = NULL;

	pthread_rwlock_rdlock(&imlock); 
		HASH_FIND(hh, inode_map, &ino, sizeof(ino_t), ret);
		if( ret == NULL ) {
			pthread_rwlock_unlock(&imlock);
			dp("No inode in this server with ino=%ld\n",ino);
			return NULL;
		}
		pthread_rwlock_rdlock(&ret->alive);
	pthread_rwlock_unlock(&imlock);
	return ret;
}

void free_inode(struct inode *inode) {
#ifndef NDEBUG
	ino_t ino = inode->ino;
#endif
	struct inode *ret = NULL;

	dp("Releasing inode %lu\n",ino);
	pthread_rwlock_wrlock(&imlock); 
	HASH_FIND(hh, inode_map, &(inode->ino), sizeof(ino_t), ret);
	if( ret ) HASH_DELETE(hh, inode_map, inode);
	dp(" inode %lu is removed from inode-map. inode_map size = %d\n",ino,HASH_COUNT(inode_map));
	pthread_rwlock_unlock(&imlock);

	pthread_rwlock_wrlock(&inode->alive); 
	assert(inode->flayout == NULL);
	assert(inode->num_exts == 0);
	assert(inode->refcount == 0);
	pthread_rwlock_destroy(&inode->rwlock);
	pthread_rwlock_wrlock(&inode->alive); 
	
	pthread_rwlock_destroy(&inode->alive);
	free(inode);	
	dp(" inode %lu is freed!\n",ino);
}

// enter this functing with wrlock( rwlock ) acquired and rdlock( alive ) acquired 
int release_inode(struct inode *inode) {	
	if( inode->refcount > 0 || inode->flayout || IS_SHARED(inode) ) {
		return 0;
	}
	dp("Releasing inode %lu\n",inode->ino);

	SET_DELETED(inode);

#ifdef WITH_MAPSERVER
	// unregister mapping from l-p mapping server
	struct sockaddr_in msaddr;
	int sockfd;
	unsigned char com = 'D';
	ino_t bino = inode->base_ino;
	get_ms_addr(&msaddr, bino);
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( connect(sockfd, (struct sockaddr *)&msaddr, sizeof(msaddr)) >= 0 ) {
		write(sockfd, &com, sizeof(com));
		write(sockfd, &bino, sizeof(ino_t));
		read(sockfd, &bino, sizeof(ino_t));
		close(sockfd);
	} else {
		//TODO : handle error
	}
#endif
	return 1;
}

// enter this functing with wrlock( rwlock ) acquired and rdlock( alive ) acquired 
int delete_inode(struct inode *inode) {
	struct dobject *cur,*tmp;

	//remove all dobjects, which may remove reference from distributed data objects  
	HASH_ITER(hh, inode->do_map, cur, tmp) {
		remove_dobject(cur, 1);
	}

	// drain shared basefile
	if( IS_SHARED(inode) ) {
		struct sockaddr_in caddr;
		int fd, ret = SUCCESS;
		unsigned char com = MOVE_SHARED_BASE;

		caddr.sin_family = AF_INET;
		caddr.sin_port = htons(DOS_ETH_PORT);
		caddr.sin_addr.s_addr = inet_addr(DOS_ADDR_DEF);

		fd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(fd, (struct sockaddr *)&caddr, sizeof(caddr) ) < 0 ) {
			perror("ref link fail:");
			return 0;
		}
		write(fd, &com, sizeof(com));
#ifdef WITH_MAPSERVER
		write(fd, &inode->base_ino, sizeof(ino_t));
		write(fd, &inode->ino, sizeof(ino_t));
		write(fd, &inode->base_ino, sizeof(ino_t));
#else 
		write(fd, &inode->ino, sizeof(ino_t));
		write(fd, &inode->ino, sizeof(ino_t));
#endif
		read(fd, &ret, sizeof(ret) );
		close(fd);
		UNSET_SHARED(inode);
	}

	// set deleted and release inode from inode-map 
	return release_inode(inode);
}

int get_inode_dobj(struct inode *inode, int fd) {
	dp("pado get_inode_dobj ino = %ld\n",inode->ino);
	uint32_t count;
	struct dobject *cur, *tmp;

	count = HASH_COUNT( inode->do_map );

	write(fd, &count, sizeof(count));

    HASH_ITER(hh, inode->do_map, cur, tmp) {
		write(fd, &(cur->addr.host_id), sizeof(uint32_t));
		write(fd, &(cur->addr.loid), sizeof(loid_t));
	}
	
	return SUCCESS;
}

struct extent *create_extent(struct dobject *dobj, size_t off_f, size_t off_do, size_t length) 
{
	struct extent *newone;
	// need pooling
	newone = calloc(1, sizeof(struct extent));

	newone->depth = 1;
	newone->dobj = dobj;

	newone->off_f = off_f;
	newone->off_do = off_do;
	newone->length = length;

	newone->next_ref = dobj->refs;
	newone->prev_ref = NULL;
	if( dobj->refs ) {
		dobj->refs->prev_ref = newone;
	}
	dobj->refs = newone;
	dobj->num_refs++;
	dobj->inode->num_exts++;

	dp("a new extent(%lx) is created. depth=%d, do=[%lx,%d,%ld,%ld],off=[%ld,%ld,%ld],ref=[%lx,%lx]\n",(long)newone,newone->depth,(long)newone->dobj,newone->dobj->addr.host_id,newone->dobj->addr.loid,newone->dobj->inode->ino,newone->off_f,newone->off_do,newone->length,(long)newone->prev_ref,(long)newone->next_ref);
	return newone;
}

void release_extent(struct extent *target, int triggered ) 
{
	dp("releasing extent %lx, pn=[%lx,%lx]\n",(long)target,(long)target->prev_ref,(long)target->next_ref );
	struct dobject *dobj = NULL;
		
	if( target->prev_ref ) {
		target->prev_ref->next_ref = target->next_ref;
	} else {
		target->dobj->refs = target->next_ref;
	}
	if( target->next_ref ) {
		target->next_ref->prev_ref = target->prev_ref;
	}
	target->dobj->num_refs--;
	target->dobj->inode->num_exts--;

	if( target->dobj->refs == NULL ) {
		assert( target->dobj->num_refs == 0 );
		dobj = target->dobj;
	}
#ifndef NDEBUG
	check_dobject(target->dobj);
#endif
	// need pooling
	free(target);
	
	if( dobj && triggered ) {
		dp(" trigger removing_dobject %lx\n",(long)dobj);
		remove_dobject(dobj, 1); 
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
	dp("replace_extent %lx with %lx\n",(long)orie,(long)newe);
	newe->depth = orie->depth;
	
	newe->parent = orie->parent;
	newe->pivot = orie->pivot;
	*(newe->pivot) = newe;

	newe->prev = orie->prev;
	newe->next = orie->next;

	if( newe->prev ) newe->prev->next = newe;
	if( newe->next ) newe->next->prev = newe;

	newe->left = orie->left;
	newe->right = orie->right;
	if( newe->left ) {
		newe->left->parent = newe;
		newe->left->pivot = &(newe->left);
	}
	if( newe->right ) {
		newe->right->parent = newe;
		newe->right->pivot = &(newe->right);
	}

	release_extent(orie, 1);

#ifndef NDEBUG
	check_extent(newe,1,1);
#endif
}

//flag : 0 : return null if not exist, 1: create new one if not exist, 2: crete new one and add ref on dos
struct dobject *acquire_dobject(uint32_t host_id, loid_t loid, struct inode *inode, int flag) 
{
	if( inode == NULL ) {
		dp("acquire_dobject receive NULL inode hid=%u loid=%ld\n",host_id,loid);
		return NULL;
	}

	struct dobject *res = NULL;
	struct do_addr addr;
	memset(&addr, 0, sizeof(struct do_addr));

	addr.host_id = host_id;
	addr.loid = loid;

	struct sockaddr_in caddr;
	int fd, ret;
	unsigned char com = ADD_LINK;

	HASH_FIND(hh, inode->do_map, &addr, sizeof(struct do_addr), res);

	if( res == NULL && flag > 0 ) {
		dp("Hash not found.Create new one! hid=%u, loid=%ld, pino=%ld\n", host_id, loid, inode->ino);
		res = calloc(1,sizeof(struct dobject));
		if(res == NULL) return NULL;

		res->addr.host_id = host_id;
		res->addr.loid = loid;
		res->inode = inode;
		res->refs = NULL;
		res->num_refs = 0;

		dp(" add new dobject %lx into hashtable\n",(long)res);
		HASH_ADD_KEYPTR(hh, inode->do_map, &res->addr, sizeof(struct do_addr), res);

		// add refrence link in DOS, called by pado_clone
		if( flag == 2 ) {
			caddr.sin_family = AF_INET;
			caddr.sin_port = htons(DOS_ETH_PORT);
			caddr.sin_addr.s_addr = host_id;

			fd = socket(AF_INET, SOCK_STREAM, 0);
			if( connect(fd, (struct sockaddr *)&caddr, sizeof(caddr) ) < 0 ) {
				dp("Fail to add link in data object server\n");
				HASH_DELETE(hh, inode->do_map, res);
				return NULL;
			}  
			write(fd, &com, sizeof(com));
			write(fd, &loid, sizeof(loid));
			write(fd, &inode->ino, sizeof(ino_t));
#ifdef WITH_MAPSERVER
			write(fd, &inode->base_ino, sizeof(ino_t));
#endif
			read(fd, &ret, sizeof(ret) );
			close(fd);
		}
	} else if ( res == NULL && flag == 0 ) {
		dp("Hash not found. return NULL. hid=%u, loid=%ld, pino=%ld\n", host_id, loid, inode->ino);
		return NULL;
	} else {	// res != NULL
		dp("Hash found!! %lx, hid=%u, loid=%ld, pino=%ld\n", (long)res, host_id, loid, inode->ino);
	}
#ifndef NDEBUG
	check_dobject(res);
#endif

	return res;
}

int remove_dobject(struct dobject *dobj, /*int force, */int unlink) 
{
	if( dobj == NULL ) {
		return -INVALID_DOBJECT;
	}
	dp("removing dobject %lx, h=%u l=%lu p=%lu \n",(long)dobj,dobj->addr.host_id,dobj->addr.loid,dobj->inode->ino);

	HASH_DELETE(hh, dobj->inode->do_map, dobj);
	
	while( dobj->refs ) {
		dp(" removing extent %lx force!!\n",(long)dobj->refs);
		remove_extent(dobj->refs, 0);
	}

	struct sockaddr_in caddr;
	int fd, ret = SUCCESS;
	unsigned char com = REMOVE_LINK;
	if( unlink ) {
		caddr.sin_family = AF_INET;
		caddr.sin_port = htons(DOS_ETH_PORT);
		caddr.sin_addr.s_addr = dobj->addr.host_id;

		fd = socket(AF_INET, SOCK_STREAM, 0);
		dp("connecting to DOS h=%u\n",dobj->addr.host_id);
		if( connect(fd, (struct sockaddr *)&caddr, sizeof(caddr) ) < 0 ) {
			perror("ref link fail:");
			ret = FAILED;
		} else { 
			dp("connected to DOS h=%u\n",dobj->addr.host_id);
			write(fd, &com, sizeof(com));
			write(fd, &dobj->addr.loid, sizeof(loid_t));
			write(fd, &dobj->inode->ino, sizeof(ino_t));
#ifdef WITH_MAPSERVER
			write(fd, &dobj->inode->base_ino, sizeof(ino_t));
#endif
			read(fd, &ret, sizeof(ret) );
			close(fd);
		}
	}

	free( dobj );
	return ret;
}

int read_dobject(struct dobject *dobj, int fd) {
	size_t off_f, off_do, len;
	uint32_t num = 0;
	struct extent *cur;

	if( dobj == NULL ) {
		write(fd, &num, sizeof(num));
		return -INVALID_DOBJECT;
	}

	dp("reading dobject %lx, h=%d l=%ld p=%ld fd=%d\n",(long)dobj,dobj->addr.host_id,dobj->addr.loid,dobj->inode->ino,fd);
#ifndef NDEBUG
	check_dobject(dobj);
#endif	
	
	num = dobj->num_refs;
	write(fd, &num, sizeof(num));

	cur = dobj->refs;
	while( cur ) {
		off_f = cur->off_f;
		off_do = cur->off_do;
		len = cur->length;
		dp("off_f = %ld, len = %ld\n",off_f, len);
		write(fd, &off_f, sizeof(size_t));
		write(fd, &off_do, sizeof(size_t));
		write(fd, &len, sizeof(size_t));
		cur = cur->next_ref;
	}
	return SUCCESS;
}

int pado_write(struct dobject *dobj, size_t off_f, size_t off_do, size_t len) 
{
	if( dobj == NULL ) {
		return -INVALID_DOBJECT;
	}

	struct inode *inode = dobj->inode;
	struct extent *new_ext = create_extent(dobj, off_f, off_do, len);
	
	if( new_ext == NULL ) {
		return -INTERNAL_ERROR;
	}

	inode->size = MAX( inode->size , off_f + len );

	if( inode->flayout ) {
		replace(inode, new_ext, new_ext, off_f, off_f + len);
	} else {
		inode->flayout = new_ext;
		new_ext->pivot = &inode->flayout;
	}

	clock_gettime(CLOCK_REALTIME_COARSE, &(inode->mtime) );
	inode->atime.tv_sec = inode->mtime.tv_sec;

	return SUCCESS;
}

int pado_truncate(struct inode *inode, size_t newsize) 
{
/*	if( inode == NULL ) {
		return -INVALID_INO;
	}	*/
	size_t oldsize = inode->size;

	inode->size = newsize;
	if( newsize < oldsize ) {
		replace(inode, NULL, NULL, newsize, oldsize);
	}

	clock_gettime(CLOCK_REALTIME_COARSE, &(inode->mtime) );
	inode->atime.tv_sec = inode->mtime.tv_sec;
	
	return SUCCESS;
}

/*
int pado_del_range(struct inode *inode, size_t start, size_t end) 
{
	if( inode == NULL ) {
		return -INVALID_INO;
	}

	size_t oldsize = inode->size;

	if( end >= oldsize ) {
		inode->size = MIN(inode->size, start);
	}
	if( start < oldsize ) {
		replace(inode, NULL, NULL, start, MIN(end, oldsize) );
	}

	clock_gettime(CLOCK_REALTIME_COARSE, &(inode->mtime) );
	inode->ctime.tv_sec = inode->mtime.tv_sec;
	inode->atime.tv_sec = inode->mtime.tv_sec;

	pthread_rwlock_unlock(&inode->rwlock);

	return SUCCESS;
}
*/

int pado_clone(struct inode *tinode, int fd, size_t start, size_t end) 
{
/*	if( tinode == NULL ) {
		return -INVALID_INO;
	}
	if( end <= start ) {
		return -INVALID_RANGE;
	}*/

	struct extent *alts = NULL, *tail = NULL;

	uint32_t hid;
	loid_t loid;
	size_t offset, off_do,length, off_f;

	dp("running clone from %ld to %ld\n",start,end); 
	while(1) {
		read(fd, &hid, sizeof(uint32_t));
		read(fd, &loid, sizeof(loid_t));

		if( hid == 0 && loid == 0 ) break;	//end of list

		read(fd, &offset, sizeof(size_t));
		read(fd, &off_do, sizeof(size_t));
		read(fd, &length, sizeof(size_t));

		off_f = start + offset;
		if( off_f > end ) continue;
		if( off_f + length > end ) length = end - off_f;

		if( alts == NULL ) {
			alts = create_extent( acquire_dobject(hid, loid, tinode, 2), off_f, off_do, length);
			tail = alts;
		} else {
			tail->next = create_extent( acquire_dobject(hid, loid, tinode, 2), off_f, off_do, length);
			tail = tail->next;
		}
	}

	tinode->size = MAX(tinode->size, end);
	replace(tinode, alts, tail, start, end);

	clock_gettime(CLOCK_REALTIME_COARSE, &(tinode->mtime) );
	tinode->atime.tv_sec = tinode->mtime.tv_sec;

	return SUCCESS;
}

// this function does not acquire lock. Need to acquire lock at the calling function
void replace(struct inode *inode, struct extent* alts, struct extent *tail, size_t start, size_t end) 
{
	assert( start < end );

	dp("entered replace ino=%ld, alts=%lx, tail=%lx, start=%ld, end=%ld\n",inode->ino, (long)alts,(long)tail, start,end);

	if( inode->flayout == NULL ) {
		dp("empty inode\n");
		if( alts ) {
			dp("set %lx to the root extent\n",(long)alts);
			inode->flayout = alts;
			alts = alts->next;
			inode->flayout->next = NULL;
			inode->flayout->pivot = &inode->flayout;
			start = inode->flayout->off_f + inode->flayout->length;
		} else {
			dp("does noting");
			return;
		}
	}

	struct extent *dh = inode->flayout, *tmp;
	struct extent *tol = NULL, *tor = NULL;
	
	size_t Cs,Ce,head_len;

	dp("finding first_start_ext\n");
	// find the first extent greater than of equal ti the start location, with binary search
	while( dh ) {
		Cs = dh->off_f;
		Ce = Cs + dh->length;
		dp("  cur_dh = %lx, depth=%d, Cs=%ld, Ce=%ld, left=%lx, right=%lx\n",(long)dh,dh->depth,Cs,Ce,(long)dh->left,(long)dh->right);
		if( Cs == start ) {
			dp("     Cs = start!! stop loop\n");
			tor = dh->prev;
			break;
		}
		if( Cs < start && start < Ce ) {
			dp("     Cs < start < Ce , \n");
			head_len = start - Cs;
			if( end < Ce ) {
				dp("  end < Ce, all replaces are taken place in one existing extent. altheadobj=%lx dhobj=%lx\n",(alts)?(long)alts->dobj:0, (long)dh->dobj);
				if( alts && dh->dobj == alts->dobj &&
	    		    dh->off_do + head_len == alts->off_do && start == alts->off_f ) {
					dp("      the first new extent is merged\n");
					alts->off_f = dh->off_f;
					alts->off_do = dh->off_do;
					alts->length += head_len;
				} else {
					dp("      the head part of the existing extent is added to a new extent\n");
					tmp = create_extent( dh->dobj, dh->off_f, dh->off_do, start - Cs );
					tmp->next = alts;
					alts = tmp;
				}
				tor = dh->prev;
				break;
			} else {
				dp("   Ce < end, resize the dh extent and dh survive. The next extent of dh is the first_start_ext\n");
				dh->length = start - Cs;
				tor = dh;
				dh = dh->next;
				break;
			}
		}
		if( start < Cs && dh->left ) {
			dp("   deep into left\n");
			dh = dh->left;
			continue;
		}
		if( start < Cs && dh->left == NULL ) {
			dp("    stop! no more left\n");
			tor = dh->prev;
			break;
		}
		if( Ce <= start && dh->right ) {
			dp("   deep into right\n");
			dh = dh->right;
			continue;
		}
		if( Ce <= start && dh->right == NULL ) {
			dp("    stop! no more right\n");
			tor = dh;
			dh = dh->next;
			break;
		}
	}
	dp("found first_start_ext=%lx",(long)dh);
	if( dh ) dp(", [%ld,%ld,%ld]\n",dh->off_f,dh->off_do,dh->length);
	else dp("\n");

	struct extent *alte, *dnext;

	// check mergeableility of the first extent in the insert list
	if( tor && alts && tor->dobj == alts->dobj && 
	    tor->off_do + tor->length == alts->off_do && tor->off_f + tor->length == alts->off_f ) {
		dp("   first ext is merged!!\n");
		tor->length += alts->length;
		if( alts == tail ) tail = tor;
		alte = alts;
		alts = alts->next;
		release_extent(alte, 1);
	}

	// delete or replace existing extents within the range
	while( dh ) {
		assert( dh->off_f + dh->length > start );
		dp("check to replace or remove extent %lx, C=[%ld,%ld],se=[%ld,%ld]\n",(long)dh,dh->off_f, dh->off_f+dh->length, start,end);

		// chech boundary condition
		if( end <= dh->off_f ) {
			dp(" this extent is out of range. it survives.\n");
			tol = dh;
			break;
		} else if( end < dh->off_f + dh->length ) {	// boundary extents are overlalled. resize it
			size_t diff = end - dh->off_f;
			dh->off_f += diff;
			dh->off_do += diff;
			dh->length -= diff;
			dp(" this boundary extent %lx is partially overlapped. resize it and not delete. now [%ld,%ld,%ld]\n",(long)dh,dh->off_f,dh->off_do,dh->length);
			tol = dh;
			break;
		}

		dp(" execute !!\n");
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
			dp("insert %lx to right of %lx\n",(long)alte,(long)tor);
			if( tor->right ) {
				assert( tor->next && tor->next->left == NULL );
				dp(" having child. insert %lx to left of %lx\n",(long)alte,(long)tor->next);
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
			dp("insert %lx to left of %lx\n",(long)alte,(long)tol);
			if( tol->left ) {
				assert( tol->prev && tol->prev->right == NULL );
				dp(" having child. insert %lx to right of %lx\n",(long)alte,(long)tol->prev);
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
			dp("   last ext is merged!!\n");
			tail->length += tail->next->length;
			remove_extent(tail->next, 1);
		}
	}
}

void remove_extent(struct extent *target, int triggered) 
{
	dp("removing extent %lx dep=%d, plr=[%lx,%lx,%lx], pn=[%lx,%lx]\n",(long)target,target->depth,(long)target->parent,(long)target->left,(long)target->right,(long)target->prev,(long)target->next );
	// unlink target from prev-next list 
	if( target->prev ) {
		target->prev->next = target->next;
	}
	if( target->next ) {
		target->next->prev = target->prev;
	}

	struct extent *alte = NULL, *rebe = NULL, *tmp;

	if( target->left && target->right ) {
		dp(" it has both left and right children. take prev one as another victim\n");
		alte = target->prev;
		assert( alte->right == NULL );

		tmp = alte->left;
		*(alte->pivot) = tmp;
		if( tmp ) {
			dp(" the prev one is not a leaf having left child.\n");
			tmp->parent = alte->parent;
			tmp->pivot = alte->pivot;
		}

		rebe = alte->parent;
		if( rebe == target ) {
			rebe = alte;	
		}

		dp(" info of alt extent %lx dep=%d, plr=[%lx,%lx,%lx], pn=[%lx,%lx]\n",(long)alte,alte->depth,(long)alte->parent,(long)alte->left,(long)alte->right,(long)alte->prev,(long)alte->next );
		dp(" info of tmp extent %lx\n",(long)tmp );
		dp(" info of target extent %lx dep=%d, plr=[%lx,%lx,%lx], pn=[%lx,%lx]\n",(long)target,target->depth,(long)target->parent,(long)target->left,(long)target->right,(long)target->prev,(long)target->next );
		*(target->pivot) = alte;
		alte->pivot = target->pivot;
		alte->parent = target->parent;
		alte->left = target->left;
		alte->right = target->right;
		alte->depth = target->depth;

		if( alte->left ) {
			alte->left->parent = alte;
			alte->left->pivot = &alte->left;
		}
		alte->right->parent = alte;
		alte->right->pivot = &alte->right;

	} else {
		if( target->left && target->right == NULL ) {
			dp(" it has only left child, pull it up\n");
			alte = target->left;
		} else if ( target->left == NULL && target->right ) {
			dp(" it has only right child, pull it up\n");
			alte = target->right;
		} else {
			dp(" it is a leaf!!!!\n");
		}
		if (alte) {
			dp("  it is not a leaf\n");
			alte->parent = target->parent;
			alte->pivot = target->pivot;
		} 
		*(target->pivot) = alte;	//alte is NULL in default
		rebe = target->parent;
	}
	rebalance(rebe);
	release_extent(target, triggered);
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

// flag = 1 mean include base_ino which also means its clone source. 0 means exclude base_ino, a simple read 
int pado_read(struct inode *inode, int fd, int flag, size_t start, size_t end) 
{
/*	if( inode == NULL ) {
		return -INVALID_INO;
	}*/

	uint32_t hid = 0;
	loid_t loid = 0;

	end = MIN(end, inode->size);

	if( start >= end ) {
		write(fd, &hid, sizeof(uint32_t));
		write(fd, &loid, sizeof(loid_t));
		return -INVALID_RANGE;
	}

	struct extent *cur = find_start_extent(inode, start);
#ifdef WITH_MAPSERVER
	ino_t bino = inode->base_ino;
#else
	ino_t bino = inode->ino;
#endif
	size_t off_do, length, off_f, offset, next_off, diff;
	int base_cloned = 0;

	offset = (flag) ? 0 : start;
	off_f = start;
//	if( bino == 0 ) flag = 0;

	while( off_f < end ) {
		next_off = cur ? MIN(end, cur->off_f) : end;
		if( flag && off_f < next_off ) { // write hole with base object 
			hid = 0;
			loid = bino;
			off_do = off_f;
			length = next_off - off_f;
			base_cloned = 1;
		} else if( cur && cur->off_f < end ) { //flag=1, off_f >= next_off, since off_f < end, cur->off_f<=off_f<end
			//current extent exists and is in the range, write it to list
			hid = cur->dobj->addr.host_id;
			loid = cur->dobj->addr.loid;
			diff = MAX( off_f - cur->off_f , 0 );
			off_do = cur->off_do + diff;
			length = MIN( end, cur->off_f + cur->length ) - off_f;
			cur = cur->next;
		} else {	// flag=0, no more extent in range. flag=1, 
			break;
		}
		write(fd, &hid, sizeof(uint32_t));
		write(fd, &loid, sizeof(loid_t));
		write(fd, &offset, sizeof(size_t));
		write(fd, &off_do, sizeof(size_t));
		write(fd, &length, sizeof(size_t));

		off_f += length;
		offset += length;
	}

	hid = 0;
	loid = 0;
	write(fd, &hid, sizeof(uint32_t));
	write(fd, &loid, sizeof(loid_t));

	if(base_cloned) return -BASE_CLONED;

	return SUCCESS;
}

int pado_getinode_meta(struct inode *inode, int fd) 
{
	char zeros[8] = {0,};
	if( inode ) {
		dp("pado getinode ino = %ld\n",inode->ino);
		
#ifdef WITH_MAPSERVER
		write(fd, &inode->base_ino , sizeof(ino_t) );
#endif
		write(fd, &inode->size , sizeof(size_t) );
	
		write(fd, &inode->atime.tv_sec , sizeof(time_t) );
		write(fd, &inode->mtime.tv_sec , sizeof(time_t) );
		write(fd, &inode->flags , sizeof(uint8_t) );
	} else {
#ifdef WITH_MAPSERVER
		write(fd, zeros, sizeof(ino_t));
#endif
		write(fd, zeros, sizeof(size_t));
		write(fd, zeros, sizeof(time_t) );
		write(fd, zeros, sizeof(time_t) );
		write(fd, zeros, sizeof(uint8_t) );
		return -INVALID_INO;
	}
	return SUCCESS;
}

int pado_getinode_all(struct inode *inode, int fd) 
{
	struct extent *cur;
	uint32_t num = 0;;

	pado_getinode_meta(inode,fd);

	if( inode ) {
		dp("pado getinodeall ino = %ld\n",inode->ino);
		write(fd, &(inode->num_exts), sizeof(uint32_t));
		cur = find_start_extent(inode, 0);
		while( cur ) {
			write(fd, &(cur->dobj->addr.host_id), sizeof(uint32_t));
			write(fd, &(cur->dobj->addr.loid), sizeof(loid_t));
			write(fd, &(cur->off_f), sizeof(size_t));
			write(fd, &(cur->off_do), sizeof(size_t));
			write(fd, &(cur->length), sizeof(size_t));
			cur = cur->next;
		}
	} else {
		write(fd, &num, sizeof(uint32_t));
		return -INVALID_INO;
	}

	return SUCCESS;
}

void do_backup(int fd) {
	struct inode *cur, *tmp;
	dp("Backing up whole inodes. num inodes = %d\n", HASH_COUNT(inode_map));

//	write(fd, &inodebase, sizeof(ino_t));
#ifdef WITH_MAPSERVER
	struct sockaddr_in msaddr;
	int sockfd;
	unsigned char com = 'D';
	ino_t bino;
#endif

	pthread_rwlock_rdlock(&imlock);			//grarantee to any inode is created of deleted
	HASH_ITER(hh, inode_map, cur, tmp) {	//for all inodes, write whole inode to the fd
		if( IS_DELETED(cur) ) continue;
#ifdef WITH_MAPSERVER
	// unregister mapping from l-p mapping server
		bino = cur->base_ino;
		get_ms_addr(&msaddr, bino);
	
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(sockfd, (struct sockaddr *)&msaddr, sizeof(msaddr)) >= 0 ) {
			write(sockfd, &com, sizeof(com));
			write(sockfd, &bino, sizeof(ino_t));
			read(sockfd, &bino, sizeof(ino_t));
			close(sockfd);
		}
#else
		write(fd, &cur->ino, sizeof(ino_t));
#endif
		pado_getinode_all(cur, fd);
	}
	pthread_rwlock_unlock(&imlock);
}

void stageout_all() {
	struct inode *cur, *tmp;

	dp("Staging out all inodes\n");

	pthread_rwlock_wrlock(&imlock);
	HASH_ITER(hh, inode_map, cur, tmp) {
		stageout(cur);
	}
	pthread_rwlock_unlock(&imlock);
}

int stageout(struct inode *inode) {
	if( inode == NULL ) {
		return -INVALID_INO;
	}

	dp("Staging out a inode %ld\n",inode->ino);
	struct dobject *cur, *tmp;
	int i, c, ret = SUCCESS, fail = 0;
	struct sockaddr_in caddr;
	int fds[MAX_CLI_SOCKET];
	uint32_t hid;
	loid_t loid;
	unsigned char com;

	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(DOS_ETH_PORT);

	pthread_rwlock_wrlock( &inode->rwlock );

//	if(inode->base_ino == 0 ) {
		// TODO : create base file
//	}
	// copy real data to cloned files if this file is clone source
	if( IS_SHARED(inode) ) {
		caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		fds[0] = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(fds[0], (struct sockaddr *)&caddr, sizeof(caddr) ) < 0 ) {
			pthread_rwlock_unlock( &inode->rwlock );
			printf("[WARNING]Stageout of file %lu failed due to connection error during shared file draining\n", inode->ino);
			return -DOS_CON_ERROR;
		}
		com = DRAIN_SHARED;
#ifdef WITH_MAPSERVER
		loid = inode->base_ino;
#else
		loid = inode->ino;
#endif

		write(fds[0], &com, sizeof(com));
		write(fds[0], &loid, sizeof(loid));

		read(fds[0], &ret, sizeof(ret) );
		close(fds[0] );
		if( ret != SUCCESS ) {
			pthread_rwlock_unlock( &inode->rwlock );
			printf("[WARNING]Stageout of file %lu failed to draining share file\n", inode->ino);
			return -DOS_FAIL;
		}
		UNSET_SHARED(inode);
	}

	c = 0;
	com = DRAIN_DO;
	HASH_ITER(hh, inode->do_map, cur, tmp) {	//fo all dataobject in this inode, drain data 
		hid = cur->addr.host_id;
		loid = cur->addr.loid;
		caddr.sin_addr.s_addr = hid;

		fds[c] = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(fds[c], (struct sockaddr *)&caddr, sizeof(caddr) ) < 0 ) {
			pthread_rwlock_unlock( &inode->rwlock );
			printf("[WARNING]Stageout of file %lu failed due to connection error\n", inode->ino);
			return -DOS_CON_ERROR;
		}

		write(fds[c], &com, sizeof(com));
		write(fds[c], &loid, sizeof(loid));
		read_dobject( cur, fds[c] );	//send mapping list on this data object

		c++;
		if( c == MAX_CLI_SOCKET ) {		//spwan draining processes in parallel and wait to complete 
			for(i = 0 ; i < c ; i++ ) {
				read( fds[i], &ret, sizeof(ret) );
				close( fds[i] );
				if( ret != SUCCESS ) fail++;
			}
			c = 0;
		}
	}

	for(i = 0 ; i < c ; i++ ) {
		read( fds[i], &ret, sizeof(ret) );
		close( fds[i] );
		if( ret != SUCCESS ) fail++;
	}

	SET_DELETED(inode);
	pthread_rwlock_unlock( &inode->rwlock );

	if( fail > 0 ) {
		printf("[WARNING]Stageout of file %lu failed %c!!\n", inode->ino, fail);
		return -DOS_FAIL;
	}
	return SUCCESS;
}

//#ifndef NODP
void print_all() {
	struct inode *cur, *tmp;

	dp("printing whole inodes. num inodes = %d \n",HASH_COUNT(inode_map));

	pthread_rwlock_rdlock(&imlock);
	HASH_ITER(hh, inode_map, cur, tmp) {
		print_inode(cur);
	}
	pthread_rwlock_unlock(&imlock);
}
//#endif

void rebalance(struct extent *ext) {
	int dep_ori, dl, dr, dcl, dcr;
	struct extent *T,*L,*R,*LL,*LR,*RL,*RR;

	while( ext ) 
	{
		dep_ori = ext->depth;

		dl = DEPTH(ext->left);
		dr = DEPTH(ext->right);

		dp(" rebalancing %lx, dep_ori=%d, dl=%d, dr=%d \n",(long)ext, dep_ori, dl,dr);

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
			} else if ( dl < dr && dcl > dcr ) {
				L = ext;
				R = L->right;
				T = R->left;
				LL = L->left;
				LR = T->left;
				RL = T->right;
				RR = R->right;
			} else { // dl < dr && dcl <= dcr
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
			R->depth = MAX( DEPTH(RL), DEPTH(RR) ) + 1;

			T->depth = MAX( L->depth, R->depth ) + 1;

			ext = T;
		} else {
			ext->depth = MAX(dl,dr) + 1;
		}
		if( ext->depth == dep_ori ) break;
		ext = ext->parent;
	}
}

void check_dobject(struct dobject *dobj) {
	uint32_t rc, wc;
	struct extent *ref;
	if( dobj == NULL ) return;

	wc = dobj->num_refs;
	rc = 0;
	
	ref = dobj->refs;

	while(ref) {
		rc++;
		ref = ref->next_ref;
	}
	assert(wc == rc);
}

void check_extent(struct extent *ext, int p, int dur_rep)
{
	if(p) dp("[check_extent]%lx,%-8ld,%-8ld[%d,%ld,%ld,%ld][%d,%lx,%lx,%lx][%lx,%lx]\n",(long)ext,ext->off_f,ext->off_f+ext->length,ext->dobj->addr.host_id,ext->dobj->addr.loid,ext->dobj->inode->ino,ext->off_do,ext->depth,(long)ext->parent,(long)ext->left,(long)ext->right,(long)ext->prev,(long)ext->next);

	assert( ext->length > 0 );
	if( ext->prev_ref ) assert( ext->prev_ref->next_ref == ext );
	else assert( ext->dobj->refs == ext );
	if( ext->next_ref ) assert( ext->next_ref->prev_ref == ext );
    
	assert( *(ext->pivot) == ext );
	assert( ext->parent == NULL || ext->pivot == &ext->parent->left || ext->pivot == &ext->parent->right );
	if( ext->prev ) {
		assert( ext->prev->next == ext );
		assert( ext->prev->off_f + ext->prev->length <= ext->off_f );
	}
	if( ext->next ) {
		assert( ext->next->prev == ext );
		if(dur_rep != 1) assert( ext->next->off_f >= ext->length + ext->off_f );
	}
	if( ext->left ) {
		assert( ext->left->parent == ext );
		assert( ext->left->pivot == &(ext->left) );
	}
	if( ext->right ) {
		assert( ext->right->parent == ext );
		assert( ext->right->pivot == &(ext->right) );
	}
	assert( ext->depth == MAX( DEPTH(ext->left), DEPTH( ext->right) ) + 1 );
	assert( ABS( DEPTH( ext->left ) - DEPTH( ext->right ) ) < 2 );
}
