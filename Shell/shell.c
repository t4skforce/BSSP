// shell.c

#define POSIX_C_SOURCE 201112L

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
#define NAME "Name"
#define MATNR "MatNR"

// parsing & helper
char * readline(char *s, size_t max);
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
char *builtin_cmd[] = { "cd", "pwd", "id", "exit", "umask", "printenv", "info",
		"setpath" };
int (*builtin_func[])(char **,
		struct tm) = {&cd, &pwd, &id, &sexit, &sumask, &printenv, &info, &setpath
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

int main(int argc, char **argv) {
	char line[MAXLEN];
	char * cmd[MAXWORDS];
	time_t t = time(NULL);
	struct tm start = *localtime(&t);
	int execstat;
	int bg = 0;
	do {
		prompt(); // display input prompt
		readline(line, MAXLEN); // read input line
		bg = isBG(line); // search &
		splitcommand(line, cmd); // split commaand into parts (tokenize)
		execstat = execcmd(cmd, bg, start); // run internal command or execute file in forked thread
	} while (execstat == 1); // run in loop until execcmd returns != 1
}

/**
 * read input line, dismiss \n at end
 */
char * readline(char *s, size_t max) {
	fgets(s, max - 1, stdin);
	s[strlen(s) - 1] = '\0';
	return s;
}

/**
 * tokenize input into command parts
 */
void splitcommand(char *zeile, char ** vec) {
	int i = 0;
	for (vec[i] = strtok(zeile, " \t\n"); vec[i];
			vec[++i] = strtok(NULL, " \t\n"))
		;
}

/**
 * internal function exit shell
 */
int sexit(char **command, struct tm start) {
	printf("Goodbye!\n");
	return 0;
}

/**
 * internal function change directory
 */
int cd(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(stderr, "expected argument to \"cd\"\n");
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
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("%s\n", cwd);
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

	printf("\n");
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
		printf("%s%c", *env, '\n');
	return 1;
}

/**
 * internal function display shell info
 */
int info(char **command, struct tm start) {
	printf("Shell von: %s %s\n", NAME, MATNR);
	printf("PID: %i\n", getpid());
	printf("Läuft seit: %d.%d.%d %d:%d:%d Uhr\n", start.tm_mday,
			start.tm_mon + 1, start.tm_year + 1900, start.tm_hour, start.tm_min,
			start.tm_sec);
	return 1;
}

/**
 * internal function set path environment variable
 */
int setpath(char **command, struct tm start) {
	if (command[1] == NULL) {
		fprintf(stderr, "expected argument to \"setpath\"\n");
	} else {
		char buff[4096];
		strcat(strcat(buff, "PATH="), command[1]);
		if (putenv(buff) != 0) {
			perror("error setenv:");
			return 0;
		} else {
			printf("new Path: %s\n", buff);
		}
	}
	return 1;
}

/**
 * display current umask
 */
int sumask(char **command, struct tm start) {
	mode_t mask = umask(0);
	umask(mask);
	printf("%04o\n", mask);
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
