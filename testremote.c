#include"incont.h"
#include"inserver.h"
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
	int suc = 0, fail = 0, fail2 = 0, c = 0, busy = 0;

	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr("10.0.0.101");
	clientaddr.sin_port = htons(3495);

	client_len = sizeof(clientaddr);

	srand(time(NULL));

	unsigned char com,ret;
	ino_t lino;
	ino_t pino;

	for(i = 0; i < INO_MAX; i++) {
		c++;
		client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if( connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len) < 0 ) {
			fail++;
			close(client_sockfd);
			continue;
		}
		suc++;

	//	com = 'P';
		com = (i < INO_MAX-1) ? i % 9 : 9;
	//	com = i % 9;
		lino = i + 100;
	//	pino = rand() % INO_MAX;
		write(client_sockfd, &com, sizeof(com));
		write(client_sockfd, &lino, sizeof(lino));
		read(client_sockfd, &ret, sizeof(ret));
		dp("command %s %ld ===> returns %s\n",comstr[com],lino,retstr[ret]);
		if( ret == SERVER_BUSY ) {
			busy++;
		} else if( ret == QUEUED ) {
			read(client_sockfd, &pino, sizeof(pino));
			read(client_sockfd, &ret, sizeof(ret));
			if( pino - lino != 1 ) { 
				fail2++;
			}
		}
		close(client_sockfd);
	}

	printf("success = %d/%d, fail = %d/%d echo fail = %d busy = %d\n",suc,c,fail,c, fail2, busy);
	
/*
	client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connect(client_sockfd, (struct sockaddr *)&clientaddr, client_len);
	com = 'Q';
	write(client_sockfd, &com, sizeof(com));
*/
	close(client_sockfd);

	return 0;
}
