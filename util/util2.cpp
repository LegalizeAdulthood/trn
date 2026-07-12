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
    std::string scrbuf(CMD_BUF_LEN, '\0');

    // interpret any % escapes
    do_interp(filename.data(), static_cast<int>(filename.size()), s, nullptr, nullptr);
    s = filename.data();
    if (*s == '~')      // does destination start with ~?
    {
        if (!*(++s) || *s == '/')
        {
            std::sprintf(scrbuf.data(), "%s%s", g_home_dir.c_str(), s);
            // swap $HOME for it
            std::strcpy(filename.data(), scrbuf.c_str());
        }
        else if (*s == '~' && (!s[1] || s[1] == '/'))
        {
            const char *prefix = get_val_const("TRNPREFIX");
            if (!prefix)
            {
                prefix = INSTALL_PREFIX;
            }
            std::sprintf(scrbuf.data(), "%s%s", prefix, s + 1);
        }
        else
        {
#ifdef TILDE_NAME
            {
                char *d = scrbuf.data();
                while (isalnum(*s))
                {
                    *d++ = *s++;
                }
                *d = '\0';
            }
            if (!s_tilde_dir.empty() && s_tilde_name == scrbuf.c_str())
            {
                std::strcpy(scrbuf.data(), s_tilde_dir.c_str());
                std::strcat(scrbuf.data(), s);
                std::strcpy(filename.data(), scrbuf.c_str());
            }
            else
            {
                s_tilde_dir.clear();
                s_tilde_name = scrbuf.c_str();
#ifdef HAS_GETPWENT     // getpwnam() is not the paragon of efficiency
                {
                    struct passwd *pwd = getpwnam(s_tilde_name.c_str());
                    if (pwd == nullptr)
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name.c_str());
                        return {};
                    }
                    std::sprintf(scrbuf.data(), "%s%s", pwd->pw_dir, s);
                    s_tilde_dir = pwd->pw_dir;
                    std::strcpy(filename.data(), scrbuf.c_str());
                    endpwent();
                }
#else                   // this will run faster, and is less D space
                { // just be sure LOGIN_DIR_FIELD is correct
                    std::FILE *pfp = std::fopen(file_exp(PASSWORD_FILE).c_str(), "r");
                    char tmpbuf[512];

                    if (pfp)
                    {
                        while (std::fgets(tmpbuf, 512, pfp) != nullptr)
                        {
                            const char *d = copy_till(scrbuf.data(), tmpbuf, ':');
                            if (!std::strcmp(scrbuf.c_str(), s_tilde_name.c_str()))
                            {
                                for (int i = LOGIN_DIR_FIELD - 2; i; i--)
                                {
                                    if (d)
                                    {
                                        d = std::strchr(d + 1, ':');
                                    }
                                }
                                if (d)
                                {
                                    copy_till(scrbuf.data(), d + 1, ':');
                                    s_tilde_dir = scrbuf.c_str();
                                    std::strcat(scrbuf.data(), s);
                                    std::strcpy(filename.data(), scrbuf.c_str());
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
        char *d = scrbuf.data();
        *d++ = '%';
        if (s[1] == '{')
        {
            std::strcpy(d, s + 2);
        }
        else
        {
            *d++ = '{';
            for (s++; std::isalnum(*s); s++)
            {
                *d++ = *s;
            }
            // skip over token
            *d++ = '}';
            std::strcpy(d, s);
        }
        // this might do some extra '%'s, but that's how the Mercedes Benz
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
