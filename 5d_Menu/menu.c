/*
 * menu.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#include <stdio.h>
#include <string.h>
#include <mqueue.h>
#include <unistd.h>

#include "share.h"

int main(int argc, char *argv[]) {
	char buff[MSG_LEN];
	int len;
	struct mq_attr attr = { 0, MQCNT, MSG_LEN, 0 };
	mqd_t mqueue;

	if ((mqueue = mq_open(MQNAME, O_WRONLY | O_CREAT, 0644, &attr)) == -1) {
		perror("mq_open");
		return 1;
	}
	do {
		printf("> ");
		fgets(buff, MSG_LEN, stdin);
		len = strlen(buff);

		if (len > 0 && buff[len - 1] == '\n') {
			buff[len - 1] = '\0';
			len--;
		}

		if (!strcmp("sleep", buff)) {
			sleep(20);
		}

		if (mq_send(mqueue, buff, strlen(buff) + 1, 0) == -1) {
			perror("mq_send");
			break;
		}
	} while (strcmp("quit", buff));
	mq_close(mqueue);
	return 0;
}
