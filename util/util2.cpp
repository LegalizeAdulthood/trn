/* util2.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <util/util2.h>

#include <config/common.h>
#include <tool/util3.h>
#include <trn/util.h>
#include <util/env.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#ifdef TILDE_NAME
static std::string s_tilde_name;
static std::string s_tilde_dir;
#endif

// safe version of string copy
char *safe_copy(char *to, const char *from, int len)
{
    char* dest = to;

    if (from)
    {
        while (--len && *from)
        {
            *dest++ = *from++;
        }
    }
    *dest = '\0';

    return to;
}

// copy a string up to some (non-backslashed) delimiter, if any
const char *copy_till(char *to, const char *from, int delim)
{
    while (*from)
    {
        if (*from == '\\' && from[1] == delim)
        {
            from++;
        }
        else if (*from == delim)
        {
            break;
        }
        *to++ = *from++;
    }
    *to = '\0';
    return from;
}

// expand filename via %, ~, and $ interpretation
// Note that there is a 1-deep cache of ~name interpretation

std::string file_exp(std::string_view text)
{
    std::string sbuf{text};
    char       *s = sbuf.data();
    std::string filename(CMD_BUF_LEN, '\0');

    // interpret any % escapes
    do_interp(filename.data(), static_cast<int>(filename.size()), s, nullptr, nullptr);
    s = filename.data();
    if (*s == '~') // does destination start with ~?
    {
        if (!*(++s) || *s == '/')
        {
            const std::string suffix{s};
            filename = g_home_dir;
            filename += suffix;
        }
        else if (*s == '~' && (!s[1] || s[1] == '/'))
        {
            (void) get_val_const("TRNPREFIX", INSTALL_PREFIX);
        }
        else
        {
#ifdef TILDE_NAME
            const char *login_start = s;
            while (std::isalnum(*s))
            {
                ++s;
            }
            std::string login_name{login_start, static_cast<std::size_t>(s - login_start)};
            std::string suffix{s};
            if (!s_tilde_dir.empty() && s_tilde_name == login_name)
            {
                filename = s_tilde_dir;
                filename += suffix;
            }
            else
            {
                s_tilde_dir.clear();
                s_tilde_name = login_name;
#ifdef HAS_GETPWENT // getpwnam() is not the paragon of efficiency
                {
                    struct passwd *pwd = getpwnam(s_tilde_name.c_str());
                    if (pwd == nullptr)
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name.c_str());
                        return {};
                    }
                    s_tilde_dir = pwd->pw_dir;
                    filename = s_tilde_dir;
                    filename += suffix;
                    endpwent();
                }
#else // this will run faster, and is less D space
                { // just be sure LOGIN_DIR_FIELD is correct
                    if (std::ifstream pfp{file_exp(PASSWORD_FILE)})
                    {
                        std::string buff;
                        while (std::getline(pfp, buff))
                        {
                            const std::string_view line{buff};
                            if (const std::size_t sep = line.find(':');
                                sep != std::string_view::npos && line.substr(0, sep) == s_tilde_name)
                            {
                                std::size_t begin = sep + 1;
                                std::size_t end = std::string_view::npos;
                                for (int i = LOGIN_DIR_FIELD - 2; i; i--)
                                {
                                    end = line.find(':', begin);
                                    if (end != std::string_view::npos)
                                    {
                                        begin = end + 1;
                                    }
                                }
                                if (end != std::string_view::npos)
                                {
                                    end = line.find(':', begin);
                                    s_tilde_dir = line.substr(begin, end - begin);
                                    filename = s_tilde_dir;
                                    filename += suffix;
                                }
                                break;
                            }
                        }
                    }
                    if (s_tilde_dir.empty())
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name.c_str());
                        return {};
                    }
                }
#endif
            }
#else // !TILDENAME
            if (g_verbose)
            {
                std::fputs("~loginname not implemented.\n", stdout);
            }
            else
            {
                std::fputs("~login not impl.\n", stdout);
            }
#endif
        }
    }
    else if (*s == '$')
    { // starts with some env variable?
        std::string scrbuf{"%"};
        if (s[1] == '{')
        {
            scrbuf += s + 2;
        }
        else
        {
            scrbuf += '{';
            for (s++; std::isalnum(*s); s++)
            {
                scrbuf += *s;
            }
            // skip over token
            scrbuf += '}';
            scrbuf += s;
        }
        // this might do some extra '%'s, but that's how the Mercedes Benz
        filename.assign(CMD_BUF_LEN, '\0');
        do_interp(filename.data(), static_cast<int>(filename.size()), scrbuf.c_str(), nullptr, nullptr);
    }
    return filename.c_str();
}

// return ptr to little string in big string, nullptr if not found

const char *in_string(const char *big, const char *little, bool case_matters)
{
    for (const char *t = big; *t; t++)
    {
        const char *s = little;
        for (const char *x = t; *s; x++, s++)
        {
            if (!*x)
            {
                return nullptr;
            }
            if (case_matters)
            {
                if (*s != *x)
                {
                    break;
                }
            }
            else
            {
                char c;
                char d;
                if (std::isupper(*s))
                {
                    c = std::tolower(*s);
                }
                else
                {
                    c = *s;
                }
                if (std::isupper(*x))
                {
                    d = std::tolower(*x);
                }
                else
                {
                    d = *x;
                }
                if (c != d)
                {
                    break;
                }
            }
        }
        if (!*s)
        {
            return t;
        }
    }
    return nullptr;
}

char *in_string(char *big, const char *little, bool case_matters)
{
    return const_cast<char *>(in_string(static_cast<const char *>(big), little, case_matters));
}

AuthCredentials read_auth_file(const char *file)
{
    if (file == nullptr || *file == '\0')
    {
        return {};
    }

    AuthCredentials result;
    std::ifstream fp{file};
    if (fp)
    {
        std::getline(fp, result.user);
        std::getline(fp, result.password);
    }
    return result;
}
