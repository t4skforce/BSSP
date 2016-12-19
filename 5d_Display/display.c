/*
 * display.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "share.h"

int main(int argc, char *argv[]) {
	char buff[MSG_LEN];
	ssize_t len;
	int fd;

	if ((fd = open(PNAME, O_RDONLY)) == -1) {
		perror("open");
		return 1;
	}

	do {
		if ((len = read(fd, buff, MSG_LEN)) == -1) {
			perror("read");
			break;
		}

		buff[MSG_LEN - 1] = '\0';
		printf("%s\n", buff);
	} while (strcmp("QUIT", buff));

	close(fd);
	unlink(PNAME);
	return 0;
}