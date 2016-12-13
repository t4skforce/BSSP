/*
 * Program: Shell Server
 *
 * Authors:
 * is141315 - Neumair Florian
 * is141305 - Gimpl Thomas
 *
 */
#define POSIX_C_SOURCE 201112L

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#define MAXLEN 2048
#define MAXWORDS 1024
#define NAME "Florian Neumair"
#define MATNR "is141315"

#define SOCKETQUEUE 5
#define SERVERPORT 5315

// parsing & helper
char * readline(int socket, char *s, size_t max);
void splitcommand(char *zeile, char ** vec);
void prompt();

// internal commands
int cd(char **command, struct tm start);
int pwd(char **command, struct tm start);
int id(char **command, struct tm start);
int sexit(char **command, struct tm start);
int sumask(char **command, struct tm start);
int printenv(char **command, struct tm start);
int info(char **command, struct tm start);
int setpath(char **command, struct tm start);

// internal command mapping
char *builtin_cmd[] = { "cd", "pwd", "id", "exit", "umask", "printenv", "info", "setpath" };
int (*builtin_func[])(char **, struct tm) = {&cd, &pwd, &id, &sexit, &sumask, &printenv, &info, &setpath };
int builtin_cnt() {
	return sizeof(builtin_cmd) / sizeof(char *);
}

// execution
int execfile(char **cmd, int bg);
int execcmd(char **cmd, int bg, struct tm start);

int isBG(char *line) {
	char *fnd = strchr(line, '&');
	if (fnd != NULL) {
		*fnd = 0;
		return 1;
	}
	return 0;
}

void clienthandler(int socket, struct tm start) {
	// redirect io to socket
	char line[MAXLEN];
	char * cmd[MAXWORDS];
	int execstat;
	int bg = 0;
	dup2(socket, STDOUT_FILENO); /* duplicate socket on stdout */
	dup2(socket, STDERR_FILENO); /* duplicate socket on stderr too */
	do {
		prompt(); // display input prompt
		readline(socket, line, MAXLEN); // read input line
		if (line[0] != '\0') {
			bg = isBG(line); // search &
			splitcommand(line, cmd); // split commaand into parts (tokenize)
			execstat = execcmd(cmd, bg, start); // run internal command or execute file in forked thread
			fflush(stdout);
		}
	} while (execstat == 1); // run in loop until execcmd returns != 1
}

int main(int argc, char **argv) {
	time_t t = time(NULL);
	struct tm start = *localtime(&t);

	struct sockaddr_in srvaddr;
	struct sockaddr clientaddr;
	int serverfd, clientfd;
	int clientaddlen = sizeof(srvaddr);

	srvaddr.sin_family = AF_INET;
	srvaddr.sin_addr.s_addr = INADDR_ANY;
	srvaddr.sin_port = htons((ushort) SERVERPORT);

	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket:");
		return 1;
	}

	if (bind(serverfd, (struct sockaddr *) &srvaddr, sizeof(srvaddr)) == -1) {
		perror("bind:");
		return 2;
	}

	if (listen(serverfd, SOCKETQUEUE) == -1) {
		perror("listen:");
		return 3;
	}

	while (1) {
		if ((clientfd = accept(serverfd, (struct sockaddr *) &clientaddr, (socklen_t*) &clientaddlen)) == -1) {
			perror("accept:");
			close(serverfd);
			return 3;
		}
		pid_t pid = fork();
		switch (pid) {
		case 0: // Child
			clienthandler(clientfd, start);
			close(clientfd);
			exit(0);
			break;
		case -1: // Error forking
			perror("forking error");
			break;
		default: // Parent
			continue;
		}
	}
}

/**
 * read input line, dismiss \n at end
 */
char * readline(int socket, char *s, size_t max) {
	ssize_t numRead;
	size_t totRead = 0;
	char ch;
	int i;
	for (i = 0; i < max; i++) {
		numRead = read(socket, &ch, 1);
		if (numRead == -1) { /* Error */
			break;
		} else if (numRead == 0) { /* EOF */
			break;
		} else {
			if (ch == '\n' || ch == '\r') {
				break;
			}
			if (totRead < max - 1) { /* Discard > (n - 1) bytes */
				totRead++;
				*s++ = ch;
			}
		}
	}
	*s = '\0';
	return s;
}

/**
 * tokenize input into command parts
 */
void splitcommand(char *zeile, char ** vec) {
	int i = 0;
	for (vec[i] = strtok(zeile, " \t\r\n"); vec[i]; vec[++i] = strtok(NULL, " \t\r\n"))
		;
}

/**
 * internal function exit shell
 */
int sexit(char **command, struct tm start) {
	printf("Goodbye!\r\n");
	return 0;
}

/**
 * internal function change directory
 */
