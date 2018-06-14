#include"incont.h"
#include<sys/types.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <fcntl.h>
#include"errno.h"

#define INO_MAX	10000

int main(int argc, char **argv) 
{
	int client_len;
	int client_sockfd;
	struct sockaddr_in clientaddr; 
	int i;
	int suc = 0, fail = 0, c = 0;

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	clientaddr.sin_port = htons(7183);

	client_len = sizeof(clientaddr);

	srand(time(NULL));

	unsigned char com;
	ino_t lino, pino;

	for(i = 0; i < INO_MAX; i++) {
		c++;
		client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			fail++;
			close(client_sockfd);
			continue;
		}
		suc++;

		com = 'P';
		lino = i;
		pino = rand() % INO_MAX;
		write(client_sockfd, &com, sizeof(com));
		write(client_sockfd, &lino, sizeof(lino));
		write(client_sockfd, &pino, sizeof(pino));
		close(client_sockfd);
	}

	printf("success = %d/%d, fail = %d/%d\n",suc,c,fail,c);
	
	client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = 'Q';
	write(client_sockfd, &com, sizeof(com));
	close(client_sockfd);

	return 0;
}
