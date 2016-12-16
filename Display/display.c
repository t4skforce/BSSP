/*
 * display.c
 *
 *  Created on: Dec 16, 2016
 *      Author: bssp
 */
#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "share.h"

int main() {
	mqd_t queue = mq_open(QUEUE_NAME, O_RDONLY);
	if (queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}
	char line[MAX_MSG_LEN];
	do {
		unsigned int prio = 0;
		ssize_t len = mq_receive(queue, line, MAX_MSG_LEN, &prio);
		if (len == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			return 1;
		}
		line[MAX_MSG_LEN-1]=0;
		printf("prio: %u, msg:'%s' \n",prio,line);
		fflush(stdout);

	} while (strcmp(line, "quit"));
	mq_close(queue);
	mq_unlink(QUEUE_NAME);
	return 0;
}
