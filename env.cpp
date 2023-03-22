/* env.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "init.h"
#include "util.h"
#include "util2.h"
#include "env.h"

#ifdef HAS_RES_INIT
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define SECURITY_WIN32
#include <security.h>
#pragma comment(lib, "Secur32")
#include <winsock2.h>
#endif

char *g_home_dir{};    /* login directory */
char *g_dot_dir{};     /* where . files go */
char *g_trn_dir{};     /* usually %./.trn */
char *g_lib{};         /* news library */
char *g_rn_lib{};      /* private news program library */
char *g_tmp_dir{};     /* where tmp files go */
char *g_login_name{};  /* login id of user */
char *g_real_name{};   /* real name of user */
char *g_p_host_name{}; /* host name in a posting */
char *g_local_host{};  /* local host name */
int g_net_speed{20};   /* how fast our net-connection is */

static void env_init2();
static int envix(const char *nam, int len);

bool env_init(char *tcbuf, bool lax)
{
    bool fully_successful = true;

    if ((g_home_dir = getenv("HOME")) == nullptr)
	g_home_dir = getenv("LOGDIR");

    if ((g_tmp_dir = getenv("TMPDIR")) == nullptr)
	g_tmp_dir = get_val("TMP","/tmp");

    /* try to set g_login_name */
    if (lax) {
	g_login_name = getenv("USER");
	if (!g_login_name)
	    g_login_name = getenv("LOGNAME");
    }
#ifndef MSDOS
    if (!lax || !g_login_name) {
	g_login_name = getlogin();
	if (g_login_name)
	    g_login_name = savestr(g_login_name);
    }
#endif
#ifdef MSDOS
    g_login_name = getenv("USERNAME");
    char *home_drive = getenv("HOMEDRIVE");
    char *home_path = getenv("HOMEPATH");
    if (home_drive != nullptr && home_path != nullptr)
    {
	strcpy(tcbuf, home_drive);
	strcat(tcbuf, home_path);
	g_home_dir = savestr(tcbuf);
    }
#endif

    /* Set g_real_name, and maybe set g_login_name and g_home_dir (if nullptr). */
    if (!set_user_name(tcbuf)) {
	if (!g_login_name)
	    g_login_name = "";
	if (!g_real_name)
	    g_real_name = "";
	fully_successful = false;
    }
    env_init2();

    /* set g_p_host_name to the hostname of our local machine */
    if (!set_p_host_name(tcbuf))
	fully_successful = false;

    {
	char* cp = get_val("NETSPEED","5");
	if (*cp == 'f')
	    g_net_speed = 10;
	else if (*cp == 's')
	    g_net_speed = 1;
	else {
	    g_net_speed = atoi(cp);
	    if (g_net_speed < 1)
		g_net_speed = 1;
	}
    }

    return fully_successful;
}

static void env_init2()
{
    if (g_dot_dir)		/* Avoid running multiple times. */
	return;
    if (!g_home_dir)
	g_home_dir = "/";
    g_dot_dir = get_val("DOTDIR",g_home_dir);
    g_trn_dir = savestr(filexp(get_val("TRNDIR",TRNDIR)));
    g_lib = savestr(filexp(NEWSLIB));
    g_rn_lib = savestr(filexp(PRIVLIB));
}

