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

#ifdef TILDE_NAME
static std::string s_tilde_name;
static std::string s_tilde_dir;
#endif

// copy a string to a safe spot

char *save_str(std::string_view str)
{
    char *newaddr = safe_malloc(static_cast<MemorySize>(str.size() + 1));

    if (!str.empty())
    {
        std::memcpy(newaddr, str.data(), str.size());
    }
    newaddr[str.size()] = '\0';
    return newaddr;
}

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
                    std::FILE *pfp = std::fopen(file_exp(PASSWORD_FILE).c_str(), "r");
                    char       tmpbuf[512];

                    if (pfp)
                    {
                        while (std::fgets(tmpbuf, 512, pfp) != nullptr)
                        {
                            std::string_view  passwd_line{tmpbuf};
                            const std::size_t login_end = passwd_line.find(':');
                            if (login_end != std::string_view::npos && passwd_line.substr(0, login_end) == s_tilde_name)
                            {
                                std::size_t field_start = login_end + 1;
                                std::size_t field_end = std::string_view::npos;
                                for (int i = LOGIN_DIR_FIELD - 2; i; i--)
                                {
                                    field_end = passwd_line.find(':', field_start);
                                    if (field_end != std::string_view::npos)
                                    {
                                        field_start = field_end + 1;
                                    }
                                }
                                if (field_end != std::string_view::npos)
                                {
                                    field_end = passwd_line.find(':', field_start);
                                    s_tilde_dir = passwd_line.substr(field_start, field_end - field_start);
                                    filename = s_tilde_dir;
                                    filename += suffix;
                                }
                                break;
                            }
                        }
                        std::fclose(pfp);
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

char *read_auth_file(const char *file, char **pass_ptr)
{
    char* strptr[2];
    char buf[1024];
    strptr[1] = nullptr;
    strptr[0] = nullptr;
    std::FILE *fp = std::fopen(file, "r");
    if (fp != nullptr)
    {
        for (int i = 0; i < 2; i++)
        {
            if (std::fgets(buf, sizeof buf, fp) != nullptr)
            {
                char* cp = buf + std::strlen(buf) - 1;
                if (*cp == '\n')
                {
                    *cp = '\0';
                }
                strptr[i] = save_str(buf);
            }
        }
        std::fclose(fp);
    }
    *pass_ptr = strptr[1];
    return strptr[0];
}
