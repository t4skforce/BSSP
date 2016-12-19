/*
 * menu.c
 *
 *  Created on: Dec 18, 2016
 *      Author: bssp
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "share.h"

int main(int argc, char const *argv[]) {
	int fd;
	char line[MAX_MSG_LEN];
	shm_for_msg_t *shm;
	sem_t *semwrite; // display -> menu: write is allowed
	sem_t *semread; // menu -> display: memory is written

	sem_unlink(SNAMEWRITE);
	if ((semwrite = sem_open(SNAMEWRITE, O_CREAT, 0644, 0)) == SEM_FAILED) {
		perror("sem_open");
		return 1;
	}

	sem_unlink(SNAMEREAD);
	if ((semread = sem_open(SNAMEREAD, O_CREAT, 0644, 1)) == SEM_FAILED) {
		perror("sem_open");
		sem_unlink(SNAMEWRITE);
		return 1;
	}

	shm_unlink(SHM_NAME);
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
		int len;
		printf(" > ");
		fgets(line, MAX_MSG_LEN, stdin);
		len = strlen(line);

		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
			len--;
		}

		if (!strcmp(line, "sleep")) {
			sleep(20);
		}

		sem_wait(semwrite);
		memcpy(shm->msg, line, MAX_MSG_LEN);
		sem_post(semread);
	} while (strcmp(line, "quit"));

	munmap(shm, sizeof(shm_for_msg_t));
	close(fd);
	sem_close(semread);
	sem_close(semwrite);
	return 0;
}