/* Set g_login_name to the user's login name and g_real_name to the user's
** real name.
*/
bool set_user_name(char *tmpbuf)
{
    char* s;
    char* c;

#ifdef HAS_GETPWENT
    struct passwd* pwd;

    if (g_login_name == nullptr)
	pwd = getpwuid(getuid());
    else
	pwd = getpwnam(g_login_name);
    if (!pwd)
	return 0;
    if (!g_login_name)
	g_login_name = savestr(pwd->pw_name);
    if (!g_home_dir)
	g_home_dir = savestr(pwd->pw_dir);
    s = pwd->pw_gecos;
#endif
#ifdef HAS_GETPW
    int i;

    if (getpw(getuid(), tmpbuf+1) != 0)
	return 0;
    if (!g_login_name) {
	cpytill(g_buf,tmpbuf+1,':');
	g_login_name = savestr(g_buf);
    }
    for (s = tmpbuf, i = GCOSFIELD-1; i; i--) {
	if (s)
	    s = strchr(s+1,':');
    }
    if (!s)
	return 0;
    s = cpytill(tmpbuf,s+1,':');
    if (!g_home_dir) {
	cpytill(g_buf,s+1,':');
	g_home_dir = savestr(g_buf);
    }
    s = tmpbuf;
#endif
#ifdef PASSNAMES
#ifdef BERKNAMES
#ifdef BERKJUNK
    while (*s && !isalnum(*s) && *s != '&') s++;
#endif
    if ((c = strchr(s, ',')) != nullptr)
	*c = '\0';
    if ((c = strchr(s, ';')) != nullptr)
	*c = '\0';
    s = cpytill(g_buf,s,'&');
    if (*s == '&') {			/* whoever thought this one up was */
	c = g_buf + strlen(g_buf);		/* in the middle of the night */
	strcat(c,g_login_name);		/* before the morning after */
	strcat(c,s+1);
	if (islower(*c))
	    *c = toupper(*c);		/* gack and double gack */
    }
    g_real_name = savestr(g_buf);
#else /* !BERKNAMES */
    if ((c = strchr(s, '(')) != nullptr)
	*c = '\0';
    if ((c = strchr(s, '-')) != nullptr)
	s = c;
    g_real_name = savestr(s);
#endif /* !BERKNAMES */
#endif
#ifndef PASSNAMES
    {
	FILE* fp;
	env_init2(); /* Make sure g_home_dir/g_dot_dir/etc. are set. */
	if ((fp = fopen(filexp(FULLNAMEFILE),"r")) != nullptr) {
	    fgets(g_buf,sizeof g_buf,fp);
	    fclose(fp);
	    g_buf[strlen(g_buf)-1] = '\0';
	    g_real_name = savestr(g_buf);
	}
    }
#ifdef WIN32
    if (g_real_name == nullptr)
    {
	DWORD size = 0;
        GetUserNameExA(NameDisplay, nullptr, &size);
	g_real_name = safemalloc(size);
	GetUserNameExA(NameDisplay, g_real_name, &size);
    }
#endif
#endif /* !PASSNAMES */
#ifdef HAS_GETPWENT
    endpwent();
#endif
    if (g_real_name == nullptr)
    {
        g_real_name = savestr("PUT_YOUR_NAME_HERE");
    }
    return true;
}

bool set_p_host_name(char *tmpbuf)
{
    FILE* fp;
    bool hostname_ok = true;

    /* Find the local hostname */

#ifdef HAS_GETHOSTNAME
#ifdef WIN32
    const WORD version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);
#endif
    gethostname(tmpbuf,TCBUF_SIZE);
#else
# ifdef HAS_UNAME
    /* get sysname */
    uname(&utsn);
    strcpy(tmpbuf,utsn.nodename);
# else
#  ifdef PHOSTCMD
    {
	FILE* popen();
	FILE* pipefp = popen(PHOSTCMD,"r");
	
	if (pipefp == nullptr) {
	    printf("Can't find hostname\n");
	    finalize(1);
	}
	fgets(tmpbuf,TCBUF_SIZE,pipefp);
	tmpbuf[strlen(tmpbuf)-1] = '\0';	/* wipe out newline */
	pclose(pipefp);
    }
#  else
    strcpy(tmpbuf, "!INVALID!");
