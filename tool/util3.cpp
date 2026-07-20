/* util3.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <tool/util3.h>

#include <config/env.h>
#include <nntp/nntpclient.h>
#include <util/env.h>
#include <util/util2.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

static std::string    s_nntp_password;

int do_shell(const char *shell, const char *cmd)
{
    return std::system(cmd);
}

[[noreturn]] void finalize(int num)
{
    nntp_close(true);
    std::exit(num);
}

std::string do_interp(std::string_view pattern)
{
    if (pattern.size() >= 2 && pattern[0] == '%' && pattern[1] == '.')
    {
        std::string result{g_dot_dir};
        result.append(pattern.substr(2));
        return result;
    }
    if (pattern.size() >= 3 && pattern[0] == '%' && pattern[1] == '{')
    {
        const std::string_view::size_type close = pattern.find('}', 2);
        if (close != std::string_view::npos)
        {
            std::string result = get_env_var(pattern.substr(2, close - 2));
            result.append(pattern.substr(close + 1));
            return result;
        }
    }
    return std::string{pattern};
}

int nntp_handle_nested_lists()
{
    std::fputs("Programming error! Nested NNTP calls detected.\n",stderr);
    return -1;
}

std::string get_auth_user()
{
    extern std::string    g_nntp_auth_file;
    const AuthCredentials credentials = read_auth_file(g_nntp_auth_file);

    s_nntp_password = credentials.password;
    return credentials.user;
}

std::string get_auth_pass()
{
    return s_nntp_password;
}
