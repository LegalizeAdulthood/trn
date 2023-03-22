/* util.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "final.h"
#include "term.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "intrp.h"
#include "env.h"
#include "util2.h"
#include "search.h"
#include "smisc.h"	/* g_s_default_cmd */
#include "univ.h"
#include "util.h"

#ifdef I_SYS_TIMEB
#include <sys/timeb.h>
#endif
#ifdef I_SYS_WAIT
#include <sys/wait.h>
#endif
#ifdef MSDOS
#include <process.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <sys/types.h>
#endif

#ifndef HAS_GETCWD
static char *trn_getcwd(char *buf, int len);
#endif

#ifdef UNION_WAIT
using WAIT_STATUS = union wait;
#else
using WAIT_STATUS = int;
#endif

bool g_waiting{}; /* waiting for subprocess (in doshell)? */
bool g_nowait_fork{};
bool export_nntp_fds{};
/* the strlen and the buffer length of "some_buf" after a call to:
 *     some_buf = get_a_line(bufptr,bufsize,realloc,fp); */
int g_len_last_line_got{};
MEM_SIZE g_buflen_last_line_got{};

#ifndef USE_DEBUGGING_MALLOC
static char s_nomem[] = "trn: out of memory!\n";
#endif

static char s_null_export[] = "_=X";/* Just in case doshell precedes util_init */
static char *s_newsactive_export = s_null_export + 2;
static char *s_grpdesc_export = s_null_export + 2;
static char *s_quotechars_export = s_null_export + 2;
static char *s_nntpserver_export = s_null_export + 2;
static char *s_nntpfds_export = s_null_export + 2;
static char *s_nntpforce_export = s_null_export + 2;

void util_init()
{
    extern char g_patchlevel[];
    char* cp;
    int i;
    for (i = 0, cp = g_buf; i < 512; i++)
	*cp++ = 'X';
    *cp = '\0';
    s_newsactive_export = export_var("NEWSACTIVE", g_buf);
    s_grpdesc_export = export_var("NEWSDESCRIPTIONS", g_buf);
    s_nntpserver_export = export_var("NNTPSERVER", g_buf);
    g_buf[64] = '\0';
    s_quotechars_export = export_var("QUOTECHARS",g_buf);
    s_nntpfds_export = export_var("NNTPFDS", g_buf);
    g_buf[3] = '\0';
    s_nntpforce_export = export_var("NNTP_FORCE_AUTH", g_buf);

    for (cp = g_patchlevel; isspace(*cp); cp++) ;
    export_var("TRN_VERSION", cp);
}
    
/* fork and exec a shell command */

