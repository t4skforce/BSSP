/*
 * compute.c
 *
 *  Created on: Dec 19, 2016
 *
 * Authors:
 * is141315 - Neumair Florian
 * is141305 - Gimpl Thomas
 */
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "share.h"

void ucase(char *line, int len) {
	int i = 0;
	for (i = 0; i < len; i++) {
		line[i] = toupper(line[i]);
	}
}

void lcase(char *line, int len) {
	int i = 0;
	for (i = 0; i < len; i++) {
		line[i] = tolower(line[i]);
	}
}

int cpipe(char *name) {
	int fd = -1;
	printf("%s make pipe\n", name);
	unlink(name);
	if (mkfifo(name, 0666) == -1) {
		perror("mkfifo");
	}

	printf("%s open pipe\n", name);
	if ((fd = open(name, O_WRONLY)) == -1) {
		perror("open");
	}
	printf("%s opend pipe\n", name);
	return fd;
}

int main() {
	int fdlc, fduc;
	printf("open queue\n");
	mqd_t queue = mq_open(QUEUE_NAME, O_RDONLY);
	if (queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}

	if ((fdlc = cpipe(PNAME1)) == -1) {
		return 1;
	}
	if ((fduc = cpipe(PNAME2)) == -1) {
		return 1;
	}

	char line[MAX_MSG_LEN];
	do {
		unsigned int prio = 0;
		printf("read queue\n");
		ssize_t len = mq_receive(queue, line, MAX_MSG_LEN, &prio);
		if (len == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			return 1;
		}
		line[MAX_MSG_LEN - 1] = 0;
		printf("IN > prio: %u, msg:'%s' \n", prio, line);
		fflush(stdout);

		lcase(line, strlen(line));
		printf("OUT(%s) > msg:'%s' \n", PNAME1, line);
		if (write(fdlc, line, strlen(line) + 1) == -1) {
			perror("write");
			break;
		}

		ucase(line, strlen(line));
		printf("OUT(%s) > msg:'%s' \n", PNAME2, line);
		if (write(fduc, line, strlen(line) + 1) == -1) {
			perror("write");
			break;
		}

	} while (strcmp(line, "QUIT"));
	mq_close(queue);
	mq_unlink(QUEUE_NAME);
	unlink(PNAME1);
	unlink(PNAME2);
	return 0;
}
