#include"inserver.h"
#include"incont.h"

char *dcomst[] = {
    "CREATE_BASEFILE",
	"READ_DATA",
	"DRAIN_DO",
	"DRAIN_LSHARED",
	"ADD_LINK",
	"REMOVE_LINK",
	"MOVE_SHARED_BASE",
};

char *retst[] = {
	"s0",
	"s1",
	"SUCCESS",
	"FAILED"
};

int main(int argc, char **argv) {
	unsigned short port = DOS_ETH_PORT;
	int ret = SUCCESS;

	if( argc > 2) {
		port = atoi(argv[2]);
	}

	if( argc > 1 && (argv[1][0] == 'F') ) {
		ret = FAILED;
	}

	int sockfd, fd, clen;
	
//	uint32_t lhid;
//	lhid = ntohl(inet_addr("127.0.0.1"));
//	printf("lhid = %d\n",lhid);

	struct sockaddr_in sa,ca;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&sa, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);

	if( bind( sockfd, (struct sockaddr *)&sa, sizeof(sa) ) == -1 ) {
		perror("bind error:");
		exit(0);
	}

	if( listen(sockfd, 32) == -1 ) {
		perror("listen error:");
		exit(0);
	}

	clen = sizeof(ca);

	printf("start listening at %d, returns %s\n", port,retst[ret]);
	
	unsigned char com;
	ino_t loid;
	ino_t pino,bino;

	while(1) {
		fd = accept(sockfd, (struct sockaddr *)&ca, (socklen_t *)&clen);

		read(fd, &com, sizeof(com));
		read(fd, &loid, sizeof(loid));
		read(fd, &pino, sizeof(loid));
#ifdef WITH_MAPSERVER
		read(fd, &bino, sizeof(loid));
#endif
		write(fd, &ret, sizeof(ret));

		printf("com=%s ,loid=%lu ,pino=%lu --> %s(%d)\n", dcomst[com],loid,pino,retst[ret],ret);

		close(fd);
	} 

	return 0;
}

