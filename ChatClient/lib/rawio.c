// rawio.h
// simple routines for terminal rawio
//******************************** needs on linux ghc-terminfo-delel packages!!!!
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

//#define DEBUG 1

static char terminf_entry[1024];
static char platz_fuer_termstrings[1024];
static char *p=platz_fuer_termstrings;
static char *Cl, *Cpos, *Scroll, *SetScrollRegion; 
static int nrlines;

static int isinit;
static struct termios told;

static pthread_mutex_t screenmutex = PTHREAD_MUTEX_INITIALIZER;

void clearscr();

static int writebyte(int val)
{
#ifdef DEBUG
#else
	char byte;
	byte=(char)val;
	write(0, &byte,1);
#endif
	return 0;
}

void exithandler()
{
#ifdef DEBUG
#else
	clearscr();
	tcsetattr(0, TCSAFLUSH, &told);
#endif
}

static void init()
{
#ifdef DEBUG
#else
	struct termios t;
	if (isinit)
		return;
	if ( tcgetattr (0,&told) == -1 )
		fprintf(stderr, "failed to init terminal\n"), exit(1);
	t=told;
		
	if (   tgetent(terminf_entry, getenv("TERM"))==-1 )
		fprintf(stderr, "failed to init terminaltype %s\n", getenv("TERM")), exit(1);
	if (( Cl=tgetstr("cl",&p)) == NULL )
		fprintf(stderr, "failed to get clear seq for terminaltype %s\n", getenv("TERM")), exit(1);
	if (( Cpos=tgetstr("cm", &p)) == NULL )
		fprintf(stderr, "failed to get cup seq for terminaltype %s\n", getenv("TERM")), exit(1);
	if (( SetScrollRegion=tgetstr("cs", &p)) == NULL )
		fprintf(stderr, "failed to get scrollreeg_seq for terminaltype %s\n", getenv("TERM")), exit(1);
	if (( Scroll=tgetstr("SF", &p)) == NULL )
		fprintf(stderr, "failed to get scroll seq for terminaltype %s\n", getenv("TERM")), exit(1);
	if (( nrlines = tgetnum("li")) == -1)
		fprintf(stderr, "failed to get line-size for terminaltype %s\n", getenv("TERM")), exit(1);

	atexit(exithandler);

	t.c_lflag &= ~ECHO;
	t.c_lflag &= ~ICANON;
	t.c_cc[VMIN] = 0;
	t.c_cc[VTIME] = 1;
	if(tcsetattr(0, TCSAFLUSH, &t) < 0)
		fprintf(stderr, "failed to change Terminal Interface to RAW\n"),  exit(1);
	++isinit;
#endif
}

char *gets_raw(char *s, int maxlen, int col, int row)
{
	char c;
	int i=0, rc;
	rc=read(0,&c,1);
	while ( i<maxlen-2 &&  c!=10 && rc!=13 && rc!='\n' && rc!='\r' )
	{
		if (rc>0)
		{
#ifdef DEBUG
#else
			pthread_mutex_lock(&screenmutex);
			tputs( tgoto( Cpos, col+i, row ), 1, writebyte);
			write(1,&c,1); // echo
			pthread_mutex_unlock(&screenmutex);
#endif
			s[i++]=c;
		}
		rc=read(0,&c,1);
	}
	s[i]=0;
	return s;
}


void writestr_raw(char *s, int col, int row)
{
#ifdef DEBUG
	pthread_mutex_lock(&screenmutex);
	write(1,s,strlen(s));
	write(1,'\n',1);
	pthread_mutex_unlock(&screenmutex);
#else
	pthread_mutex_lock(&screenmutex);
	tputs( tgoto( Cpos, col, row ), 1, writebyte);
	write( 0, s, strlen(s) );
	pthread_mutex_unlock(&screenmutex);
#endif
}


void scroll_up(int from_line, int to_line)
{
#ifdef DEBUG
	pthread_mutex_lock(&screenmutex);
	printf("scroll_up: from_line=%d, to_line=%d",from_line,to_line);
	pthread_mutex_unlock(&screenmutex);
#else
	pthread_mutex_lock(&screenmutex);
	tputs( tparm(SetScrollRegion,from_line,to_line), nrlines, writebyte);
	tputs( tparm(Scroll,1), nrlines, writebyte);
	tputs( tparm(SetScrollRegion,0,nrlines), nrlines, writebyte);
	pthread_mutex_unlock(&screenmutex);
#endif
}


int get_lines()
{
#ifdef DEBUG
	return 99;
#else
	if (!isinit)
		init();
	return nrlines;
#endif
}


void clearscr()
{
#ifdef DEBUG
#else
	if (!isinit)
		init();
	tputs(Cl,0,writebyte);
#endif
}


