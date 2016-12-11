// socket client
#define POSIX_C_SOURCE 201112L

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
// sudo apt-get install libncurses5-dev
#include <rawio.h>

#define SERVERNAME "127.0.0.1"
#define SERVERPORT 4315
#define MAXCLIENTBUFF 2050
#define MSGBUFF 1024

typedef struct {
  int sock;
  int lines;
  char name[MSGBUFF];
} socket_t;

void* readhandler(void *arg) {
	socket_t sock = *(socket_t *)arg;
	char buf[MAXCLIENTBUFF];
	memset(buf, 0, MAXCLIENTBUFF);
	int anz;
	clearscr();
	while ((anz = read(sock.sock, buf, MAXCLIENTBUFF - 1)) > -1) {
		scroll_up(1,sock.lines-3);
		writestr_raw(buf, 0, sock.lines-3);
		memset(buf, 0, MAXCLIENTBUFF);
	}
}

void* writehandler(void *arg) {
	socket_t sock = *(socket_t *)arg;
	char buf[MSGBUFF];
	while(1) {
		memset(buf, 0, MAXCLIENTBUFF);
		gets_raw(&buf, MSGBUFF, 0, sock.lines-1);
		write(sock.sock, buf,strlen(buf));
	}
}

int main() {
	int sock;
	struct sockaddr_in serversock;

	//Create socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket:");
		return 1;
	}
	serversock.sin_addr.s_addr = inet_addr(SERVERNAME);
	serversock.sin_family = AF_INET;
	serversock.sin_port = htons((ushort) SERVERPORT);

	//Connect to remote server
	if (connect(sock, (struct sockaddr *) &serversock, sizeof(serversock)) < 0) {
		perror("connect failed. Error");
		return 2;
	}

	socket_t *server = malloc(sizeof(socket_t));
	server->lines=get_lines();
	server->sock = sock;

	char buf[MAXCLIENTBUFF];
	buf[MAXCLIENTBUFF]=0;
	do {
		read(server->sock, buf, MAXCLIENTBUFF - 1);
		//printf("%s",buf);
		writestr_raw(buf, 0, server->lines-1);
		gets_raw(&server->name, MSGBUFF, strlen(buf), server->lines-1);
	} while(strlen(server->name)==0);
	clearscr();

	pthread_t thrid1;
	pthread_t thrid2;
	int err;
	if ((err = pthread_create(&thrid1, NULL, readhandler, (void *) server))
			!= 0) {
		fprintf(stderr, "pthread_create: %s", strerror(err));
		return 4;
	}
	if ((err = pthread_create(&thrid2, NULL, writehandler, (void *) server))
			!= 0) {
		fprintf(stderr, "pthread_create: %s", strerror(err));
		return 4;
	}
	write(server->sock, server->name,strlen(server->name));
	int ret;
	pthread_join(thrid1,&ret);
	pthread_join(thrid2,&ret);
	return 0;
}
