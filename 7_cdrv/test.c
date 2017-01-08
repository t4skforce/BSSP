/*
 * test.c
 *
 *  Created on: Dec 19, 2016
 *      Author: bssp
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "ioctl.h"

static char *devices[] = { "/dev/mydev0", "/dev/mydev1", "/dev/mydev2",
		"/dev/mydev3", "/dev/mydev4" };

int testRW() {
	int fd;

	char writebuff[1024];
	char readbuff[4096];
	int count, size;
	int i, j;
	char data[] = "0123456789";

	for (i = 0; i < 5; i++) {
		printf("open %s\n", devices[i]);
		fd = open(devices[i], O_WRONLY, 0666);
		if (fd == -1) {
			perror("open");
			return 1;
		}

		for (j = 0; j < 3; j++) {
			strcpy(writebuff, data);
			printf("writing '%s' to %s \n", writebuff, devices[i]);
			count = write(fd, writebuff, strlen(data));
			printf("written %d of %d bytes\n", count, strlen(data));
		}
		close(fd);

		printf("reading device %s\n", devices[i]);
		fd = open(devices[i], O_RDONLY, 0666);
		if (fd == -1) {
			perror("open");
			return 1;
		}
		size = strlen(data) * j;
		printf("reading %d from %s ...\n", size, devices[i]);
		count = read(fd, readbuff, size);
		printf("read %d of %d from %s\n", count, size, devices[i]);
		readbuff[count] = '\0';
		printf("buffer2: %s\n", readbuff);
	}
	return 0;
}

int testMultiRW() {
	int fd1, fd2;
	int count, size;
	int i, j;
	char data[] = "0123456789";

	for (i = 0; i < 5; i++) {
		fd1 = open(devices[i], O_WRONLY, 0666);
		if (fd1 == -1) {
			perror("open");
		} else {
			printf("%s opened.\n", devices[i]);
		}

		fd2 = open(devices[i], O_WRONLY, 0666);
		if (fd2 == -1) {
			perror("open");
			close(fd1);
		}
	}
	return 0;
}

int testIIOCtl() {
	int fd, fdr;
	int retVal;
	int i;
	char data[] = "0123456789";
	char readbuff[4096];

	for (i = 0; i < 5; i++) {
		printf("open %s\n", devices[i]);
		fd = open(devices[i], O_WRONLY, 0666);
		if (fd == -1) {
			perror("open");
			return 1;
		}
		retVal = ioctl(fd, IOC_READCNT);
		if (retVal == -1) {
			perror("ioctl");
			close(fd);
			return 1;
		}
		printf("dev->reading=%d\n", retVal);
		retVal = ioctl(fd, IOC_WRITECNT);
		if (retVal == -1) {
			perror("ioctl");
			close(fd);
			return 1;
		}
		printf("dev->writing=%d\n", retVal);

		fdr = open(devices[i], O_RDONLY, 0666);
		if (fdr == -1) {
			perror("open");
			return 1;
		}

		retVal = ioctl(fd, IOC_READCNT);
		if (retVal == -1) {
			perror("ioctl");
			close(fd);
			return 1;
		}
		printf("dev->reading=%d\n", retVal);

		write(fd, data, strlen(data));
		read(fdr, readbuff, strlen(data));

		printf("written: %s , read: %s\n",data,readbuff);

		retVal = ioctl(fd, IOC_CLEAR);
		if (retVal == -1) {
			perror("ioctl");
			close(fd);
			return 1;
		}
		printf("clear()=%d\n", retVal);

		memset(readbuff,0,4096);
		read(fdr, readbuff, strlen(data));
		printf("read: %s\n",data,readbuff);

		close(fd);
	}
	return 0;
}

int main(int argc, char *argv[]) {
	testRW();
	testMultiRW();
	testIIOCtl();
	return 0;
}