int doshell(const char *shell, const char *s)
{
#ifndef MSDOS
    WAIT_STATUS status;
    pid_t pid, w;
#endif
    int ret = 0;

    xmouse_off();

#ifdef SIGTSTP
    sigset(SIGTSTP,SIG_DFL);
    sigset(SIGTTOU,SIG_DFL);
    sigset(SIGTTIN,SIG_DFL);
#endif
    if (g_datasrc && (g_datasrc->flags & DF_REMOTE)) {
	if (!export_nntp_fds || !g_nntplink.rd_fp)
	    un_export(s_nntpfds_export);
	else {
	    sprintf(g_buf,"%d.%d",(int)fileno(g_nntplink.rd_fp),
				(int)fileno(g_nntplink.wr_fp));
	    re_export(s_nntpfds_export, g_buf, 64);
	}
	re_export(s_nntpserver_export,g_datasrc->newsid,512);
	if (g_datasrc->nntplink.flags & NNTP_FORCE_AUTH_NEEDED)
	    re_export(s_nntpforce_export,"yes",3);
	else
	    un_export(s_nntpforce_export);
	if (g_datasrc->auth_user) {
	    int fd;
	    if ((fd = open(g_nntp_auth_file, O_WRONLY|O_CREAT, 0600)) >= 0) {
		write(fd, g_datasrc->auth_user, strlen(g_datasrc->auth_user));
		write(fd, "\n", 1);
		if (g_datasrc->auth_pass) {
		    write(fd, g_datasrc->auth_pass, strlen(g_datasrc->auth_pass));
		    write(fd, "\n", 1);
		}
		close(fd);
	    }
	}
	if (g_nntplink.port_number) {
	    int len = strlen(s_nntpserver_export);
	    sprintf(g_buf,";%d",g_nntplink.port_number);
	    if (len + (int)strlen(g_buf) < 511)
		strcpy(s_nntpserver_export+len, g_buf);
	}
	if (g_datasrc->act_sf.fp)
	    re_export(s_newsactive_export, g_datasrc->extra_name, 512);
	else
	    re_export(s_newsactive_export, "none", 512);
    } else {
	un_export(s_nntpfds_export);
	un_export(s_nntpserver_export);
	un_export(s_nntpforce_export);
	if (g_datasrc)
	    re_export(s_newsactive_export, g_datasrc->newsid, 512);
	else
	    un_export(s_newsactive_export);
    }
    if (g_datasrc)
	re_export(s_grpdesc_export, g_datasrc->grpdesc, 512);
    else
	un_export(s_grpdesc_export);
    interp(g_buf,64-1+2,"%I");
    g_buf[strlen(g_buf)-1] = '\0';
    re_export(s_quotechars_export, g_buf+1, 64);
    if (shell == nullptr && (shell = get_val("SHELL",nullptr)) == nullptr)
	shell = PREFSHELL;
    termlib_reset();
#ifdef MSDOS
    intptr_t status = spawnl(P_WAIT, shell, shell, "/c", s, nullptr);
#else
    if ((pid = vfork()) == 0) {
	if (g_datasrc && (g_datasrc->flags & DF_REMOTE)) {
	    int i;
	    /* This is necessary to keep the bourne shell from puking */
	    for (i = 3; i < 10; ++i) {
		if (g_nntplink.rd_fp
		 && (i == fileno(g_nntplink.rd_fp)
		  || i == fileno(g_nntplink.wr_fp)))
		    continue;
		close(i);
	    }
	}
	if (g_nowait_fork) {
	    close(1);
	    close(2);
	    dup(open("/dev/null",1));
	}

	if (*s)
	    execl(shell, shell, "-c", s, nullptr);
	else
	    execl(shell, shell, nullptr, nullptr, nullptr);
	_exit(127);
    }
    sigignore(SIGINT);
#ifdef SIGQUIT
    sigignore(SIGQUIT);
#endif 
    g_waiting = true;
    while ((w = wait(&status)) != pid)
	if (w == -1 && errno != EINTR)
	    break;
    if (w == -1)
	ret = -1;
    else
#ifdef USE_WIFSTAT
	ret = WEXITSTATUS(status);
#else
#ifdef UNION_WAIT
	ret = status.w_status >> 8;
#else
	ret = status;
#endif /* UNION_WAIT */
#endif /* USE_WIFSTAT */
#endif /* !MSDOS */
    termlib_init();
    xmouse_check();
    g_waiting = false;
    sigset(SIGINT,int_catcher);
#ifdef SIGQUIT
    sigset(SIGQUIT,SIG_DFL);
#endif 
#ifdef SIGTSTP
    sigset(SIGTSTP,stop_catcher);
    sigset(SIGTTOU,stop_catcher);
    sigset(SIGTTIN,stop_catcher);
#endif
    if (g_datasrc && g_datasrc->auth_user)
	remove(g_nntp_auth_file);
    return ret;
}

/* paranoid version of malloc */

#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE size)
{
    char* ptr;

    ptr = (char*) malloc(size ? size : (MEM_SIZE)1);
    if (!ptr) {
	fputs(s_nomem,stdout) FLUSH;
	sig_catcher(0);
    }
    return ptr;
}
#endif

/* paranoid version of realloc.  If where is nullptr, call malloc */

#ifndef USE_DEBUGGING_MALLOC
char *saferealloc(char *where, MEM_SIZE size)
{
    char* ptr;

    if (!where)
	ptr = (char*) malloc(size ? size : (MEM_SIZE)1);
    else
	ptr = (char*) realloc(where, size ? size : (MEM_SIZE)1);
    if (!ptr) {
	fputs(s_nomem,stdout) FLUSH;
	sig_catcher(0);
    }
    return ptr;
}
#endif /* !USE_DEBUGGING_MALLOC */

/* safe version of string concatenate, with \n deletion and space padding */

char *safecat(char *to, const char *from, int len)
{
    char* dest = to;

    len--;				/* leave room for null */
    if (*dest) {
	while (len && *dest++) len--;
	if (len) {
	    len--;
	    *(dest-1) = ' ';
	}
    }
    if (from)
	while (len && (*dest++ = *from++)) len--;
    if (len)
	dest--;
    if (*(dest-1) == '\n')
	dest--;
    *dest = '\0';
    return to;
}

/* effective access */

