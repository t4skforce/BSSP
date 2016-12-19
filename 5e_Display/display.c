/*
 * display.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "share.h"

int usage(char *n) {
	printf("%s [1,2]\n", n);
	return 1;
}

void lcase(char *line, int len) {
	int i = 0;
	for (i = 0; i < len; i++) {
		line[i] = tolower(line[i]);
	}
}

int main(int argc, char *argv[]) {
	char buff[MAX_MSG_LEN];
	ssize_t len;
	int fd;

	if (argc != 2) {
		return usage(argv[0]);
	}
	int nr = atoi(argv[1]);
	if (nr <= 0 || nr >= 3) {
		return usage(argv[0]);
	}

	char *pipename;
	if (nr == 1) {
		pipename = PNAME1;
	} else {
		pipename = PNAME2;
	}

	printf("%s open pipe\n", pipename);
	if ((fd = open(pipename, O_RDONLY)) == -1) {
		perror("open");
		return 1;
	}
	printf("%s  opend pipe\n", pipename);

	do {
		if ((len = read(fd, buff, MAX_MSG_LEN)) == -1) {
			perror("read");
			break;
		}

		buff[MAX_MSG_LEN - 1] = '\0';
		printf("%s\n", buff);

		lcase(buff,strlen(buff));
	} while (strcmp("quit", buff));

	close(fd);
	unlink(pipename);
	return 0;
}
