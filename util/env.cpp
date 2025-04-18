/* env.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "util/env-internal.h"

#include <config/common.h>
#include <config/pipe_io.h>
#include <trn/init.h>
#include <trn/util.h>
#include <util/util2.h>

#ifdef HAS_RES_INIT
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#define SECURITY_WIN32
#include <security.h>
#pragma comment(lib, "Secur32")
#include <winsock2.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>

char       *g_home_dir{};    // login directory
std::string g_dot_dir;       // where . files go
std::string g_trn_dir;       // usually %./.trn
std::string g_lib;           // news library
std::string g_rn_lib;        // private news program library
std::string g_tmp_dir;       // where tmp files go
std::string g_login_name;    // login name of user
std::string g_real_name;     // real name of user
std::string g_p_host_name;   // host name in a posting
char       *g_local_host{};  // local host name
int         g_net_speed{20}; // how fast our net-connection is

static std::function<char *(const char *name)> s_getenv_fn = std::getenv;

static void env_init2();
static int  envix(const char *nam, int len);
static bool set_user_name(char *tmpbuf);
static bool set_p_host_name(char *tmpbuf);

void set_environment(std::function<char *(const char *)> getenv_fn)
{
    if (getenv_fn)
    {
        s_getenv_fn = std::move(getenv_fn);
    }
    else
    {
        s_getenv_fn = std::getenv;
    }
}

bool env_init(char *tcbuf, bool lax, const std::function<bool(char *tmpbuf)> &set_user_name_fn,
              const std::function<bool(char *tmpbuf)> &set_host_name_fn)
{
    bool fully_successful = true;

    const char *home_dir = s_getenv_fn("HOME");
    if (home_dir == nullptr)
    {
        home_dir = s_getenv_fn("LOGDIR");
    }
    if (home_dir)
    {
        g_home_dir = save_str(home_dir);
    }

    const char *val = s_getenv_fn("TMPDIR");
    if (val == nullptr)
    {
        g_tmp_dir = get_val_const("TMP","/tmp");
    }
    else
    {
        g_tmp_dir = val;
    }

    // try to set g_login_name
    if (lax)
    {
        const char *login_name = s_getenv_fn("USER");
        if (!login_name)
        {
            login_name = s_getenv_fn("LOGNAME");
        }
        if (login_name && g_login_name.empty())
        {
            g_login_name  = login_name;
        }
    }
#ifndef MSDOS
    if (!lax || g_login_name.empty())
    {
        if (const char *login = getlogin())
        {
            g_login_name = login;
        }
    }
#endif
#ifdef MSDOS
    if (g_login_name.empty())
    {
        if (const char *user_name = s_getenv_fn("USERNAME"))
        {
            g_login_name = user_name;
        }
    }
    if (!g_home_dir)
    {
        char *home_drive = s_getenv_fn("HOMEDRIVE");
        char *home_path = s_getenv_fn("HOMEPATH");
        if (home_drive != nullptr && home_path != nullptr)
        {
            std::strcpy(tcbuf, home_drive);
            std::strcat(tcbuf, home_path);
            g_home_dir = save_str(tcbuf);
        }
    }
#endif

    // Set g_real_name, and maybe set g_login_name and g_home_dir (if nullptr).
    if (!set_user_name_fn(tcbuf))
    {
        g_login_name.clear();
        g_real_name.clear();
        fully_successful = false;
    }
    env_init2();

    // set g_p_host_name to the hostname of our local machine
    if (!set_host_name_fn(tcbuf))
    {
        if (!g_local_host)
        {
            g_local_host = save_str("");
        }
        fully_successful = false;
    }

    {
        const char* cp = get_val_const("NETSPEED","5");
        if (*cp == 'f')
        {
            g_net_speed = 10;
        }
        else if (*cp == 's')
        {
            g_net_speed = 1;
        }
        else
        {
            g_net_speed = std::atoi(cp);
            g_net_speed = std::max(g_net_speed, 1);
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
    safe_free0(g_local_host);
    g_real_name.clear();
    g_login_name.clear();
    safe_free0(g_home_dir);
    g_tmp_dir.clear();
    g_dot_dir.clear();
    g_trn_dir.clear();
    g_lib.clear();
    g_rn_lib.clear();
}

static void env_init2()
{
    if (!g_dot_dir.empty()) // Avoid running multiple times.
    {
        return;
    }

    if (!g_home_dir)
    {
        g_home_dir = save_str("/");
    }
    g_dot_dir = get_val_const("DOTDIR",g_home_dir);
    g_trn_dir = file_exp(get_val_const("TRNDIR",TRNDIR));
    g_lib = file_exp(NEWSLIB);
    g_rn_lib = file_exp(PRIVLIB);
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

    if (g_login_name.empty())
    {
        pwd = getpwuid(getuid());
    }
    else
    {
        pwd = getpwnam(g_login_name.c_str());
    }
    if (!pwd)
    {
        return 0;
    }
    if (g_login_name.empty())
    {
        g_login_name = pwd->pw_name;
    }
    if (!g_home_dir)
    {
        g_home_dir = save_str(pwd->pw_dir);
    }
    s = pwd->pw_gecos;
#endif
#ifdef PASSNAMES
#ifdef BERKNAMES
#ifdef BERKJUNK
    while (*s && !isalnum(*s) && *s != '&')
    {
        s++;
    }
#endif
    c = std::strchr(s, ',');
    if (c != nullptr)
    {
        *c = '\0';
    }
    c = std::strchr(s, ';');
    if (c != nullptr)
    {
        *c = '\0';
    }
    s = cpytill(g_buf,s,'&');
    if (*s == '&')                      // whoever thought this one up was
    {
        c = g_buf + std::strlen(g_buf);      // in the middle of the night
        std::strcat(c, g_login_name.c_str()); // before the morning after
        std::strcat(c,s+1);
        if (std::islower(*c))
        {
            *c = std::toupper(*c);           // gack and double gack
        }
    }
    g_real_name = g_buf;
#else // !BERKNAMES
    c = std::strchr(s, '(');
    if (c != nullptr)
    {
        *c = '\0';
    }
    c = std::strchr(s, '-');
    if (c != nullptr)
    {
        s = c;
    }
    g_real_name = s;
#endif // !BERKNAMES
#endif
#ifndef PASSNAMES
    {
        env_init2(); // Make sure g_home_dir/g_dot_dir/etc. are set.
        std::FILE *fp = std::fopen(file_exp(FULLNAMEFILE), "r");
        if (fp != nullptr)
        {
            std::fgets(g_buf,sizeof g_buf,fp);
            std::fclose(fp);
            g_buf[std::strlen(g_buf)-1] = '\0';
            g_real_name = g_buf;
        }
    }
#ifdef WIN32
    if (g_login_name.empty())
    {
        DWORD size = 0;
        GetUserNameExA(NameSamCompatible, nullptr, &size);
        std::unique_ptr<char[]> buffer{new char[size]};
        GetUserNameExA(NameSamCompatible, buffer.get(), &size);
        std::string            value{buffer.get()};
        std::string::size_type backslash = value.find_last_of('\\');
        if (backslash != std::string::npos)
        {
            value = value.substr(backslash + 1);
        }
        g_login_name = value;
    }
    if (g_real_name.empty())
    {
        DWORD size = 0;
        if (!GetUserNameExA(NameDisplay, nullptr, &size))
        {
            if (GetLastError() == ERROR_MORE_DATA)
            {
                char *buffer = safe_malloc(size);
                GetUserNameExA(NameDisplay, buffer, &size);
                g_real_name = buffer;
                safe_free(buffer);
            }
        }
    }
    if (g_real_name.empty())
    {
        g_real_name = "?Unknown";
    }
#endif
#endif // !PASSNAMES
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
    std::FILE* fp;
    bool hostname_ok = true;

    // Find the local hostname

#ifdef HAS_GETHOSTNAME
#ifdef WIN32
    const WORD version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);
#endif
    gethostname(tmpbuf,TCBUF_SIZE);
#else
# ifdef HAS_UNAME
    // get sysname
    uname(&utsn);
    std::strcpy(tmpbuf,utsn.nodename);
# else
#  ifdef PHOSTCMD
    {
        std::FILE* pipefp = popen(PHOSTCMD,"r");

        if (pipefp == nullptr)
        {
            std::printf("Can't find hostname\n");
            finalize(1);
        }
        std::fgets(tmpbuf,TCBUF_SIZE,pipefp);
        tmpbuf[std::strlen(tmpbuf)-1] = '\0';        // wipe out newline
        pclose(pipefp);
    }
#  else
    std::strcpy(tmpbuf, "!INVALID!");
#  endif // PHOSTCMD
# endif // HAS_UNAME
#endif // HAS_GETHOSTNAME
    g_local_host = save_str(tmpbuf);

    // Build the host name that goes in postings

    const char *filename{PHOSTNAME};
    if (FILE_REF(filename) || filename[0] == '~')
    {
        fp = std::fopen(file_exp(filename), "r");
        if (fp == nullptr)
        {
            std::strcpy(tmpbuf, ".");
        }
        else
        {
            std::fgets(tmpbuf, TCBUF_SIZE, fp);
            std::fclose(fp);
            char *end = tmpbuf + std::strlen(tmpbuf) - 1;
            if (*end == '\n')
            {
                *end = '\0';
            }
        }
    }
    else
    {
        std::strcpy(tmpbuf, PHOSTNAME);
    }

    if (tmpbuf[0] == '.')
    {
        if (tmpbuf[1] != '\0')
        {
            std::strcpy(g_buf,tmpbuf);
        }
        else
        {
            g_buf[0] = '\0';
        }
        std::strcpy(tmpbuf,g_local_host);
        std::strcat(tmpbuf,g_buf);
    }

    if (!std::strchr(tmpbuf, '.'))
    {
        if (tmpbuf[0])
        {
            std::strcat(tmpbuf, ".");
        }
#ifdef HAS_RES_INIT
        if (!(_res.options & RES_INIT))
        {
            res_init();
        }
        if (_res.defdname != nullptr)
        {
            std::strcat(tmpbuf,_res.defdname);
        }
        else
#endif
#ifdef HAS_GETDOMAINNAME
        if (getdomainname(g_buf,LBUFLEN) == 0)
        {
            std::strcat(tmpbuf,g_buf);
        }
        else
#endif
        {
            std::strcat(tmpbuf,"UNKNOWN.HOST");
            hostname_ok = false;
        }
    }
    g_p_host_name = tmpbuf;
    return hostname_ok;
}

char *get_val(const char *nam, char *def)
{
    char *val = s_getenv_fn(nam);
    if (val == nullptr || !*val)
    {
        return def;
    }
    return val;
}

const char *get_val_const(const char *nam, const char *def)
{
    const char *val = s_getenv_fn(nam);
    return val == nullptr || !*val ? def : val;
}

static bool s_first_export = true;

#ifndef WIN32
extern char **environ;
#endif

char *export_var(const char *nam, const char *val)
{
#if 1
    std::string envar{nam};
    envar += '=';
    envar += val;
    char *buff = save_str(envar.c_str());
    putenv(buff);
    return buff;
#else
    int namlen = std::strlen(nam);
    int i=envix(nam,namlen);    // where does it go?

    if (!environ[i])                    // does not exist yet
    {
        if (s_first_export)              // need we copy environment?
        {
#ifndef lint
            char** tmpenv = (char**)    // point our wand at memory
                safe_malloc((MemorySize) (i+2) * sizeof(char*));
#else
            char** tmpenv = nullptr;
#endif // lint

            s_first_export = false;
            for (int j = 0; j < i; j++) // copy environment
            {
                tmpenv[j] = environ[j];
            }
            environ = tmpenv;           // tell exec where it is now
        }
#ifndef lint
        else
        {
            environ = (char**) safe_realloc((char*) environ,
                (MemorySize) (i+2) * sizeof(char*));
                                        // just expand it a bit
        }
#endif // lint
        environ[i+1] = nullptr; // make sure it's null terminated
    }
    environ[i] = safe_malloc((MemorySize)(namlen + std::strlen(val) + 2));
                                        // this may or may not be in
                                        // the old environ structure
    std::sprintf(environ[i],"%s=%s",nam,val);// all that work just for this
    return environ[i] + namlen + 1;
#endif
}

void un_export(char *export_val)
{
    if (export_val[-1] == '=' && export_val[-2] != '_')
    {
        export_val[0] = export_val[-2];
        export_val[1] = '\0';
        export_val[-2] = '_';
    }
}

void re_export(char *export_val, const char *new_val, int limit)
{
    if (export_val[-1] == '=' && export_val[-2] == '_' && !export_val[1])
    {
        export_val[-2] = export_val[0];
    }
    safe_copy(export_val, new_val, limit+1);
}

static int envix(const char *nam, int len)
{
    int i;

    for (i = 0; environ[i]; i++)
    {
        if (!std::strncmp(environ[i], nam, len) && environ[i][len] == '=')
        {
            break;                      // strncmp must come first to avoid
        }
    }                                   // potential SEGV's
    return i;
}
