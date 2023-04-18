/* env.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "env-internal.h"

#include "init.h"
#include "util.h"
#include "util2.h"

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

#include <memory>

char       *g_home_dir{};    /* login directory */
std::string g_dot_dir;       /* where . files go */
std::string g_trn_dir;       /* usually %./.trn */
std::string g_lib;           /* news library */
std::string g_rn_lib;        /* private news program library */
std::string g_tmp_dir;       /* where tmp files go */
std::string g_login_name;    /* login id of user */
std::string g_real_name;     /* real name of user */
std::string g_p_host_name;   /* host name in a posting */
char       *g_local_host{};  /* local host name */
int         g_net_speed{20}; /* how fast our net-connection is */

static void env_init2();
static int  envix(const char *nam, int len);
static bool set_user_name(char *tmpbuf);
static bool set_p_host_name(char *tmpbuf);

bool env_init(char *tcbuf, bool lax, const std::function<bool(char *tmpbuf)> &set_user_name_fn,
              const std::function<bool(char *tmpbuf)> &set_host_name_fn)
{
    bool fully_successful = true;

    const char *home_dir = getenv("HOME");
    if (home_dir == nullptr)
	home_dir = getenv("LOGDIR");
    if (home_dir)
        g_home_dir = savestr(home_dir);

    const char *val = getenv("TMPDIR");
    if (val == nullptr)
	g_tmp_dir = get_val("TMP","/tmp");
    else
	g_tmp_dir = val;

    /* try to set g_login_name */
    if (lax) {
	const char *login_name = getenv("USER");
	if (!login_name)
	    login_name = getenv("LOGNAME");
	if (login_name && g_login_name.empty())
	{
	    g_login_name  = login_name;
	}
    }
#ifndef MSDOS
    if (!lax || g_login_name.empty()) {
	g_login_name = getlogin();
    }
#endif
#ifdef MSDOS
    if (g_login_name.empty())
    {
	if (const char *user_name = getenv("USERNAME"))
	{
            g_login_name = user_name;
	}
    }
    if (!g_home_dir)
    {
        char *home_drive = getenv("HOMEDRIVE");
        char *home_path = getenv("HOMEPATH");
        if (home_drive != nullptr && home_path != nullptr)
        {
            strcpy(tcbuf, home_drive);
            strcat(tcbuf, home_path);
            g_home_dir = savestr(tcbuf);
        }
    }
#endif

    /* Set g_real_name, and maybe set g_login_name and g_home_dir (if nullptr). */
    if (!set_user_name_fn(tcbuf)) {
	g_login_name.clear();
	g_real_name.clear();
	fully_successful = false;
    }
    env_init2();

    /* set g_p_host_name to the hostname of our local machine */
    if (!set_host_name_fn(tcbuf))
    {
        if (!g_local_host)
            g_local_host = savestr("");
        fully_successful = false;
    }

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

bool env_init(char *tcbuf, bool lax)
{
    return env_init(tcbuf, lax, set_user_name, set_p_host_name);
}

void env_final()
{
    g_p_host_name.clear();
    safefree0(g_local_host);
    g_real_name.clear();
    g_login_name.clear();
    safefree0(g_home_dir);
    g_tmp_dir.clear();
    g_dot_dir.clear();
    g_trn_dir.clear();
    g_lib.clear();
    g_rn_lib.clear();
}

static void env_init2()
{
    if (!g_dot_dir.empty()) /* Avoid running multiple times. */
	return;

    if (!g_home_dir)
	g_home_dir = savestr("/");
    g_dot_dir = get_val("DOTDIR",g_home_dir);
    g_trn_dir = filexp(get_val("TRNDIR",TRNDIR));
    g_lib = filexp(NEWSLIB);
    g_rn_lib = filexp(PRIVLIB);
}

/* Set g_login_name to the user's login name and g_real_name to the user's
** real name.
*/
static bool set_user_name(char *tmpbuf)
{
    char* s;
    char* c;

#ifdef HAS_GETPWENT
    passwd* pwd;

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
	return false;
    if (!g_login_name) {
	cpytill(g_buf,tmpbuf+1,':');
	g_login_name = savestr(g_buf);
    }
    for (s = tmpbuf, i = GCOSFIELD-1; i; i--) {
	if (s)
	    s = strchr(s+1,':');
    }
    if (!s)
	return false;
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
    c = strchr(s, ',');
    if (c != nullptr)
	*c = '\0';
    c = strchr(s, ';');
    if (c != nullptr)
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
    c = strchr(s, '(');
    if (c != nullptr)
	*c = '\0';
    c = strchr(s, '-');
    if (c != nullptr)
	s = c;
    g_real_name = savestr(s);
#endif /* !BERKNAMES */
#endif
#ifndef PASSNAMES
    {
        env_init2(); /* Make sure g_home_dir/g_dot_dir/etc. are set. */
        FILE *fp = fopen(filexp(FULLNAMEFILE), "r");
        if (fp != nullptr)
        {
	    fgets(g_buf,sizeof g_buf,fp);
	    fclose(fp);
	    g_buf[strlen(g_buf)-1] = '\0';
	    g_real_name = g_buf;
	}
    }
#ifdef WIN32
    if (g_login_name.empty())
    {
	DWORD size = 0;
        GetUserNameExA(NameSamCompatible, nullptr, &size);
	std::unique_ptr<char> buffer{new char[size]};
	GetUserNameExA(NameSamCompatible, buffer.get(), &size);
	std::string value{buffer.get()};
	std::string::size_type backslash = value.find_last_of('\\');
	if (backslash != std::string::npos)
	    value = value.substr(backslash + 1);
	g_login_name = value;
    }
    if (g_real_name.empty())
    {
	DWORD size = 0;
        GetUserNameExA(NameDisplay, nullptr, &size);
	char *buffer = safemalloc(size);
	GetUserNameExA(NameDisplay, buffer, &size);
	g_real_name = buffer;
	safefree(buffer);
    }
#endif
#endif /* !PASSNAMES */
#ifdef HAS_GETPWENT
    endpwent();
#endif
    if (g_real_name.empty())
    {
        g_real_name = "PUT_YOUR_NAME_HERE";
    }
    return true;
}

static bool set_p_host_name(char *tmpbuf)
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

    const char *filename{PHOSTNAME};
    if (FILE_REF(filename) || filename[0] == '~')
    {
        fp = fopen(filexp(filename), "r");
        if (fp == nullptr)
            strcpy(tmpbuf, ".");
        else
        {
            fgets(tmpbuf, TCBUF_SIZE, fp);
            fclose(fp);
            char *end = tmpbuf + strlen(tmpbuf) - 1;
            if (*end == '\n')
                *end = '\0';
        }
    }
    else
        strcpy(tmpbuf, PHOSTNAME);

    if (tmpbuf[0] == '.') {
	if (tmpbuf[1] != '\0')
	    strcpy(g_buf,tmpbuf);
	else
	    g_buf[0] = '\0';
	strcpy(tmpbuf,g_local_host);
	strcat(tmpbuf,g_buf);
    }

    if (!strchr(tmpbuf,'.')) {
	if (tmpbuf[0])
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
    g_p_host_name = tmpbuf;
    return hostname_ok;
}

char *get_val(const char *nam, char *def)
{
    char *val = getenv(nam);
    if (val == nullptr || !*val)
	return def;
    return val;
}

static bool s_firstexport = true;

#ifndef WIN32
extern char **environ;
#endif

char *export_var(const char *nam, const char *val)
{
#if 1
    std::string envar{nam};
    envar += '=';
    envar += val;
    putenv(envar.c_str());
    return savestr(envar.c_str());
#else
    int namlen = strlen(nam);
    int i=envix(nam,namlen);	/* where does it go? */

    if (!environ[i]) {			/* does not exist yet */
	if (s_firstexport) {		/* need we copy environment? */
#ifndef lint
	    char** tmpenv = (char**)	/* point our wand at memory */
		safemalloc((MEM_SIZE) (i+2) * sizeof(char*));
#else
	    char** tmpenv = nullptr;
#endif /* lint */
    
	    s_firstexport = false;
	    for (int j = 0; j < i; j++) /* copy environment */
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
#endif
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
	    break;			/* strncmp must come first to avoid */
    }					/* potential SEGV's */
    return i;
}
