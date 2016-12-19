/*
 * compute.c
 *
 *  Created on: Dec 19, 2016
 *
 * Authors:
 * is141315 - Neumair Florian
 * is141305 - Gimpl Thomas
 */
#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "share.h"

int main() {
	int fd;
	printf("open queue\n");
	mqd_t queue = mq_open(QUEUE_NAME, O_RDONLY);
	if (queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}

	printf("make pipe\n");
	unlink(PNAME);
	if (mkfifo(PNAME, 0666) == -1) {
		perror("mkfifo");
		return 1;
	}

	printf("%s open pipe\n",PNAME);
	if ((fd = open(PNAME, O_WRONLY)) == -1) {
		perror("open");
		return 1;
	}
	printf("%s opend pipe\n",PNAME);

	char line[MAX_MSG_LEN];
	do {
		int i = 0;
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

		for (i = 0; i < strlen(line); i++) {
			line[i] = toupper(line[i]);
		}

		printf("OUT > msg:'%s' \n", line);

		if (write(fd, line, strlen(line) + 1) == -1) {
			perror("write");
			break;
		}

	} while (strcmp(line, "QUIT"));
	mq_close(queue);
	mq_unlink(QUEUE_NAME);
	unlink(PNAME);
	return 0;
}