#  endif /* PHOSTCMD */
# endif /* HAS_UNAME */
#endif /* HAS_GETHOSTNAME */
    g_local_host = savestr(tmpbuf);

    /* Build the host name that goes in postings */

    g_p_host_name = PHOSTNAME;
    if (FILE_REF(g_p_host_name) || *g_p_host_name == '~') {
	g_p_host_name = filexp(g_p_host_name);
	if ((fp = fopen(g_p_host_name,"r")) == nullptr)
	    strcpy(tmpbuf,".");
	else {
	    fgets(tmpbuf,TCBUF_SIZE,fp);
	    fclose(fp);
	    g_p_host_name = tmpbuf + strlen(tmpbuf) - 1;
	    if (*g_p_host_name == '\n')
		*g_p_host_name = '\0';
	}
    }
    else
	strcpy(tmpbuf,g_p_host_name);

    if (*tmpbuf == '.') {
	if (tmpbuf[1] != '\0')
	    strcpy(g_buf,tmpbuf);
	else
	    *g_buf = '\0';
	strcpy(tmpbuf,g_local_host);
	strcat(tmpbuf,g_buf);
    }

    if (!strchr(tmpbuf,'.')) {
	if (*tmpbuf)
	    strcat(tmpbuf, ".");
#ifdef HAS_RES_INIT
	if (!(_res.options & RES_INIT))
	    res_init();
	if (_res.defdname != nullptr)
	    strcat(tmpbuf,_res.defdname);
	else
#endif
#ifdef HAS_GETDOMAINNAME
	if (getdomainname(g_buf,LBUFLEN) == 0)
	    strcat(tmpbuf,g_buf);
	else
#endif
	{
	    strcat(tmpbuf,"UNKNOWN.HOST");
	    hostname_ok = false;
	}
    }
    g_p_host_name = savestr(tmpbuf);
    return hostname_ok;
}

char *get_val(const char *nam, char *def)
{
    char* val;

    if ((val = getenv(nam)) == nullptr || !*val)
	return def;
    return val;
}

static bool firstexport = true;

#ifndef WIN32
extern char **environ;
#endif

char *export_var(const char *nam, const char *val)
{
    int namlen = strlen(nam);
    int i=envix(nam,namlen);	/* where does it go? */

    if (!environ[i]) {			/* does not exist yet */
	if (firstexport) {		/* need we copy environment? */
	    int j;
#ifndef lint
	    char** tmpenv = (char**)	/* point our wand at memory */
		safemalloc((MEM_SIZE) (i+2) * sizeof(char*));
#else
	    char** tmpenv = nullptr;
#endif /* lint */
    
	    firstexport = false;
	    for (j = 0; j < i; j++)	/* copy environment */
		tmpenv[j] = environ[j];
	    environ = tmpenv;		/* tell exec where it is now */
	}
#ifndef lint
	else
	    environ = (char**) saferealloc((char*) environ,
		(MEM_SIZE) (i+2) * sizeof(char*));
					/* just expand it a bit */
#endif /* lint */
	environ[i+1] = nullptr;	/* make sure it's null terminated */
    }
    environ[i] = safemalloc((MEM_SIZE)(namlen + strlen(val) + 2));
					/* this may or may not be in */
					/* the old environ structure */
    sprintf(environ[i],"%s=%s",nam,val);/* all that work just for this */
    return environ[i] + namlen + 1;
}

void un_export(char *export_val)
{
    if (export_val[-1] == '=' && export_val[-2] != '_') {
	export_val[0] = export_val[-2];
	export_val[1] = '\0';
	export_val[-2] = '_';
    }
}

void re_export(char *export_val, const char *new_val, int limit)
{
    if (export_val[-1] == '=' && export_val[-2] == '_' && !export_val[1])
	export_val[-2] = export_val[0];
    safecpy(export_val, new_val, limit+1);
}

static int envix(const char *nam, int len)
{
    int i;

    for (i = 0; environ[i]; i++) {
	if (!strncmp(environ[i],nam,len) && environ[i][len] == '=')
	    break;			/* strnEQ must come first to avoid */
    }					/* potential SEGV's */
    return i;
}
