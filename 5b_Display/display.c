/*
 * display.c
 *
 *  Created on: Dec 18, 2016
 *
 * Authors:
 * is141315 - Neumair Florian
 * is141305 - Gimpl Thomas
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "share.h"

int main() {
	int fd;
	char line[MAX_MSG_LEN];
	shm_for_msg_t *shm;
	sem_t *semwrite; // display -> menu: write is allowed
	sem_t *semread; // menu -> display: memory is written

	if ((semwrite = sem_open(SNAMEWRITE, O_RDWR)) == SEM_FAILED) {
		perror("sem_open");
		return 1;
	}

	if ((semread = sem_open(SNAMEREAD, O_RDWR)) == SEM_FAILED) {
		perror("sem_open");
		return 1;
	}

	fd = shm_open(SHM_NAME, O_RDWR | O_CREAT, 0600);
	if (fd == -1) {
		perror(SHM_NAME);
		return 1;
	}

	if (ftruncate(fd, sizeof(shm_for_msg_t)) == -1) {
		perror(SHM_NAME);
		close(fd);
		shm_unlink(SHM_NAME);
		return 1;
	}

	shm = (shm_for_msg_t*) mmap(NULL, sizeof(shm_for_msg_t), PROT_WRITE,
	MAP_SHARED, fd, 0);

	if (shm == MAP_FAILED) {
		perror(SHM_NAME);
		close(fd);
		shm_unlink(SHM_NAME);
		return 1;
	}

	do {
		sem_wait(semread);
		memcpy(line, shm->msg, MAX_MSG_LEN);
		sem_post(semwrite);
		if (shm->msg[0] != NULL) {
			printf("msg:'%s' \n", line);
			fflush(stdout);
		}
	} while (strcmp(line, "quit"));
	return 0;
}