#ifdef SETUIDGID
int
eaccess(filename, mod)
char* filename;
int mod;
{
    int protection, euid;
    
    mod &= 7;				/* remove extraneous garbage */
    if (stat(filename, &g_filestat) < 0)
	return -1;
    euid = geteuid();
    if (euid == ROOTID)
	return 0;
    protection = 7 & ( g_filestat.st_mode >> (g_filestat.st_uid == euid ?
			6 : (g_filestat.st_gid == getegid() ? 3 : 0)) );
    if ((mod & protection) == mod)
	return 0;
    errno = EACCES;
    return -1;
}
#endif

/*
 * Get working directory
 */
char *trn_getwd(char *buf, int buflen)
{
    char* ret;

#ifdef HAS_GETCWD
    ret = getcwd(buf, buflen);
#else
    ret = trn_getcwd(buf, buflen);
#endif
    if (!ret) {
	printf("Cannot determine current working directory!\n") FLUSH;
	finalize(1);
    }
#ifdef MSDOS
    strlwr(buf);
    while ((buf = strchr(buf,'\\')) != nullptr)
	*buf++ = '/';
#endif
    return ret;
}

#ifndef HAS_GETCWD
static char *trn_getcwd(char *buf, int len)
{
    char* ret;
#ifdef HAS_GETWD
    buf[len-1] = 0;
    ret = getwd(buf);
    if (buf[len-1]) {
	/* getwd() overwrote the end of the buffer */
	printf("getwd() buffer overrun!\n") FLUSH;
	finalize(1);
    }
#else
    FILE* popen();
    FILE* pipefp;
    char* nl;

    if ((pipefp = popen("/bin/pwd","r")) == nullptr) {
	printf("Can't popen /bin/pwd\n") FLUSH;
	return nullptr;
    }
    buf[0] = 0;
    fgets(ret = buf, len, pipefp);
    if (pclose(pipefp) == EOF) {
	printf("Failed to run /bin/pwd\n") FLUSH;
	return nullptr;
    }
    if (!buf[0]) {
	printf("/bin/pwd didn't output anything\n") FLUSH;
    	return nullptr;
    }
    if ((nl = strchr(buf, '\n')) != nullptr)
	*nl = '\0';
#endif
    return ret;
}
#endif

/* just like fgets but will make bigger buffer as necessary */

char *get_a_line(char *buffer, int buffer_length, bool realloc_ok, FILE *fp)
{
    int bufix = 0;
    int nextch;

    do {
	if (bufix >= buffer_length) {
	    buffer_length *= 2;
	    if (realloc_ok) {		/* just grow in place, if possible */
		buffer = saferealloc(buffer,(MEM_SIZE)buffer_length+1);
	    }
	    else {
		char* tmp = safemalloc((MEM_SIZE)buffer_length+1);
		strncpy(tmp,buffer,buffer_length/2);
		buffer = tmp;
		realloc_ok = true;
	    }
	}
	if ((nextch = getc(fp)) == EOF) {
	    if (!bufix)
		return nullptr;
	    break;
	}
	buffer[bufix++] = (char)nextch;
    } while (nextch && nextch != '\n');
    buffer[bufix] = '\0';
    g_len_last_line_got = bufix;
    g_buflen_last_line_got = buffer_length;
    return buffer;
}

int makedir(char *dirname, int nametype)
{
    char* end;
    char* s;
# ifdef HAS_MKDIR
    int status = 0;
# else
    char tmpbuf[1024];
    register char* tbptr = tmpbuf+5;
# endif

    for (end = dirname; *end; end++) ;	/* find the end */
    if (nametype == MD_FILE) {		/* not to create last component? */
	for (--end; end != dirname && *end != '/'; --end) ;
	if (*end != '/')
	    return 0;			/* nothing to make */
	*end = '\0';			/* isolate file name */
    }
# ifndef HAS_MKDIR
    strcpy(tmpbuf,"mkdir");
# endif

    s = end;
    for (;;) {
	if (stat(dirname,&g_filestat) >= 0 && S_ISDIR(g_filestat.st_mode)) {
					/* does this much exist as a dir? */
	    *s = '/';			/* mark this as existing */
	    break;
	}
	s = strrchr(dirname,'/');	/* shorten name */
	if (!s)				/* relative path! */
	    break;			/* hope they know what they are doing */
	*s = '\0';			/* mark as not existing */
    }
    
    for (s=dirname; s <= end; s++) {	/* this is grody but efficient */
	if (!*s) {			/* something to make? */
# ifdef HAS_MKDIR
#ifdef _WIN32
	    status = status || mkdir(dirname);
#else
	    status = status || mkdir(dirname,0777);
#endif
# else
	    sprintf(tbptr," %s",dirname);
	    tbptr += strlen(tbptr);	/* make it, sort of */
# endif
	    *s = '/';			/* mark it made */
	}
    }
    if (nametype == MD_DIR)		/* don't need final slash unless */
	*end = '\0';			/*  a filename follows the dir name */

# ifdef HAS_MKDIR
    return status;
# else
    return (tbptr==tmpbuf+5 ? 0 : doshell(g_sh,tmpbuf));/* exercise our faith */
# endif
}

