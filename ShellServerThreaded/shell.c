// shell.c

#define POSIX_C_SOURCE 201112L

#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>
#include <errno.h>

#define MAXLEN 2048
#define MAXWORDS 1024
#define NAME "Name"
#define MATNR "MatNR"

#define LOGPATH "/var/log/"
#define LOGEXT ".log"
#define LOGFILE LOGPATH MATNR LOGEXT

#define SOCKETQUEUE 5
#define SERVERPORT 6315

#define DEBUG 1

typedef struct {
	int sock;
	FILE *fp;
	char ip[INET_ADDRSTRLEN];
	char cwd[MAXLEN];
	char path[MAXLEN];
	mode_t mask;
	struct tm start;
} client_t;

// thread local variable of client status
// https://en.wikipedia.org/wiki/Thread-local_storage
static __thread client_t client;
// perror can alos be used in thread with errno being thread-local
// https://linux.die.net/man/3/errno =>
// "errno is thread-local; setting it in one thread does not affect its value in any other thread."

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
int getlog(char **command, struct tm start);
int clientinfo(char **command, struct tm start);

// internal command mapping
char *builtin_cmd[] = { "cd", "pwd", "id", "exit", "umask", "printenv", "info", "setpath", "getprot", "getlog", "clientinfo" };
int (*builtin_func[])(char **, struct tm) = {&cd, &pwd, &id, &sexit, &sumask, &printenv, &info, &setpath, &getlog, &getlog, &clientinfo
};
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

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *LOGFD;

// working directory mutex
pthread_mutex_t cwd_mutex = PTHREAD_MUTEX_INITIALIZER;

// path mutex
pthread_mutex_t path_mutex = PTHREAD_MUTEX_INITIALIZER;

void logcommand(char *ipaddr, char *cmd) {
	pthread_mutex_lock(&log_mutex);
	fprintf(LOGFD, "%s: %s\n", ipaddr, cmd);
	fflush(LOGFD);
	pthread_mutex_unlock(&log_mutex);
}

void clienthandler(void * arg) {
	memcpy(&client, &(*(client_t *) arg), sizeof(client_t)); // set thread local values
	free(arg); // free maloc value
	// redirect io to socket
	char line[MAXLEN];
	char * cmd[MAXWORDS];
	int execstat;
	int bg = 0;
	do {
		prompt(); // display input prompt
		readline(client.sock, line, MAXLEN); // read input line
		if (line[0] != '\0') {
			char logline[MAXLEN] = { 0 };
			strncpy(logline, line, strlen(line));
			bg = isBG(line); // search &
			splitcommand(line, cmd); // split command into parts (tokenize)
			execstat = execcmd(cmd, bg, client.start); // run internal command or execute file in forked thread
			if (execstat == 0) {
				logcommand(client.ip, logline);
			}
			fflush(client.fp);
		}
	} while (execstat != -1); // run in loop until execcmd returns != 1
	fprintf(client.fp, "disconnect.\n");
	close(client.sock);
}

int main(int argc, char **argv) {
	time_t t = time(NULL);
	struct tm start = *localtime(&t);
	LOGFD = fopen(LOGFILE, "a");

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

	// get startup env variables
	if (getcwd(client.cwd, sizeof(client.cwd)) == NULL) {
		perror("getcwd:");
		return 4;
	}
	strcpy(client.path, getenv("PATH"));
	client.mask = umask(0);
	umask(client.mask);

	while (1) {
		if ((clientfd = accept(serverfd, (struct sockaddr *) &clientaddr, (socklen_t*) &clientaddlen)) == -1) {
			perror("accept:");
			close(serverfd);
			return 3;
		}
		// init thread local struct client_t client
		client_t *arg = malloc(sizeof(client_t));
		arg->start = start;
		inet_ntop(clientaddr.sa_family, &(((struct sockaddr_in *) &clientaddr)->sin_addr), arg->ip, clientaddlen);
		arg->sock = clientfd;
		arg->fp = fdopen(clientfd, "a");
		strcpy(arg->cwd, client.cwd);
		strcpy(arg->path, client.path);
		arg->mask = client.mask;
		// launch thread
		pthread_t thrid;
		int err;
		if ((err = pthread_create(&thrid, NULL, clienthandler, (void *) arg)) != 0) {
			fprintf(client.fp, "pthread_create: %s", strerror(err));
			close(serverfd);
			return 4;
		}
	}
	fclose(LOGFD);
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
	fprintf(client.fp, "Goodbye!\r\n");
	return -1;
}

/**
 * Change into thread-local working directory
 */
void cwd_fake() {
	if (chdir(client.cwd) != 0) {
		fprintf(client.fp, "cwd_fake: %s\r\n", strerror(errno));
	}
}

/**
 * internal function change directory
 */
int cd(char **command, struct tm start) {
	int retVal = 0;
	if (command[1] == NULL) {
		fprintf(client.fp, "expected argument to \"cd\"\r\n");
	} else {
		pthread_mutex_lock(&cwd_mutex);
		cwd_fake();
		if (chdir(command[1]) != 0) {
			fprintf(client.fp, "cd chdir: %s\r\n", strerror(errno));
			retVal = 1;
		}
		// update to new cwd
		if (getcwd(client.cwd, sizeof(client.cwd)) == NULL) {
			fprintf(client.fp, "getcwd: %s\r\n", strerror(errno));
		}
		pthread_mutex_unlock(&cwd_mutex);
	}
	return retVal;
}

/**
 * print current working directory
 */
