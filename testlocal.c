#include"incont.h"
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>

#define	FIFOFILE	"/tmp/pado_test_fifo"

void print_extent(struct extent *, int, uint32_t);
void check_bst(struct extent *, size_t *, size_t *);

int main(int argc, char **argv) 
{
	dp("sizeof inode = %ld\n", sizeof(struct inode) );
	dp("sizeof extent = %ld\n", sizeof(struct extent) );
	dp("sizeof object = %ld\n", sizeof(struct dobject) );

	init_inode_container(0,0);

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
/*	
	while(1) {
		dp("Insert command:");
		scanf("%s %d %ld %ld %ld",com, &hid, &loid, &start, &end);
*/
//		/*
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
//	*/
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
	return 0;
}
