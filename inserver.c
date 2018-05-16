#include"inserver.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NUM_WORKER_THREAD	4

#define	FIFOFILE	"/tmp/pado_test_fifo"

void print_extent(struct extent *);

int main(int argc, char **argv) 
{
	printf("sizeof inode = %ld\n", sizeof(struct inode) );
	printf("sizeof extent = %ld\n", sizeof(struct extent) );
	printf("sizeof object = %ld\n", sizeof(struct dobject) );

	//test 
	init_inode_container(0);

	struct inode *inode = create_inode("test.txt",01755,0,11,20,21,201024);
	struct inode *inode2 = create_inode("test2.txt",01755,0,12,20,21,512);


	struct inode *getinode = get_inode(inode->ino);
	inode = getinode;

	printf("%s mode=%o size=%d,%d\n", getinode->name, getinode->mode, (int)getinode->size, (int)getinode->uid);
	printf("inode = %ld\n", getinode->ino);

	getinode = get_inode(inode2->ino);
	inode2 = getinode;

	printf("%s mode=%o size=%d,%d\n", getinode->name, getinode->mode, (int)getinode->size, (int)getinode->gid);
	printf("inode = %ld\n", getinode->ino);


	struct dobject *dobj;

	char com;
	uint32_t hid;
	ino_t loid;
	size_t start, end;
	int fd;

	while(1) {
	//	printf("Insert command:");
		scanf("%c %d %ld %ld %ld",&com, &hid, &loid, &start, &end);
		if( com == '#' ) continue;
		if( com == 'q' || com == 'Q' ) break;
		switch(com) {
			case 'w':
			case 'W':
				if( end <= start ) {
					printf("INVALID INPUT!!\n");
					break;
				}
				printf("writing do(hid=%d,loid=%ld) to offset= %ld, length= %ld ...",hid, loid,start, end-start); 
				dobj= get_dobject(hid, loid);
				pado_write(inode, dobj, start, start, end-start);
				printf("size = %ld\n", inode->size);
				print_extent( find_start_extent(inode, 0) );			
				break;
			case 'r':
			case 'R':
				printf("reading tree from %ld to %ld , size = %ld\n",start, end, inode->size);
				mkfifo(FIFOFILE, 0666);
				fd = open(FIFOFILE, O_RDWR);
				pado_read(inode, fd, 1, start, end);
				print_extent( pado_clone_tmp(inode, fd, start, end) );
				close(fd);
				remove(FIFOFILE);
				break;
			case 'c':
			case 'C':
				// TODO
				printf("cloning tree of inode1 (size=%ld) ranged from %ld to  %ld, to inode2 from %ld...", inode->size, start, end, loid);

				printf(" size = %ld\n",inode2->size);
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
		printf("%-8ld,%-8ld[%d,%ld,%ld]\n",ext->off_f,prev_end,ext->dobj->addr.host_id,ext->dobj->addr.loid,ext->off_do);
		ext = ext->next;
	}
	printf("----------------------------------------------\n");
}