int pwd(char **command, struct tm start) {
	fprintf(client.fp, "%s\r\n", client.cwd);
	return 0;
}

/**
 * print uid, euid, gid, egid (Namen, is existant)
 * source: http://git.savannah.gnu.org/gitweb/?p=coreutils.git;a=blob;f=src/id.c;h=05d98a5d08a98aea2c9e804b4a33ac4d1dea153a;hb=HEAD
 */
int id(char **command, struct tm start) {
	struct passwd *pwd = NULL;
	struct group *gr = NULL;

	uid_t uid = getuid();
	fprintf(client.fp, "uid:%i", uid);
	pwd = getpwuid(uid);
	if (pwd)
		fprintf(client.fp, "(%s)", pwd->pw_name);

	fprintf(client.fp, " ");

	uid_t euid = geteuid();
	fprintf(client.fp, "euid:%i", euid);
	pwd = getpwuid(euid);
	if (pwd)
		fprintf(client.fp, "(%s)", pwd->pw_name);

	fprintf(client.fp, " ");
	gid_t gid = getgid();
	fprintf(client.fp, "gid:%i", gid);
	gr = getgrgid(gid);
	if (gr)
		fprintf(client.fp, "(%s)", gr->gr_name);

	gid_t egid = getegid();
	fprintf(client.fp, "egid:%i", egid);
	gr = getgrgid(egid);
	if (gr)
		fprintf(client.fp, "(%s)", gr->gr_name);

	fprintf(client.fp, "\r\n");
	return 0;
}

/**
 * internal function display shell info
 */
int info(char **command, struct tm start) {
	fprintf(client.fp, "Shell von: %s %s\r\n", NAME, MATNR);
	fprintf(client.fp, "PID: %i\r\n", getpid());
	fprintf(client.fp, "Läuft seit: %d.%d.%d %d:%d:%d Uhr\r\n", start.tm_mday, start.tm_mon + 1, start.tm_year + 1900, start.tm_hour, start.tm_min, start.tm_sec);
	return 0;
}

int clientinfo(char **command, struct tm start) {
	fprintf(client.fp, "IP: %s \r\n", client.ip);
	fprintf(client.fp, "FP: %d \r\n", client.sock);
	fprintf(client.fp, "PATH: %s \r\n", client.path);
	fprintf(client.fp, "CWD: %s \r\n", client.cwd);
	return 0;
}

/**
 * Change into thread-local working directory
 */
void path_fake() {
	if (setenv("PATH", client.path, 1) != 0) {
		fprintf(client.fp, "path_fake putenv: %s\r\n", strerror(errno));
	}
}

/**
 * Print Environment Variables
 * http://git.savannah.gnu.org/gitweb/?p=coreutils.git;a=blob;f=src/printenv.c;h=e351d3a1ad82ceb195926490a333099f13231812;hb=HEAD
 */
extern char **environ;
int printenv(char **command, struct tm start) {
	char **env;
	pthread_mutex_lock(&path_mutex);
	path_fake();
	for (env = environ; *env != NULL; ++env)
		fprintf(client.fp, "%s\r\n", *env);
	pthread_mutex_unlock(&path_mutex);
	return 0;
}

/**
 * internal function set path environment variable
 */
int setpath(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(client.fp, "expected argument to \"setpath\"\r\n");
	} else {
		snprintf(client.path, MAXLEN, "%s", command[1]);
		fprintf(client.fp, "new Path: %s\r\n", client.path);
	}
	return 0;
}

void umask_fake() {
	umask(client.mask);
}

/**
 * display current umask
 */
int sumask(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(client.fp, "%04o\n", client.mask);
	} else {
		char buf[4] = { 0 };
		strncat(buf, command[1], 3);
		client.mask = strtol(buf, NULL, 8);
	}
	return 0;
}

/**
 *
 */
int getlog(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(client.fp, "expected argument to \"%s\"\r\n", command[0]);
	} else {
		int retVal;
		char * cmd[] = { "tail", "-n", command[1], LOGFILE, NULL };
		pthread_mutex_lock(&log_mutex);
		retVal = execfile(cmd, 0);
		pthread_mutex_unlock(&log_mutex);
		return retVal;
	}
	return 0;
}

/**
 * display formatted input prompt
 */
void prompt() {
	struct utsname s_uname;
	if (uname(&s_uname) == 0) {
		fprintf(client.fp, "%s %s $ ", s_uname.nodename, client.cwd);
		fflush(client.fp);
	}
}

void parent_trap(int sig) {
	fprintf(client.fp, "\nCaught signal(%i).\n", sig);
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
			fprintf(client.fp, "execfile setpgid: %s\r\n", strerror(errno));
		}
		umask_fake();
		path_fake();
		cwd_fake();
		dup2(client.sock, STDOUT_FILENO); /* duplicate socket on stdout */
		dup2(client.sock, STDERR_FILENO); /* duplicate socket on stderr too */
		if (execvp(cmd[0], cmd) == -1) {
			fprintf(client.fp, "execfile execvp: %s\r\n", strerror(errno));
		}
		exit(1);
		break;
	case -1: // Error forking
		fprintf(client.fp, "forking error: %s\r\n", strerror(errno));
		break;
	default: // Parent
		// parent process
		// parent gets signal handler to ignore int and quit and stay running in foreground
		signal(SIGINT, &parent_trap);
		signal(SIGQUIT, &parent_trap);
		if (bg != 1) {
			// parent wait for exit process
			waitpid(pid, &status, 0);
			return status;
		}
		return 1;
	}
	return 0;
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
