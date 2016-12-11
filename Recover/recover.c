// recover.c
// syntax: ./recover backup.bin

#define POSIX_C_SOURCE 201112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#define MAXPATHLEN 4096
#define BUFSIZE 2048
#define DEBUG 1

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

void RecoverBackup(int bfd);
int ReadName(int bfd, char *name);
int ReadStat(int bfd, struct stat *inode);
void RecoverFile(int bfd, const char *fname, struct stat *inode);
void RecoverDir(const char *name, struct stat *inode);

// helper
void pathFromFile(const char *fname, char *path);
void ensurePath(char * path);

int main(int argc, char **argv) {

	// parameter check
	if (argc != 2) {
		fprintf(stderr, "please use: recover backup.bin\n");
		return 1;
	}

	// check backup file
	int backupfd;
	if ((backupfd = open(argv[1], O_RDONLY)) == -1) {
		perror("backup opening error: ");
		return 2;
	}

	struct stat backupinode;
	if (fstat(backupfd, &backupinode) == -1) {
		perror("backup stat error: ");
		return 3;
	}

	// check ob wirklich backup datei
	if (S_ISREG(backupinode.st_mode)) {
		// run recovery
		RecoverBackup(backupfd);
		close(backupfd);
	} else {
		fprintf(stderr, "we require a file to recover\n");
		return 4;
	}
}

void RecoverBackup(int bfd) {
	char name[BUFSIZE];
	// search backup for names
	while (ReadName(bfd, name) > 0) {
		// search backup for inodes
		struct stat backupnode;
		if (ReadStat(bfd, &backupnode) == 0) {
			fprintf(stderr, "error reading backup stat\n");
			exit(1);
		}

		if (S_ISREG(backupnode.st_mode)) {
			// if file recover contents
			RecoverFile(bfd, name, &backupnode);
		} else if (S_ISDIR(backupnode.st_mode)) {
			// recover dir if dir
			RecoverDir(name, &backupnode);
		}
	}
}

// loop file until we find \0 for string end
int ReadName(int bfd, char *name) {
	name[0] = '\0';
	char c[1];
	int cnt;
	for (cnt = 0; read(bfd, c, 1) > 0; cnt++) {
		if (cnt == BUFSIZE) {
			fprintf(stderr, "error finding name %s\n", name);
			exit(1);
		}
		name[cnt] = c[0];
		if (c[0] == '\0') {
			return strlen(name);
		}
	}
	return 0;
}

// read stat struct back from binary file
int ReadStat(int bfd, struct stat *inode) {
	int rbytes = 0;
	if ((rbytes = read(bfd, inode, sizeof(struct stat)))
			!= sizeof(struct stat)) {
#ifdef DEBUG
		fprintf(stderr, "%i vs. %l\n", rbytes, sizeof(struct stat));
#endif
		return 0;
	}
	return 1;
}

// recover file contents
void RecoverFile(int bfd, const char *fname, struct stat *inode) {
	char path[BUFSIZE];
	pathFromFile(fname, &path);
	ensurePath(path);
	int cnt;
	char buf[BUFSIZE];
	int fileEnd = inode->st_size;
	int targetfd;
	if ((targetfd = creat(fname, inode->st_mode)) == -1) {
		perror("Backuptarget: ");
		exit(2);
	}

	while (fileEnd > 0 && (cnt = read(bfd, &buf, min(BUFSIZE, fileEnd))) > 0) {
#ifdef DEBUG
		printf("%i - %i\n", fileEnd, cnt);
#endif
		write(targetfd, buf, cnt);
		fileEnd -= BUFSIZE;
	}
	close(targetfd);
	printf("recovered file %s\n", fname);
}

// recover dir
void RecoverDir(const char *name, struct stat *inode) {
	struct stat st = { 0 };
	if (stat(name, &st) == -1) {
		mkdir(name, inode->st_mode);
		printf("recovered folder %s\n", name);
	}
}

// split filename to retreave path
void pathFromFile(const char *fname, char *path) {
	int i;
	int fnd = 0;
	for (i = strlen(fname); i >= 0; i--) {
		if (fname[i] == '/' || fnd == 1) {
			if (fnd == 0) {
				fnd = 1;
				path[i] = '\0';
			} else {
				path[i] = fname[i];
			}
		}
	}
}

// ensure given path exists
void ensurePath(char * path) {
	struct stat st = { 0 };
	if (stat(path, &st) == -1) {
		mkdir(path, 0777);
	}
}
