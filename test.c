#include"incont.h"
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>

#define NUM_WORKER_THREAD	4

#define	FIFOFILE	"/tmp/pado_test_fifo"

void print_extent(struct extent *, int, uint32_t);
void check_bst(struct extent *, size_t *, size_t *);

void print_inode(struct inode *inode) {
	size_t min = 0,max = 0;
	pthread_rwlock_rdlock(&inode->rwlock);
	dp("printing inode %s, ino=%ld, mode=%o, uid/gid=%d/%d\n",inode->name, inode->ino, inode->mode,inode->uid, inode->gid);
	print_extent( find_start_extent(inode, 0) , 0, inode->num_exts);			
	check_bst(inode->flayout, &max, &min); 
	if( inode->flayout) assert(inode->size >= max);
	dp("CHECK_BST SUCCESS min= %ld, max= %ld, size= %ld\n",min,max,inode->size);
	dp("----------------------------------------------\n");
	pthread_rwlock_unlock(&inode->rwlock);
}

void test_main() {

	struct inode *inode = get_inode(1);
	if( inode == NULL ) 
		inode = create_inode("test.txt",0,01755,0,11,20,21,1024);
	dp("%s pino=%ld size=%ld, base_ino=%ld\n", inode->name, inode->ino, inode->size, inode->base_ino);
	struct inode *inode2 = get_inode(2);
	if( inode2 == NULL )
		inode2 = create_inode("test2.txt",0,01755,0,12,20,21,512);
	dp("%s pino=%ld size=%ld, base_ino=%ld\n", inode2->name, inode2->ino, inode2->size, inode2->base_ino);


	struct dobject *dobj;

	char com[4];
	uint32_t hid;
	ino_t loid;
	size_t start, end;
	int fd;
	struct inode *ti;

	srand(time(NULL));

	dp("----------------------------------------------------------------------------------------------------------------------------\n");
///*	
	while(1) {
		printf("Insert command:");
		scanf("%s %d %ld %ld %ld",com, &hid, &loid, &start, &end);
//*/
		/*
	int i;
	for( i = 0 ; i < 100000 ; i++ ) {
		if( i % 10 < 10 ) {
			com[0]='W';
			com[1]='\0';
			hid = rand() % 5;
			loid = rand() % 128;
			start = rand()%10000;
			end = start + rand()%100;
		} else {
			com[0]='C';
			com[1]='\0';
			loid = rand()%5000;
			start = rand()%5000;
			end = start+ rand()%5100;
		}
*/
		if( com[0] == '#' ) continue;
		if( com[0] == 'q' || com[0] == 'Q' ) break;
		switch(com[0]) {
			case 'W':	//write
				if( end <= start ) {
					dp("INVALID INPUT!!\n");
					break;
				}
				dp("writing do(hid=%d,loid=%ld) to offset= %ld, length= %ld ...\n",hid, loid,start, end-start); 
				dobj= get_dobject(hid, loid, inode);
				pado_write(inode, dobj, start, start, end-start);
				print_inode(inode);
				break;
			case 'R':	//read
				ti = get_inode(loid);
				if( ti == NULL ) {
					dp("There is no inode with ino = %ld\n", loid);
				} else {
					dp("reading tree of inode (ino = %ld) from %ld to %ld , size = %ld\n",ti->ino, start, end, ti->size);
					print_inode(ti);
				}
				break;
			case 'C':	//clone
				dp("cloning tree of inode1 (size=%ld) ranged from %ld to  %ld, to inode2 from %ld...\n", inode->size, start, end, loid);
				mkfifo(FIFOFILE, 0666);
				fd = open(FIFOFILE, O_RDWR);
				pado_read(inode, fd, 1, start, end);
				pado_clone(inode2, fd, loid, loid+(end-start));
				close(fd);
				remove(FIFOFILE);
				print_inode( inode2 );
				break;
			case 'T':
				dp("Truncate inode1, to size %ld ...\n", end);
				pado_truncate(inode, end);
				print_inode(inode);
				break;
			case 'D':	//delete range
				dp("delete range inode#%d, from %ld to %ld ...\n", hid, start,end);
				if( hid == 1 ) {
					ti = inode;
				} else {
					ti = inode2;
				}
				pado_del_range(ti, start, end);
				print_inode(ti);
				break;
			default:
				dp("INVALID COMMAND. com=%s\n",com);
		}
	}
}

void print_extent(struct extent *ext, int clone, uint32_t num_exts) {
	uint32_t num = 0;
	struct extent *h;
	dp("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -\n");
	while( ext ) {
		for(h = ext ; h->parent ; h = h->parent ) {
			dp("                 ");
		}
		dp("%lx,%-8ld,%-8ld[%d,%ld,%ld,%ld][%d,%lx,%lx,%lx][%lx,%lx]\n",(long)ext,ext->off_f,ext->off_f+ext->length,ext->dobj->addr.host_id,ext->dobj->addr.loid,ext->dobj->inode->ino,ext->off_do,ext->depth,(long)ext->parent,(long)ext->left,(long)ext->right,(long)ext->prev,(long)ext->next);

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
