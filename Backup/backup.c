// backup.c
// syntax: ./backup node1 node2 ...
// Backup Device steht in ENV: BACKUPDEVICE

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
#define MAGICBYTES "Name515635200"

void BackupNode(const char *nodename, const struct stat *tinode, int tfd);
void WriteMagicBytes(int tfd);
void HeaderWrite(const char *nodename, const struct stat *srcinode,
		int tfd);
void WriteContent(const char *nodename, int tfd);
void BackupDirContent(const char *nodename, const struct stat *tinode, int tfd);

int main(int argc, char **argv) {
	char *targetname = getenv("BACKUPDEVICE");
	int targetfd;
	struct stat targetinode;

	// check env var
	if (!targetname) {
		fprintf(stderr, "BACKUPDEVICE environment variable not set\n");
		return 1;
	}

	// try open backup file
	if ((targetfd = creat(targetname, 0660)) == -1) {
		perror("Backuptarget: ");
		return 2;
	}

	// read stat from file
	if (fstat(targetfd, &targetinode) == -1) {
		perror("Backuptarget: ");
		return 3;
	}

	WriteMagicBytes(targetfd);
	if (argc == 1) {
		BackupNode(".", &targetinode, targetfd);
	} else {
		while (--argc) {
			BackupNode(*++argv, &targetinode, targetfd);
		}
	}
	close(targetfd);
	return 0;
}

void WriteMagicBytes(int tfd) {
	write(tfd, MAGICBYTES, strlen(MAGICBYTES));
}

// backup layout
// file:    [Name][Inode][Content]
// folder:  [Name][Inode]
void BackupNode(const char *nodename, const struct stat *tinode, int tfd) {
	struct stat srcinode;

	// error stating src
	if (lstat(nodename, &srcinode) == -1) {
		perror("srcinode error:  ");
#ifdef DEBUG
		char cwd[1024];
		if (getcwd(cwd, sizeof(cwd)) != NULL) fprintf(stdout, "Current working dir: %s\n", cwd);
#endif
		return;
	}

	// check if backup (inodenum and dev equal)
	if (srcinode.st_ino == tinode->st_ino
			&& srcinode.st_dev == tinode->st_dev) {
		fprintf(stderr, "source equals target\n");
		return;
	}

	// is file
	if (S_ISREG(srcinode.st_mode)) {
		HeaderWrite(nodename, &srcinode, tfd);
		WriteContent(nodename, tfd);
	} else if (S_ISDIR(srcinode.st_mode)) {
		// is dir
		// strcmp == 0 is both equal => !.
		// ignore dir if "." | ".."
		if (!strcmp(nodename, ".") || !strcmp(nodename, "..")) {
			printf("Ignoring special Folder %s\n", nodename);
			return;
		}
		HeaderWrite(nodename, &srcinode, tfd);
		BackupDirContent(nodename, tinode, tfd);
	} else {
		printf("ignoring special File %s\n", nodename);
	}
}

// write header
void HeaderWrite(const char *nodename, const struct stat *srcinode,
		int tfd) {
	write(tfd, nodename, strlen(nodename) + 1);
	write(tfd, srcinode, sizeof(struct stat));
}

// write file content
void WriteContent(const char *nodename, int tfd) {
	int rfd;
	int cnt;
	char buf[BUFSIZE];

	if ((rfd = open(nodename, O_RDONLY)) == -1) {
		perror("error opening backup file:  ");
		return;
	}

	printf("backup %s\n", nodename);
	while ((cnt = read(rfd, buf, BUFSIZE)) > 0) {
		write(tfd, buf, cnt);
	}

	close(rfd);
}

// backup dir
void BackupDirContent(const char *nodename, const struct stat *tinode, int tfd) {
	DIR *dir;
	struct dirent *dentry;
	char name[MAXPATHLEN];

	dir = opendir(nodename);
	if (!dir) {
		perror("folder open error:  ");
		return;
	}
	while ((dentry = readdir(dir)) != NULL) {
		// strcmp == 0 wenn beide gleich desshalb !. Wenn Verzeichnis "." oder ".." ignorieren.
		if (!strcmp(dentry->d_name, ".") || !strcmp(dentry->d_name, "..")) {
			continue;
		}
		strcat(strcat(strcpy(name, nodename), "/"), dentry->d_name);
		BackupNode(name, tinode, tfd);
	}
}
