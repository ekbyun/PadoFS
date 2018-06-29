#include"incont.h"
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>

#define NUM_WORKER_THREAD	4

#define	FIFOFILE	"/tmp/pado_test_fifo"

void print_extent(struct extent *, int, uint32_t);
void check_bst(struct extent *, size_t *, size_t *);

void print_inode(struct inode *inode) {
	size_t min = 0, max = 0;
	if( inode == NULL ) {
		dp("print_inode fail. input inode is NULL\n");
	}
	pthread_rwlock_rdlock(&inode->rwlock);
//	dp("printing inode %s, ino=%lu, mode=%o, uid/gid=%d/%d, flag=%x\n",inode->name, inode->ino, inode->mode,inode->uid, inode->gid, inode->flag);
	dp("printing inode #%lu, mode=%o, uid/gid=%d/%d, flag=%x\n",inode->ino, inode->mode,inode->uid, inode->gid, inode->flags);
	print_extent( find_start_extent(inode, 0) , 0, inode->num_exts);			
	check_bst(inode->flayout, &max, &min); 
	if( inode->flayout) assert(inode->size >= max);
	dp("CHECK_BST SUCCESS min= %ld, max= %ld, size= %ld\n",min,max,inode->size);
	dp("----------------------------------------------\n");
	pthread_rwlock_unlock(&inode->rwlock);
}

void print_extent(struct extent *ext, int clone, uint32_t num_exts) {
	uint32_t num = 0;
	struct extent *h;
	dp("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -\n");
	while( ext ) {
		for(h = ext ; h->parent ; h = h->parent ) {
			dp("                 ");
		}
		dp("%lx,%-8ld,%-8ld[%u,%ld,%ld,%ld][%u,%lx,%lx,%lx][%lx,%lx]\n",(long)ext,ext->off_f,ext->off_f+ext->length,ext->dobj->addr.host_id,ext->dobj->addr.loid,ext->dobj->inode->ino,ext->off_do,ext->depth,(long)ext->parent,(long)ext->left,(long)ext->right,(long)ext->prev,(long)ext->next);

		if( clone != 1 ) {
			check_extent(ext,0,0);
		}
		ext = ext->next;
		num++;
	}
	if( num_exts > 0 ) assert( num == num_exts );
	dp("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -\n");
}

void check_bst(struct extent *cur, size_t *max, size_t *min)
{
	if( !cur ) {
		dp("inode flayout is emtpy!!\n");
		return;
	}
//	dp("_checking %lx\n",(long)cur);

	size_t cmax, cmin;
	size_t Cs, Ce;
	Cs = cur->off_f;
	Ce = cur->off_f + cur->length;

	if( cur->left ) {
		check_bst( cur->left, &cmax, &cmin );
		if( cmax > Cs ) dp("extent %lx is invalid cmax(%ld) of left %lx > Cs(%ld)\n",(long)cur, cmax, (long)cur->left, Cs);
		*min = cmin;
		assert( cmin <= Cs );
	} else {
		*min = Cs;
	}

	if( cur->right ) {
		check_bst( cur->right, &cmax, &cmin );
		if( Ce > cmin ) dp("extent %lx is invalid cmin(%ld) of left %lx < Cs(%ld)\n",(long)cur, cmin, (long)cur->right, Cs);
		*max = cmax;
	} else {
		*max = Ce;
	}
//	dp("[check_bst] extent=%lx, min= %ld, Cs= %ld, Ce= %ld, max= %ld\n",(long)cur,*min,Cs,Ce,*max);
}
