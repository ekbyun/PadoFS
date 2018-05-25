#include"inserver.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NUM_WORKER_THREAD	4

int main(int argc, char **argv) 
{
	init_inode_container(0);

	return 0;
}
