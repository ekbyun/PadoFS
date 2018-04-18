#include"inserver.h"

struct extent *reuse;

int main()
{
	enum {A,B} a;
	a = A;
	create_node_pool(getpagesize());
	printf("sizeof extent = %ld\n", sizeof(struct extent) );
	printf("sizeof object = %ld\n", sizeof(struct object) );
	printf("a = %d ,sizeof A = %ld\n", a, sizeof(a) );
	printf("sizeof int = %ld size_t= %ld long long = %ld \n", sizeof(int) , sizeof(size_t) ,sizeof(long long));
	

	//test 
	init_inode_container(0);

	struct inode *inode = create_inode("/root/test.txt",01755,0,11,1024);
	struct inode *inode2 = create_inode("/root/test2.txt",01755,0,12,512);


	struct inode *getinode = get_inode(inode->ino);

	printf("%s mode=%o size=%d,%d\n", getinode->name, getinode->mode, (int)getinode->size, (int)getinode->flayout->length);
	printf("inode = %ld\n", getinode->ino);

	getinode = get_inode(inode2->ino);

	printf("%s mode=%o size=%d,%d\n", getinode->name, getinode->mode, (int)getinode->size, (int)getinode->flayout->length);
	printf("inode = %ld\n", getinode->ino);


	return 0;
}
