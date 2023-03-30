/* popen/pclose:
 *
 * simple MS-DOS piping scheme to imitate UNIX pipes
 */

#include "common.h"

#include "util2.h"
#include "util3.h"

#include <io.h>
#include <process.h>
#include <setjmp.h>

#ifndef	_NFILE
# define _NFILE 5			/* Number of open files */
#endif	_NFILE

#define READIT	1			/* Read pipe */
#define WRITEIT	2			/* Write pipe */

static char *s_prgname[_NFILE];  /* program name if write pipe */
static int s_pipetype[_NFILE];   /* 1=read 2=write */
static char *s_pipename[_NFILE]; /* pipe file name */

/*
 *------------------------------------------------------------------------
 * run: Execute command via SHELL or COMSPEC
 *------------------------------------------------------------------------
 */

static int run(char* command)
{
    jmp_buf     panic;             /* How to recover from errors */
    char*       shell;             /* Command processor */
    int         s_is_malloced = 0; /* True if need to free 's' */
    static char*command_com = "COMMAND.COM";
    static char dash_c[] = "/c";

    char *s = savestr(command);
    /* Determine the command processor */
    if ((shell = getenv("SHELL")) == nullptr
     && (shell = getenv("COMSPEC")) == nullptr)
	shell = command_com;
    strupr(shell);
    char *shellpath = shell;
    /* Strip off any leading backslash directories */
    shell = strrchr(shellpath, '\\');
    if (shell != nullptr)
	shell++;
    else
	shell = shellpath;
    /* Strip off any leading slash directories */
    char *bp = strrchr(shell, '/');
    if (bp != nullptr)
	shell = ++bp;
    if (strcmp(shell, command_com) != 0) {
	s = safemalloc(strlen(command) + 3);
        char *bp = s;
	*bp++ = '\'';
	while ((*bp++ = *command++) != '\0') ;
	*(bp - 1) = '\'';
	*bp = '\0';
	s_is_malloced = 1;
    } else
	s = command;
    /* Run the program */
    int status = spawnl(P_WAIT, shellpath, shell, dash_c, s, nullptr);
    if (s_is_malloced)
	free(s);
    return status;
}

/* 
 *------------------------------------------------------------------------
 * uniquepipe: returns a unique file name 
 *------------------------------------------------------------------------
 */

static char *uniquepipe()
{ 
    static char name[14];
    static short int num = 0;
    (void) sprintf(name, "pipe%04d.tmp", num++);
    return name;
}

/*
 *------------------------------------------------------------------------
 * resetpipe: Private routine to cancel a pipe
 *------------------------------------------------------------------------
 */
static void resetpipe(int fd)
{
    if (fd >= 0 && fd < _NFILE) {
	s_pipetype[fd] = 0;
        char *bp = s_pipename[fd];
        if (bp != nullptr)
        {
	    (void) unlink(bp);
	    free(bp);
	    s_pipename[fd] = nullptr;
	}
        bp = s_prgname[fd];
        if (bp != nullptr)
        {
	    free(bp);
	    s_prgname[fd] = nullptr;
	}
    }
}

/* 
 *------------------------------------------------------------------------
 * popen: open a pipe 
 *------------------------------------------------------------------------
 */
//char* prg;			/* The command to be run */
//char* type;			/* "w" or "r" */
FILE *popen(char *prg, char *type)
{ 
    FILE*   p = nullptr; /* Where we open the pipe */
    int     ostdin;      /* Where our stdin is now */
    int     pipefd = -1; /* fileno(p) -- for convenience */
    jmp_buf panic;       /* Where to go if there's an error */

    /* Get a unique pipe file name */
    char *tmpfile = filexp("%Y/");
    strcat(tmpfile, uniquepipe());
    int lineno = setjmp(panic);
    if (lineno != 0)
    {
	/* An error has occurred, so clean up */
	int E = errno;
	if (p != nullptr)
	    (void) fclose(p);
	resetpipe(pipefd);
	errno = E;
	lineno = lineno;
	return nullptr;
    }
    if (strcmp(type, "w") == 0) {
	/* for write style pipe, pclose handles program execution */
        p = fopen(tmpfile, "w");
        if (p != nullptr)
        {
	    pipefd = fileno(p);
	    s_pipetype[pipefd] = WRITEIT;
	    s_pipename[pipefd] = savestr(tmpfile);
	    s_prgname[pipefd]  = savestr(prg);
	}
    } else if (strcmp(type, "r") == 0) {
	/* read pipe must create tmp file, set up stdout to point to the temp
	 * file, and run the program.  note that if the pipe file cannot be
	 * opened, it'll return a condition indicating pipe failure, which is
	 * fine. */
        p = fopen(tmpfile, "w");
        if (p != nullptr)
        {
            pipefd = fileno(p);
	    s_pipetype[pipefd]= READIT;
	    s_pipename[pipefd] = savestr(tmpfile);
	    /* Redirect stdin for the new command */
	    int ostdout = dup(fileno(stdout));
	    if (dup2(fileno(stdout), pipefd) < 0) {
		int E = errno;
		(void) dup2(fileno(stdout), ostdout);
		errno = E;
		longjmp(panic, __LINE__);
	    }
	    if (run(prg) != 0)
		longjmp(panic, __LINE__);
	    if (dup2(fileno(stdout), ostdout) < 0)
		longjmp(panic, __LINE__);
	    if (fclose(p) < 0)
		longjmp(panic, __LINE__);
            p = fopen(tmpfile, "r");
            if (p == nullptr)
		longjmp(panic, __LINE__);
	}
    } else {
	/* screwy call or unsupported type */
	errno = EINVAL;
	longjmp(panic, __LINE__);
    }
    return p;
}

/* close a pipe */
int pclose(FILE *p)
{
    int     pipefd = -1; /* Fildes where pipe is opened */
    int     ostdout;     /* Where our stdout points now */
    int     ostdin;      /* Where our stdin points now */
    jmp_buf panic;       /* Context to return to if error */
    int     lineno = setjmp(panic);
    if (lineno != 0)
    {
	/* An error has occurred, so clean up and return */
	int E = errno;
	if (p != nullptr)
	    (void) fclose(p);
	resetpipe(pipefd);
	errno = E;
	lineno = lineno;
	return -1;
    }
    pipefd = fileno(p);
    if (fclose(p) < 0)
	longjmp(panic, __LINE__);
    switch (s_pipetype[pipefd]) {
    case WRITEIT:
	/* open the temp file again as read, redirect stdin from that
	 * file, run the program, then clean up. */
        p = fopen(s_pipename[pipefd], "r");
        if (p == nullptr) 
	    longjmp(panic, __LINE__);
	ostdin = dup(fileno(stdin));
	if (dup2(fileno(stdin), fileno(p)) < 0)
	    longjmp(panic, __LINE__);
	if (run(s_prgname[pipefd]) != 0)
	    longjmp(panic, __LINE__);
	if (dup2(fileno(stdin), ostdin) < 0)
	    longjmp(panic, __LINE__);
	if (fclose(p) < 0)
	    longjmp(panic, __LINE__);
	resetpipe(pipefd);
	break;
    case READIT:
	/* close the temp file and remove it */
	resetpipe(pipefd);
	break;
    default:
	errno = EINVAL;
	longjmp(panic, __LINE__);
	/*NOTREACHED*/
    }
    return 0;
}
