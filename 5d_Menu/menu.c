/*
 * menu.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "share.h"

int main() {
	struct mq_attr attr = { 0, MAX_MSG_COUNT, MAX_MSG_LEN, 0 };
	mqd_t queue = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0644, &attr);
	if (queue == -1) {
		perror(QUEUE_NAME);
		return 1;
	}
	char line[MAX_MSG_LEN];
	do {
		int len;
		printf("> ");
		fgets(line, MAX_MSG_LEN, stdin);
		len = strlen(line);

		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}

		if (!strcmp(line, "sleep")) {
			sleep(20);
		}

		if (mq_send(queue, line, len + 1, 7) == -1) {
			perror(QUEUE_NAME);
			mq_close(queue);
			return 1;
		}

	} while (strcmp(line, "quit"));
	mq_close(queue);
	return 0;
}
