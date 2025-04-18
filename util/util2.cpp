/* util2.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "util/util2.h"

#include <config/common.h>
#include <tool/util3.h>
#include <trn/util.h>
#include <util/env.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef TILDE_NAME
static char *s_tilde_name{};
static char *s_tilde_dir{};
#endif

/* copy a string to a safe spot */

char *save_str(const char *str)
{
    char* newaddr = safe_malloc((MemorySize)(std::strlen(str)+1));

    std::strcpy(newaddr,str);
    return newaddr;
}

/* safe version of string copy */
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

/* copy a string up to some (non-backslashed) delimiter, if any */
char *copy_till(char *to, char *from, int delim)
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

/* expand filename via %, ~, and $ interpretation */
/* returns pointer to static area */
/* Note that there is a 1-deep cache of ~name interpretation */

char *file_exp(const char *text)
{
    // sbuf exists so that we can have a const input
    static char sbuf[CMD_BUF_LEN];
    std::strcpy(sbuf, text);
    char *s = sbuf;
    static char filename[CMD_BUF_LEN];
    char scrbuf[CMD_BUF_LEN];

    /* interpret any % escapes */
    do_interp(filename,sizeof filename,s,nullptr,nullptr);
    s = filename;
    if (*s == '~')      /* does destination start with ~? */
    {
        if (!*(++s) || *s == '/')
        {
            std::sprintf(scrbuf, "%s%s", g_home_dir, s);
            /* swap $HOME for it */
            std::strcpy(filename, scrbuf);
        }
        else if (*s == '~' && (!s[1] || s[1] == '/'))
        {
            const char *prefix = get_val_const("TRNPREFIX");
            if (!prefix)
            {
                prefix = INSTALLPREFIX;
            }
            std::sprintf(scrbuf, "%s%s", prefix, s + 1);
        }
        else
        {
#ifdef TILDE_NAME
            {
                char *d = scrbuf;
                while (isalnum(*s))
                {
                    *d++ = *s++;
                }
                *d = '\0';
            }
            if (s_tilde_dir && !strcmp(s_tilde_name, scrbuf))
            {
                std::strcpy(scrbuf, s_tilde_dir);
                std::strcat(scrbuf, s);
                std::strcpy(filename, scrbuf);
            }
            else
            {
                if (s_tilde_name)
                {
                    std::free(s_tilde_name);
                }
                if (s_tilde_dir)
                {
                    std::free(s_tilde_dir);
                }
                s_tilde_dir = nullptr;
                s_tilde_name = save_str(scrbuf);
#ifdef HAS_GETPWENT     /* getpwnam() is not the paragon of efficiency */
                {
                    struct passwd *pwd = getpwnam(s_tilde_name);
                    if (pwd == nullptr)
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name);
                        return nullptr;
                    }
                    std::sprintf(scrbuf, "%s%s", pwd->pw_dir, s);
                    s_tilde_dir = save_str(pwd->pw_dir);
                    std::strcpy(filename, scrbuf);
                    endpwent();
                }
#else                   /* this will run faster, and is less D space */
                { /* just be sure LOGDIRFIELD is correct */
                    std::FILE *pfp = std::fopen(file_exp(PASSFILE), "r");
                    char tmpbuf[512];

                    if (pfp)
                    {
                        while (std::fgets(tmpbuf, 512, pfp) != nullptr)
                        {
                            char *d = copy_till(scrbuf, tmpbuf, ':');
                            if (!std::strcmp(scrbuf, s_tilde_name))
                            {
                                for (int i = LOGDIRFIELD - 2; i; i--)
                                {
                                    if (d)
                                    {
                                        d = std::strchr(d + 1, ':');
                                    }
                                }
                                if (d)
                                {
                                    copy_till(scrbuf, d + 1, ':');
                                    s_tilde_dir = save_str(scrbuf);
                                    std::strcat(scrbuf, s);
                                    std::strcpy(filename, scrbuf);
                                }
                                break;
                            }
                        }
                        std::fclose(pfp);
                    }
                    if (!s_tilde_dir)
                    {
                        std::printf("%s is an unknown user. Using default.\n", s_tilde_name);
                        return nullptr;
                    }
                }
#endif
            }
#else /* !TILDENAME */
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
    { /* starts with some env variable? */
        char *d = scrbuf;
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
            /* skip over token */
            *d++ = '}';
            std::strcpy(d, s);
        }
        /* this might do some extra '%'s, but that's how the Mercedes Benz */
        do_interp(filename, sizeof filename, scrbuf, nullptr, nullptr);
    }
    return filename;
}

/* return ptr to little string in big string, nullptr if not found */

char *in_string(char *big, const char *little, bool case_matters)
{
    for (char *t = big; *t; t++)
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
                if ( c != d )
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