void notincl(const char *feature)
{
    printf("\nNo room for feature \"%s\" on this machine.\n",feature) FLUSH;
}

/* grow a static string to at least a certain length */

void growstr(char **strptr, int *curlen, int newlen)
{
    if (newlen > *curlen) {		/* need more room? */
	if (*curlen)
	    *strptr = saferealloc(*strptr,(MEM_SIZE)newlen);
	else
	    *strptr = safemalloc((MEM_SIZE)newlen);
	*curlen = newlen;
    }
}

void setdef(char *buffer, const char *dflt)
{
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    if (*buffer == ' '
#ifndef STRICTCR
     || *buffer == '\n' || *buffer == '\r'
#endif
    ) {
	g_s_default_cmd = true;
	g_univ_default_cmd = true;
	if (*dflt == '^' && isupper(dflt[1]))
	    pushchar(Ctl(dflt[1]));
	else
	    pushchar(*dflt);
	getcmd(buffer);
    }
}

#ifndef NO_FILELINKS
void safelink(char *old, char *new)
{
#if 0
    extern int sys_nerr;
    extern char* sys_errlist[];
#endif

    if (link(old,new)) {
	printf("Can't link backup (%s) to .newsrc (%s)\n", old, new) FLUSH;
#if 0
	if (errno>0 && errno<sys_nerr)
	    printf("%s\n", sys_errlist[errno]);
#endif
	finalize(1);
    }
}
#endif

/* attempts to verify a cryptographic signature. */
void verify_sig()
{
    int i;

    printf("\n");
    /* RIPEM */
    i = doshell(g_sh,filexp("grep -s \"BEGIN PRIVACY-ENHANCED MESSAGE\" %A"));
    if (!i) {	/* found RIPEM */
	i = doshell(g_sh,filexp(get_val("VERIFY_RIPEM",VERIFY_RIPEM)));
	printf("\nReturned value: %d\n",i) FLUSH;
	return;
    }
    /* PGP */
    i = doshell(g_sh,filexp("grep -s \"BEGIN PGP\" %A"));
    if (!i) {	/* found PGP */
	i = doshell(g_sh,filexp(get_val("VERIFY_PGP",VERIFY_PGP)));
	printf("\nReturned value: %d\n",i) FLUSH;
	return;
    }
    printf("No PGP/RIPEM signatures detected.\n") FLUSH;
}

double current_time()
{
#ifdef HAS_GETTIMEOFDAY
    Timeval t;
    (void) gettimeofday(&t, (struct timezone*)nullptr);
    return (double)t.tv_usec / 1000000. + t.tv_sec;
#else
# ifdef HAS_FTIME
    Timeval t;
    ftime(&t);
    return (double)t.millitm / 1000. + t.time;
# else
    return (double)time((time_t*)nullptr);
# endif
#endif
}

time_t text2secs(char *s, time_t defSecs)
{
    time_t secs = 0;
    time_t item;

    if (!isdigit(*s)) {
	if (*s == 'm' || *s == 'M')	/* "missing" */
	    return 2;
	if (*s == 'y' || *s == 'Y')	/* "yes" */
	    return defSecs;
	return secs;			/* "never" */
    }
    do {
	item = atol(s);
	while (isdigit(*s)) s++;
	while (isspace(*s)) s++;
	if (isalpha(*s)) {
	    switch (*s) {
	      case 'd': case 'D':
		item *= 24 * 60L;
		break;
	      case 'h': case 'H':
		item *= 60L;
		break;
	      case 'm': case 'M':
		break;
	      default:
		item = 0;
		break;
	    }
	    while (isalpha(*s)) s++;
	    if (*s == ',') s++;
	    while (isspace(*s)) s++;
	}
	secs += item;
    } while (isdigit(*s));

    return secs * 60;
}

