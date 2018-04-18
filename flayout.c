#include"inserver.h"

int create_node_pool(size_t pagesize)
{
	printf("create_node_pool!!. pagesize = %ld\n", pagesize);
	return 0;
}

struct extent *get_extent() {
	struct extent *newone;
	newone = calloc(sizeof(struct extent),1);
	perror("calloc");
	return newone;
}

void release_extent(struct extent *target) {
	free(target);
}
