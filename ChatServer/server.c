// socketserver
#define POSIX_C_SOURCE 201112L

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define SOCKETQUEUE 5
#define MAXCLIENTS 20
#define MAXCLIENTBUFF 1024
#define SENDBUFF (MAXCLIENTBUFF * 2) + 2
#define SERVERPORT 4315

typedef struct {
	int sock;
	FILE *fd;
	char id[MAXCLIENTBUFF];
} client_t;

client_t* clientfds[MAXCLIENTS];
int aktclients = 0;

pthread_mutex_t clientfd_mutex = PTHREAD_MUTEX_INITIALIZER;

void clean_str(char *src,int len) {
	int i;
	src[len]=0;
	int l = strlen(src)-1;
	for (i = 0; i < l; i++) {
		if (src[i] == '\r' || src[i] == '\n') {
			src[i]=0;
		}
	}
}

int insert_client(client_t *client) {
	pthread_mutex_lock(&clientfd_mutex);
	if (aktclients >= MAXCLIENTS) {
		pthread_mutex_unlock(&clientfd_mutex);
		return 0;
	}
	clientfds[aktclients] = client;
	++aktclients;
	pthread_mutex_unlock(&clientfd_mutex);
	return 1;
}

void remove_client(client_t *client) {
	int i;
	pthread_mutex_lock(&clientfd_mutex);
	for (i = 0; i < aktclients; i++) {
		if (clientfds[i] == client) {
			clientfds[i] = clientfds[aktclients - 1];
			close(client->sock);
			break;
		}
	}
	clientfds[aktclients - 1] = NULL;
	--aktclients;
	pthread_mutex_unlock(&clientfd_mutex);
}

void tell_all_clients(char *line) {
	if (line[0] != 0) {
		pthread_mutex_lock(&clientfd_mutex);
		int i;
		for (i = 0; i < aktclients; i++) {
			printf("send %d: %s\n", clientfds[i]->sock, line);
			fprintf(clientfds[i]->fd,"%s\n",line);
			fflush(clientfds[i]->fd);
		}
		pthread_mutex_unlock(&clientfd_mutex);
	}
}

void* clienthandler(void *arg) {
	client_t client = *(client_t *) arg;
	char buf[MAXCLIENTBUFF];
	char send_buf[SENDBUFF] = { 0 };
	int anz;

	do {
		fprintf(client.fd,"Client-ID: ");
		fflush(client.fd);

	} while ((anz = read(client.sock, buf, MAXCLIENTBUFF - 1)) <= 0);
	clean_str(buf,anz);
	strncpy(client.id, buf, strlen(buf));
	insert_client(&client);

	printf("%s joind the Server\n", client.id);

	snprintf(send_buf,SENDBUFF,"%s: joind the Server\n",client.id);
	tell_all_clients(send_buf);
	while ((anz = read(client.sock, buf, MAXCLIENTBUFF - 1)) > 0) {
		clean_str(buf,anz);
		memset(send_buf, 0, SENDBUFF);
		snprintf(send_buf,SENDBUFF,"%s: %s\n",client.id,buf);
		tell_all_clients(send_buf);
	}
	printf("%s left the Server\n", client.id);
	remove_client(&client);
	close(client.sock);
	//free(arg);
}

int running = 1;
int endhandler() {
	running = 0;
	return 0;
}

int main() {
	struct sockaddr_in srvaddr;
	struct sockaddr clientaddr;
	int serverfd, clientfd;
	int clientaddlen = sizeof(srvaddr);
	pthread_t thrid;
	int err;

	srvaddr.sin_family = AF_INET;
	srvaddr.sin_addr.s_addr = INADDR_ANY;
	srvaddr.sin_port = htons((ushort) SERVERPORT);

	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket:");
		return 1;
	}

	if (bind(serverfd, (struct sockaddr *) &srvaddr, sizeof(srvaddr)) == -1) {
		perror("bind:");
		return 2;
	}

	if (listen(serverfd, SOCKETQUEUE) == -1) {
		perror("listen:");
		return 3;
	}

	while (running) {
		if ((clientfd = accept(serverfd, (struct sockaddr *) &clientaddr, (socklen_t*) &clientaddlen)) == -1) {
			perror("accept:");
			close(serverfd);
			return 3;
		}

		client_t *clnt = malloc(sizeof(client_t));
		clnt->sock = clientfd;
		clnt->fd = fdopen(clientfd, "a");
		// threadaddr belegen und übergeben
		if ((err = pthread_create(&thrid, NULL, clienthandler, (void *) clnt)) != 0) {
			fprintf(stderr, "pthread_create: %s", strerror(err));
			close(serverfd);
			free(clnt);
			return 4;
		}
	}

	// Melden an alle Clients
	// ev. pthread_join
	printf("goodby");
	close(serverfd);
}