char *secs2text(time_t secs)
{
    char* s = g_buf;
    int items;

    if (!secs || (secs & 1))
	return "never";
    if (secs & 2)
	return "missing";

    secs /= 60;
    if (secs >= 24L * 60) {
	items = (int)(secs / (24*60));
	secs = secs % (24*60);
	sprintf(s, "%d day%s, ", items, PLURAL(items));
	s += strlen(s);
    }
    if (secs >= 60L) {
	items = (int)(secs / 60);
	secs = secs % 60;
	sprintf(s, "%d hour%s, ", items, PLURAL(items));
	s += strlen(s);
    }
    if (secs) {
	sprintf(s, "%d minute%s, ", (int)secs, PLURAL(items));
	s += strlen(s);
    }
    s[-2] = '\0';
    return g_buf;
}

/* returns a saved string representing a unique temporary filename */
char *temp_filename()
{
    static int tmpfile_num = 0;
    char tmpbuf[CBUFLEN];
    extern long g_our_pid;
    sprintf(tmpbuf,"%s/trn%d.%ld",g_tmp_dir,tmpfile_num++,g_our_pid);
    return savestr(tmpbuf);
}

char *get_auth_user()
{
    return g_datasrc->auth_user;
}

char *get_auth_pass()
{
    return g_datasrc->auth_pass;
}

char **prep_ini_words(INI_WORDS words[])
{
    int checksum;
    char* cp = (char*)INI_VALUES(words);
    if (!cp) {
	int i;
	for (i = 1; words[i].item != nullptr; i++) {
	    if (*words[i].item == '*') {
		words[i].checksum = -1;
		continue;
	    }
	    checksum = 0;
	    for (cp = words[i].item; *cp; cp++)
		checksum += (isupper(*cp)? tolower(*cp) : *cp);
	    words[i].checksum = (checksum << 8) + (cp - words[i].item);
	}
	words[0].checksum = i;
	words[0].help_str = cp = safemalloc(i * sizeof (char*));
    }
    memset(cp,0,(words)[0].checksum * sizeof (char*));
    return (char**)cp;
}

void unprep_ini_words(INI_WORDS words[])
{
    free((char*)INI_VALUES(words));
    words[0].checksum = 0;
    words[0].help_str = nullptr;
}

void prep_ini_data(char *cp, char *filename)
{
    char* t = cp;

#ifdef DEBUG
    if (debug & DEB_RCFILES)
	printf("Read %d bytes from %s\n",strlen(cp),filename);
#endif

    while (*cp) {
	while (isspace(*cp)) cp++;

	if (*cp == '[') {
	    char* s = t;
	    do {
		*t++ = *cp++;
	    } while (*cp && *cp != ']' && *cp != '\n');
	    if (*cp == ']' && t != s) {
		*t++ = '\0';
		cp++;
		if (parse_string(&t, &cp))
		    cp++;

		while (*cp) {
		    while (isspace(*cp)) cp++;
		    if (*cp == '[')
			break;
		    if (*cp == '#')
			s = cp;
		    else {
			s = t;
			while (*cp && *cp != '\n') {
			    if (*cp == '=')
				break;
			    if (isspace(*cp)) {
				if (s == t || t[-1] != ' ')
				    *t++ = ' ';
				cp++;
			    }
			    else
				*t++ = *cp++;
			}
			if (*cp == '=' && t != s) {
			    while (t != s && isspace(t[-1])) t--;
			    *t++ = '\0';
			    cp++;
			    if (parse_string(&t, &cp))
				s = nullptr;
			    else
				s = cp;
			}
			else
			    s = cp;
		    }
		    cp++;
		    if (s)
			for (cp = s; *cp && *cp++ != '\n'; ) ;
		}
	    }
	    else {
		*t = '\0';
		printf("Invalid section in %s: %s\n", filename, s);
		t = s;
		while (*cp && *cp++ != '\n') ;
	    }
	}
	else
	    while (*cp && *cp++ != '\n') ;
    }
    *t = '\0';
}

