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

	struct inode *inode = create_inode();
	sprintf(inode->baseobj.addr.posix_path, "/root/test.txt");

	struct inode *inode2 = create_inode();
	sprintf(inode2->baseobj.addr.posix_path, "/root/test2.txt");

	struct inode *getinode = get_inode(0);

	printf("%s\n", getinode->baseobj.addr.posix_path);
	printf("inode = %ld\n", getinode->ino);

	getinode = get_inode(1);

	printf("%s\n", getinode->baseobj.addr.posix_path);
	printf("inode = %ld\n", getinode->ino);


	return 0;
}
