/*
 * ioctl_clear.c
 *
 *  Created on: Jan 8, 2017
 *      Author: bssp
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "ioctl.h"

int main(int argc, char *argv[]) {
	int fd, retVal;
	if (argc != 2) {
		printf("Syntax: %s <device>\n", argv[0]);
		return 1;
	}
	printf("clear: %s\n", argv[1]);
	fd = open(argv[1], O_RDONLY, 0666);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	retVal = ioctl(fd, IOC_CLEAR);
	if (retVal != 0) {
		perror("ioctl");
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}
