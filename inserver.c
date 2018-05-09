#include"inserver.h"

void print_extent(struct extent *);

int main() 
{
	printf("sizeof inode = %ld\n", sizeof(struct inode) );
	printf("sizeof extent = %ld\n", sizeof(struct extent) );
	printf("sizeof object = %ld\n", sizeof(struct dobject) );

	//test 
	init_inode_container(0);

	struct inode *inode = create_inode("test.txt",01755,0,11,20,21,1024);
	struct inode *inode2 = create_inode("test2.txt",01755,0,12,20,21,512);


	struct inode *getinode = get_inode(inode->ino);

	printf("%s mode=%o size=%d,%d\n", getinode->name, getinode->mode, (int)getinode->size, (int)getinode->uid);
	printf("inode = %ld\n", getinode->ino);

	getinode = get_inode(inode2->ino);

	printf("%s mode=%o size=%d,%d\n", getinode->name, getinode->mode, (int)getinode->size, (int)getinode->gid);
	printf("inode = %ld\n", getinode->ino);


	struct dobject *dobj;
	struct extent *ext;

	dobj= get_dobject(5,6);
	pado_write(inode, dobj, 10000, 10000, 10000);

	ext = find_start_extent(inode, 0);
	print_extent(ext);

	return 0;
}

void print_extent(struct extent *ext) {
	int i = 0;
	while( ext != NULL ) {
		for(i=0;i<ext->depth-1;i++) {
			printf("                 ");
		}
		printf("%-8ld,%-8ld[%d,%ld,%ld]\n",ext->start,ext->start+ext->length,ext->dobj->addr.host_id,ext->dobj->addr.loid,ext->offset);
		ext = ext->next;
	}
}
