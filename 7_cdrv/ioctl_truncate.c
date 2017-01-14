/*
 * ioctl_resize.c
 *
 *  Created on: Jan 8, 2017
 *      Author: bssp
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "ioctl.h"

int main(int argc, char *argv[]) {
	int fd, retVal;
	long size;
	if (argc != 3) {
		printf("Syntax: %s <device> <size>\n\n", argv[0]);
		return 1;
	}
	size = strtol(argv[2], NULL, 10);
	printf("%s: size truncated: %ld bytes\n", argv[1], size);
	fd = open(argv[1], O_WRONLY, 0666);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	retVal = ioctl(fd, IOC_TRUNCATE, size);
	if (retVal != 0) {
		perror("ioctl");
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}