int cd(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(stderr, "expected argument to \"cd\"\r\n");
	} else {
		if (chdir(command[1]) != 0) {
			perror("error cd:");
			return 0;
		}
	}
	return 1;
}

/**
 * print current working directory
 */
int pwd(char **command, struct tm start) {
	char cwd[1024] = { 0 };
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\r\n", cwd);
	} else {
		perror("getcwd:");
	}
	return 1;
}

/**
 * print uid, euid, gid, egid (Namen, is existant)
 * source: http://git.savannah.gnu.org/gitweb/?p=coreutils.git;a=blob;f=src/id.c;h=05d98a5d08a98aea2c9e804b4a33ac4d1dea153a;hb=HEAD
 */
int id(char **command, struct tm start) {
	struct passwd *pwd = NULL;
	struct group *gr = NULL;

	uid_t uid = getuid();
	printf("uid:%i", uid);
	pwd = getpwuid(uid);
	if (pwd)
		printf("(%s)", pwd->pw_name);

	printf(" ");

	uid_t euid = geteuid();
	printf("euid:%i", euid);
	pwd = getpwuid(euid);
	if (pwd)
		printf("(%s)", pwd->pw_name);

	printf(" ");
	gid_t gid = getgid();
	printf("gid:%i", gid);
	gr = getgrgid(gid);
	if (gr)
		printf("(%s)", gr->gr_name);

	gid_t egid = getegid();
	printf("egid:%i", egid);
	gr = getgrgid(egid);
	if (gr)
		printf("(%s)", gr->gr_name);

	printf("\r\n");
	return 1;
}

/**
 * Print Environment Variables
 * http://git.savannah.gnu.org/gitweb/?p=coreutils.git;a=blob;f=src/printenv.c;h=e351d3a1ad82ceb195926490a333099f13231812;hb=HEAD
 */
extern char **environ;
int printenv(char **command, struct tm start) {
	char **env;
	for (env = environ; *env != NULL; ++env)
		printf("%s\r\n", *env);
	return 1;
}

/**
 * internal function display shell info
 */
int info(char **command, struct tm start) {
	printf("Shell von: %s %s\r\n", NAME, MATNR);
	printf("PID: %i\r\n", getpid());
	printf("Läuft seit: %d.%d.%d %d:%d:%d Uhr\r\n", start.tm_mday, start.tm_mon + 1, start.tm_year + 1900, start.tm_hour, start.tm_min, start.tm_sec);
	return 1;
}

/**
 * internal function set path environment variable
 */
int setpath(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(stderr, "expected argument to \"setpath\"\r\n");
	} else {
		if (setenv("PATH", command[1], 1) != 0) {
			perror("error setenv:");
			return 0;
		} else {
			printf("new Path: %s\r\n", getenv("PATH"));
		}
	}
	return 1;
}

/**
 * display current umask
 */
int sumask(char **command, struct tm start) {
	if (command[1] == NULL) {
		mode_t mask = umask(0);
		umask(mask);
		printf("%04o\n", mask);
	} else {
		char buf[4] = { 0 };
		strncat(buf, command[1], 3);
		long int nmask = strtol(buf, NULL, 8);
		umask(nmask);
	}
	return 1;
}

/**
 * display formatted input prompt
 */
void prompt() {
	struct utsname s_uname;
	if (uname(&s_uname) == 0) {
		char cwd[1024];
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			printf("%s %s $ ", s_uname.nodename, cwd);
			fflush(stdout);
		}
	}
}

void parent_trap(int sig) {
	fprintf(stderr, "\nCaught signal(%i).\n", sig);
	signal(sig, &parent_trap);
}

/**
 * executre file in forked child thread
 */
int execfile(char **cmd, int bg) {
	pid_t pid;
	int status;
	pid = fork();
	switch (pid) {
	case 0: // Child
		// if we run in background we change the process group so signals are ignored
		if (bg == 1 && setpgid(0, 0) == -1) {
			perror("setpgid");
		}
		if (execvp(cmd[0], cmd) == -1) {
			perror("execvp:");
		}
		exit(1);
		break;
	case -1: // Error forking
		perror("forking error");
		break;
	default: // Parent
		// parent process
		// parent gets signal handler to ignore int and quit and stay running in foreground
		signal(SIGINT, &parent_trap);
		signal(SIGQUIT, &parent_trap);
		if (bg != 1) {
			// parent wait for exit process
			waitpid(pid, &status, 0);
		}
	}
	return 1;
}

/**
 * executes either internal command or external command
 */
int execcmd(char **cmd, int bg, struct tm start) {
	if (cmd[0] == NULL) {
		// An empty command was entered.
		return 1;
	}
	int i;
	for (i = 0; i < builtin_cnt(); i++) {
		if (strcmp(cmd[0], builtin_cmd[i]) == 0) {
			return (*builtin_func[i])(cmd, start);
		}
	}
	return execfile(cmd, bg);
}
