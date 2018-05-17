#include"inserver.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NUM_WORKER_THREAD	4

#define	FIFOFILE	"/tmp/pado_test_fifo"

void print_extent(struct extent *);

int main(int argc, char **argv) 
{
//	printf("sizeof inode = %ld\n", sizeof(struct inode) );
//	printf("sizeof extent = %ld\n", sizeof(struct extent) );
//	printf("sizeof object = %ld\n", sizeof(struct dobject) );

	//test 
	init_inode_container(0);

	struct inode *inode = create_inode("test.txt",01755,0,11,20,21,1024);
	struct inode *inode2 = create_inode("test2.txt",01755,0,12,20,21,512);

	struct inode *getinode = get_inode(inode->ino);
	inode = getinode;
	printf("%s pino=%ld size=%ld, base_ino=%ld\n", getinode->name, getinode->ino, getinode->size, getinode->base_ino);
	getinode = get_inode(inode2->ino);
	inode2 = getinode;
	printf("%s pino=%ld size=%ld, base_ino=%ld\n", getinode->name, getinode->ino, getinode->size, getinode->base_ino);


	struct dobject *dobj;

	char com;
	uint32_t hid;
	ino_t loid;
	size_t start, end;
	int fd;

	printf("----------------------------------------------\n");
	while(1) {
	//	printf("Insert command:");
		scanf("%c %d %ld %ld %ld",&com, &hid, &loid, &start, &end);
		if( com == '#' ) continue;
		if( com == 'q' || com == 'Q' ) break;
		switch(com) {
			case 'W':	//write
				if( end <= start ) {
					printf("INVALID INPUT!!\n");
					break;
				}
				printf("writing do(hid=%d,loid=%ld) to offset= %ld, length= %ld ...\n",hid, loid,start, end-start); 
				dobj= get_dobject(hid, loid, 1);
				pado_write(inode, dobj, start, start, end-start);
				printf("size = %ld\n", inode->size);
				print_extent( find_start_extent(inode, 0) );			
				break;
			case 'R':	//read
				printf("reading tree from %ld to %ld , size = %ld\n",start, end, inode->size);
				mkfifo(FIFOFILE, 0666);
				fd = open(FIFOFILE, O_RDWR);
				pado_read(inode, fd, 1, start, end);
				print_extent( pado_clone_tmp(inode, fd, start, end, NULL) );
				close(fd);
				remove(FIFOFILE);
				break;
			case 'C':	//clone
				printf("cloning tree of inode1 (size=%ld) ranged from %ld to  %ld, to inode2 from %ld...\n", inode->size, start, end, loid);
				// TODO : implement cloning test code
				printf(" size = %ld\n",inode2->size);
				break;
			case 'T':	//truncate
			case 'S':	//set inode
			case 'G':	//get inode
			case 'M':	// make new inode
			case 'D':	// delete inode
				printf("UNDER CONSTRUCTION com=%c\n",com);
		}
	}
	return 0;
}

void print_extent(struct extent *ext) {
	int i = 0;
	size_t prev_end = 0;
	printf("-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -\n");
	while( ext ) {
		for(i=0;i<ext->depth-1;i++) {
			printf("                 ");
		}
		if( prev_end > ext->off_f) {
			printf("ERROR !!!!!! \n");
		}
		prev_end = ext->off_f + ext->length;
		printf("%lx,%-8ld,%-8ld[%d,%ld,%ld,%ld][%lx,%lx,%lx][%lx,%lx]\n",(long)ext,ext->off_f,prev_end,ext->dobj->addr.host_id,ext->dobj->addr.loid,ext->dobj->addr.pado_ino,ext->off_do,(long)ext->parent,(long)ext->left,(long)ext->right,(long)ext->prev,(long)ext->next);
		ext = ext->next;
	}
	printf("----------------------------------------------\n");
}
