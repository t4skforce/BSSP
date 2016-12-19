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
	char buffer[MSG_LEN];
	ssize_t len;
	mqd_t mqueue;
	unsigned int priority;
	int fd;
	int i;

	if ((mqueue = mq_open(MQNAME, O_RDONLY)) == -1) {
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
		if ((len = mq_receive(mqueue, buffer, MSG_LEN, &priority)) == -1) {
			perror("mq_receive");
			break;
		}

		buffer[MSG_LEN - 1] = '\0';

		printf("prio: %d, msg: '%s'\n", priority, buffer);

		for (i = 0; i < strlen(buffer); i++) {
			buffer[i] = toupper(buffer[i]);
		}

		if (write(fd, buffer, strlen(buffer) + 1) == -1) {
			perror("write");
			break;
		}
	} while (strcmp("QUIT", buffer));
	mq_close(mqueue);
	mq_unlink(MQNAME);
	return 0;
}
