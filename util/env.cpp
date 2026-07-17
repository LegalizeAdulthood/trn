/* env.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <util/env-internal.h>

#include <config/common.h>
#include <config/pipe_io.h>
#include <trn/init.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

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
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>

std::string g_home_dir;      // login directory
std::string g_dot_dir;       // where . files go
std::string g_trn_dir;       // usually %./.trn
std::string g_lib;           // news library
std::string g_rn_lib;        // private news program library
std::string g_tmp_dir;       // where tmp files go
std::string g_login_name;    // login name of user
std::string g_real_name;     // real name of user
std::string g_p_host_name;   // host name in a posting
std::string g_local_host;    // local host name
int         g_net_speed{20}; // how fast our net-connection is

static std::function<char *(const char *name)> s_getenv_fn = std::getenv;

static void env_init2();
static bool set_user_name();
static bool set_p_host_name();

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

bool env_init(bool lax, const std::function<bool()> &set_user_name_fn, const std::function<bool()> &set_host_name_fn)
{
    bool fully_successful = true;

    const char *home_dir = s_getenv_fn("HOME");
    if (home_dir == nullptr)
    {
        home_dir = s_getenv_fn("LOGDIR");
    }
    if (home_dir)
    {
        g_home_dir = home_dir;
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
    if (g_home_dir.empty())
    {
        const char *home_drive = s_getenv_fn("HOMEDRIVE");
        const char *home_path = s_getenv_fn("HOMEPATH");
        if (home_drive != nullptr && home_path != nullptr)
        {
            g_home_dir = std::string{home_drive} + home_path;
        }
    }
#endif

    // Set g_real_name, and maybe set g_login_name and g_home_dir.
    if (!set_user_name_fn())
    {
        g_login_name.clear();
        g_real_name.clear();
        fully_successful = false;
    }
    env_init2();

    // set g_p_host_name to the hostname of our local machine
    if (!set_host_name_fn())
    {
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

bool env_init(bool lax)
{
    return env_init(lax, set_user_name, set_p_host_name);
}

void env_final()
{
    g_p_host_name.clear();
    g_local_host.clear();
    g_real_name.clear();
    g_login_name.clear();
    g_home_dir.clear();
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

    if (g_home_dir.empty())
    {
        g_home_dir = "/";
    }
    g_dot_dir = get_val_const("DOTDIR", g_home_dir.c_str());
    g_trn_dir = file_exp(get_val_const("TRNDIR",TRNDIR));
    g_lib = file_exp(NEWS_LIB);
    g_rn_lib = file_exp(PRIVATE_LIB);
}

// Set g_login_name to the user's login name and g_real_name to the user's
// real name.
//
static bool set_user_name()
{
    const char *s{};

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
    if (g_home_dir.empty())
    {
        g_home_dir = pwd->pw_dir;
    }
    s = pwd->pw_gecos;
#endif
#ifdef PASS_NAMES
#ifdef BERKELEY_NAMES
#ifdef BERKJUNK
    std::string_view gecos{s};
    while (!gecos.empty() && !isalnum(gecos.front()) && gecos.front() != '&')
    {
        gecos.remove_prefix(1);
    }
#else
    std::string_view gecos{s};
#endif
    const std::string_view::size_type delimiter = gecos.find_first_of(",;");
    if (delimiter != std::string_view::npos)
    {
        gecos = gecos.substr(0, delimiter);
    }
    g_real_name.clear();
    std::string_view::size_type index = 0;
    while (index < gecos.size())
    {
        if (gecos[index] == '\\' && index + 1 < gecos.size() && gecos[index + 1] == '&')
        {
            index++;
        }
        else if (gecos[index] == '&')
        {
            break;
        }
        g_real_name += gecos[index++];
    }
    if (index < gecos.size() && gecos[index] == '&') // whoever thought this one up was
    {
        const std::size_t login_pos = g_real_name.size(); // in the middle of the night
        g_real_name += g_login_name;                      // before the morning after
        g_real_name.append(gecos.substr(index + 1));
        if (login_pos < g_real_name.size() && std::islower(static_cast<unsigned char>(g_real_name[login_pos])))
        {
            g_real_name[login_pos] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(g_real_name[login_pos]))); // gack and double gack
        }
    }
#else // !BERKELEY_NAMES
    std::string_view gecos{s};
    if (const std::string_view::size_type paren = gecos.find('('); paren != std::string_view::npos)
    {
        gecos = gecos.substr(0, paren);
    }
    if (const std::string_view::size_type hyphen = gecos.find('-'); hyphen != std::string_view::npos)
    {
        gecos.remove_prefix(hyphen);
    }
    g_real_name = std::string{gecos};
#endif // !BERKELEY_NAMES
#endif
#ifndef PASS_NAMES
    {
        env_init2(); // Make sure g_home_dir/g_dot_dir/etc. are set.
        std::ifstream input{file_exp(FULLNAMEFILE)};
        if (input)
        {
            std::getline(input, g_real_name);
        }
    }
#ifdef WIN32
    if (g_login_name.empty())
    {
        DWORD size = 0;
        if (!GetUserNameExA(NameSamCompatible, nullptr, &size) && GetLastError() == ERROR_MORE_DATA)
        {
            std::string value(size, '\0');
            if (GetUserNameExA(NameSamCompatible, value.data(), &size))
            {
                const std::string::size_type backslash = value.find_last_of('\\');
                if (backslash != std::string::npos)
                {
                    value = value.substr(backslash + 1);
                }
                g_login_name = value.c_str();
            }
        }
    }
    if (g_real_name.empty())
    {
        DWORD size = 0;
        if (!GetUserNameExA(NameDisplay, nullptr, &size))
        {
            if (GetLastError() == ERROR_MORE_DATA)
            {
                std::string buffer(size, '\0');
                if (GetUserNameExA(NameDisplay, buffer.data(), &size))
                {
                    g_real_name = buffer.c_str();
                }
            }
        }
    }
    if (g_real_name.empty())
    {
        g_real_name = "?Unknown";
    }
#endif
#endif // !PASS_NAMES
#ifdef HAS_GETPWENT
    endpwent();
#endif
    if (g_real_name.empty())
    {
        g_real_name = "PUT_YOUR_NAME_HERE";
    }
    return true;
}

static bool set_p_host_name()
{
    bool        hostname_ok = true;
    std::string local_host_name;

    // Find the local hostname

#ifdef HAS_GETHOSTNAME
#ifdef WIN32
    const WORD version = MAKEWORD(2, 2);
    WSADATA    data;
    WSAStartup(version, &data);
#endif
    std::array<char, TCBUF_SIZE + 1> host_buffer{};
    gethostname(host_buffer.data(), TCBUF_SIZE);
    local_host_name = host_buffer.data();
#else
# ifdef HAS_UNAME
    // get sysname
    uname(&utsn);
    local_host_name = utsn.nodename;
# else
#  ifdef PIPE_HOST_CMD
    {
        std::FILE *pipefp = popen(PIPE_HOST_CMD, "r");

        if (pipefp == nullptr)
        {
            fmt::print("Can't find hostname\n");
            finalize(1);
        }
        std::array<char, TCBUF_SIZE + 1> host_buffer{};
        std::fgets(host_buffer.data(), TCBUF_SIZE, pipefp);
        local_host_name = host_buffer.data();
        if (!local_host_name.empty() && local_host_name.back() == '\n')
        {
            local_host_name.pop_back();
        }
        pclose(pipefp);
    }
#  else
    local_host_name = "!INVALID!";
#  endif // PIPE_HOST_CMD
# endif // HAS_UNAME
#endif // HAS_GETHOSTNAME
    g_local_host = local_host_name;

    // Build the host name that goes in postings

    std::string posting_host_name;
    const char *filename{POSTING_HOSTNAME};
    if (FILE_REF(filename) || filename[0] == '~')
    {
        std::ifstream input{file_exp(filename)};
        if (!input)
        {
            posting_host_name = ".";
        }
        else
        {
            posting_host_name = g_local_host;
            std::string posting_host_buffer;
            posting_host_buffer.reserve(TCBUF_SIZE);
            if (std::getline(input, posting_host_buffer))
            {
                posting_host_name = posting_host_buffer;
            }
        }
    }
    else
    {
        posting_host_name = POSTING_HOSTNAME;
    }

    if (!posting_host_name.empty() && posting_host_name.front() == '.')
    {
        posting_host_name = g_local_host + (posting_host_name.size() > 1 ? posting_host_name : "");
    }

    if (posting_host_name.find('.') == std::string::npos)
    {
        if (!posting_host_name.empty())
        {
            posting_host_name += ".";
        }
#ifdef HAS_RES_INIT
        if (!(_res.options & RES_INIT))
        {
            res_init();
        }
        if (_res.defdname != nullptr)
        {
            posting_host_name += _res.defdname;
        }
        else
#endif
#ifdef HAS_GETDOMAINNAME
        {
            std::array<char, LINE_BUF_LEN + 1> domain_name{};
            if (getdomainname(domain_name.data(), LINE_BUF_LEN) == 0)
            {
                posting_host_name += domain_name.data();
            }
            else
#endif
            {
                posting_host_name += "UNKNOWN.HOST";
                hostname_ok = false;
            }
#ifdef HAS_GETDOMAINNAME
        }
#endif
    }
    g_p_host_name = posting_host_name;
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
