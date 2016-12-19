/*
 * compute.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "share.h"

int main(int argc, char *argv[]) {
	char line[MAX_MSG_LEN];
	ssize_t len;
	mqd_t queue;
	unsigned int priority;
	int fd;
	int i;

	if ((queue = mq_open(QUEUE_NAME, O_RDONLY)) == -1) {
		perror("mq_open");
		return 1;
	}

	unlink(PNAME);
	if (mkfifo(PNAME, 0600) == -1) {
		perror("mkfifo");
		return 1;
	}

	if ((fd = open(PNAME, O_WRONLY)) == -1) {
		perror("open");
		return 1;
	}

	do {
		unsigned int prio = 0;
		ssize_t len = mq_receive(queue, line, MAX_MSG_LEN, &prio);
		if (len == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			return 1;
		}
		line[MAX_MSG_LEN - 1] = 0;
		printf("prio: %u, msg:'%s' \n", prio, line);

		for (i = 0; i < strlen(line); i++) {
			line[i] = toupper(line[i]);
		}

		if (write(fd, line, strlen(line) + 1) == -1) {
			perror("write");
			break;
		}
	} while (strcmp("QUIT", line));
	mq_close(line);
	mq_unlink(QUEUE_NAME);
	return 0;
}