bool parse_string(char **to, char **from)
{
    char inquote = 0;
    char* t = *to;
    char* f = *from;
    char* s;

    while (isspace(*f) && *f != '\n') f++;

    for (s = t; *f; f++) {
	if (inquote) {
	    if (*f == inquote) {
		inquote = 0;
		s = t;
		continue;
	    }
	}
	else if (*f == '\n')
	    break;
	else if (*f == '\'' || *f == '"') {
	    inquote = *f;
	    continue;
	}
	else if (*f == '#') {
	    while (*++f && *f != '\n') ;
	    break;
	}
	if (*f == '\\') {
	    if (*++f == '\n')
		continue;
	    f = interp_backslash(t, f);
	    t++;
	}
	else
	    *t++ = *f;
    }
#if 0
    if (inquote)
	printf("Unbalanced quotes.\n");
#endif
    inquote = (*f != '\0');

    while (t != s && isspace(t[-1])) t--;
    *t++ = '\0';

    *to = t;
    *from = f;

    return inquote;	/* return true if the string ended with a newline */
}

char *next_ini_section(char *cp, char **section, char **cond)
{
    while (*cp != '[') {
	if (!*cp)
	    return nullptr;
	cp += strlen(cp) + 1;
	cp += strlen(cp) + 1;
    }
    *section = cp+1;
    cp += strlen(cp) + 1;
    *cond = cp;
    cp += strlen(cp) + 1;
#ifdef DEBUG
    if (debug & DEB_RCFILES)
	printf("Section [%s] (condition: %s)\n",*section,
	       **cond? *cond : "<none>");
#endif
    return cp;
}

char *parse_ini_section(char *cp, INI_WORDS words[])
{
    int checksum;
    char* s;
    char** values = prep_ini_words(words);
    int i;

    if (!*cp)
	return nullptr;

    while (*cp && *cp != '[') {
	checksum = 0;
	for (s = cp; *s; s++) {
	    if (isupper(*s))
		*s = tolower(*s);
	    checksum += *s;
	}
	checksum = (checksum << 8) + (s++ - cp);
	if (*s) {
	    for (i = 1; words[i].checksum; i++) {
		if (words[i].checksum == checksum
		 && !strcasecmp(cp,words[i].item)) {
		    values[i] = s;
		    break;
		}
	    }
	    if (!words[i].checksum)
		printf("Unknown option: `%s'.\n",cp);
	    cp = s + strlen(s) + 1;
	}
	else
	    cp = s + 1;
    }

#ifdef DEBUG
    if (debug & DEB_RCFILES) {
	printf("Ini_words: %s\n", words[0].item);
	for (i = 1; words[i].checksum; i++)
	    if (values[i])
		printf("%s=%s\n",words[i].item,values[i]);
    }
#endif

    return cp;
}

bool check_ini_cond(char *cond)
{
    int not, equal, upordown, num;
    char* s;
    cond = dointerp(g_buf,sizeof g_buf,cond,"!=<>",nullptr);
    s = g_buf + strlen(g_buf);
    while (s != g_buf && isspace(s[-1])) s--;
    *s = '\0';
    if ((not = (*cond == '!')) != 0)
	cond++;
    if ((upordown = (*cond=='<'? -1: (*cond=='>'? 1:0))) != 0)
	cond++;
    if ((equal = (*cond == '=')) != 0)
	cond++;
    while (isspace(*cond)) cond++;
    if (upordown) {
	num = atoi(cond) - atoi(g_buf);
	if (!((equal && !num) || (upordown * num < 0)) ^ not)
	    return false;
    }
    else if (equal) {
	COMPEX condcompex;
	init_compex(&condcompex);
	if ((s = compile(&condcompex,cond,true,true)) != nullptr) {
	    /*warning(s)*/;
	    equal = false;
	}
	else
	    equal = execute(&condcompex,g_buf) != nullptr;
	free_compex(&condcompex);
	return equal;
    }
    else
	return false;
    return true;
}

/* $$ might get replaced soonish... */
/* Ask for a single character (improve the prompt?) */
char menu_get_char()
{
    printf("Enter your choice: ");
    fflush(stdout);
    eat_typeahead();
    getcmd(g_buf);
    printf("%c\n",*g_buf) FLUSH;
    return(*g_buf);
}

/* NOTE: kfile.c uses its own editor function */
/* used in a few places, now centralized */
int edit_file(char *fname)
{
    int r = -1;

    if (!fname || !*fname)
	return r;

    /* XXX paranoia check on length */
    sprintf(g_cmd_buf,"%s ",
	    filexp(get_val("VISUAL",get_val("EDITOR",g_defeditor))));
    strcat(g_cmd_buf, filexp(fname));
    termdown(3);
    resetty();			/* make sure tty is friendly */
    r = doshell(g_sh,g_cmd_buf);/* invoke the shell */
    noecho();			/* and make terminal */
    crmode();			/*   unfriendly again */
    return r;
}

/* Consider a trn_pushdir, trn_popdir pair of functions */
