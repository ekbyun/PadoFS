#include"incont.h"
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>

#define NUM_WORKER_THREAD	4

#define	FIFOFILE	"/tmp/pado_test_fifo"

void print_extent(struct extent *, int, uint32_t);
void check_bst(struct extent *, size_t *, size_t *);

int main(int argc, char **argv) 
{
	dp("sizeof inode = %ld\n", sizeof(struct inode) );
	dp("sizeof extent = %ld\n", sizeof(struct extent) );
	dp("sizeof object = %ld\n", sizeof(struct dobject) );

	//test 
	
	init_inode_container(0,0);

	struct inode *inode = create_inode("test.txt",0,01755,0,11,20,21,1024);
	struct inode *inode2 = create_inode("test2.txt",0,01755,0,12,20,21,512);

	struct inode *getinode = get_inode(inode->ino);
	inode = getinode;
	dp("%s pino=%ld size=%ld, base_ino=%ld\n", getinode->name, getinode->ino, getinode->size, getinode->base_ino);
	getinode = get_inode(inode2->ino);
	inode2 = getinode;
	dp("%s pino=%ld size=%ld, base_ino=%ld\n", getinode->name, getinode->ino, getinode->size, getinode->base_ino);


	struct dobject *dobj;

	char com[4];
	uint32_t hid;
	ino_t loid;
	size_t start, end, min, max;
	int fd, i;

	srand(time(NULL));
	dp("rand_max = %ld\n",(long)RAND_MAX+1);

	dp("----------------------------------------------------------------------------------------------------------------------------\n");
	for( i = 0 ; i < 100000 ; i++ ) {
//	while(1) {
	//	dp("Insert command:");
	//	scanf("%s %d %ld %ld %ld",com, &hid, &loid, &start, &end);

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
				print_extent( find_start_extent(inode, 0) , 0, inode->num_exts);			
				check_bst(inode->flayout, &max, &min); 
				if( inode->flayout) assert(inode->size >= max);
				dp("CHECK_BST SUCCESS min= %ld, max= %ld, size= %ld\n",min,max,inode->size);
				dp("----------------------------------------------\n");
				break;
			case 'R':	//read
				dp("reading tree from %ld to %ld , size = %ld\n",start, end, inode->size);
				mkfifo(FIFOFILE, 0666);
				fd = open(FIFOFILE, O_RDWR);
				pado_read(inode, fd, 1, start, end);
				print_extent( pado_clone_tmp(inode2, fd, start, end, NULL) , 1, 0);
				close(fd);
				remove(FIFOFILE);
				dp("----------------------------------------------\n");
				break;
			case 'C':	//clone
				dp("cloning tree of inode1 (size=%ld) ranged from %ld to  %ld, to inode2 from %ld...\n", inode->size, start, end, loid);
				mkfifo(FIFOFILE, 0666);
				fd = open(FIFOFILE, O_RDWR);
				pado_read(inode, fd, 1, start, end);
				pado_clone(inode2, fd, loid, loid+(end-start));
				close(fd);
				remove(FIFOFILE);
				print_extent( find_start_extent(inode2, 0) , 0, inode2->num_exts);			
				check_bst(inode2->flayout, &max, &min);
				if( inode2->flayout) assert(inode2->size >= max);
				dp("CHECK_BST SUCCESS min= %ld, max= %ld, size1= %ld size2= %ld\n",min,max,inode->size, inode2->size);
				dp("----------------------------------------------\n");
				break;
			case 'T':	//truncate
				dp("delete range inode#%d, from %ld to %ld ...\n", hid, start,end);
				if( hid == 1 ) {
					pado_del_range(inode, start, end);
					print_extent( find_start_extent(inode, 0) ,0, inode->num_exts);
				} else {
					pado_del_range(inode2, start, end);
					print_extent( find_start_extent(inode2, 0) ,0, inode2->num_exts);
				}
				check_bst(inode->flayout, &max, &min);
				if( inode->flayout) assert(inode->size >= max);
				dp("CHECK_BST SUCCESS min= %ld, max= %ld, size= %ld\n",min,max,inode->size);
				dp("----------------------------------------------\n");
				break;
			case 'S':	//set inode
			case 'G':	//get inode
			case 'M':	// make new inode
			case 'D':	// delete inode
				dp("UNDER CONSTRUCTION. com=%s\n",com);
			default:
				dp("INVALID COMMAND. com=%s\n",com);
		}
	}
	return 0;
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
