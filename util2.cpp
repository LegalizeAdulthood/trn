/* util2.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "util2.h"

#include "env.h"
#include "util.h"
#include "util3.h"

#ifdef MSDOS
#include <direct.h>
#endif

#ifdef TILDENAME
static char *s_tildename{};
static char *s_tildedir{};
#endif

/* copy a string to a safe spot */

char *savestr(const char *str)
{
    char* newaddr = safemalloc((MEM_SIZE)(strlen(str)+1));

    strcpy(newaddr,str);
    return newaddr;
}

/* safe version of string copy */
char *safecpy(char *to, const char *from, int len)
{
    char* dest = to;

    if (from) {
	while (--len && *from)
	    *dest++ = *from++;
    }
    *dest = '\0';

    return to;
}

/* copy a string up to some (non-backslashed) delimiter, if any */
char *cpytill(char *to, char *from, int delim)
{
    while (*from) {
	if (*from == '\\' && from[1] == delim)
	    from++;
	else if (*from == delim)
	    break;
	*to++ = *from++;
    }
    *to = '\0';
    return from;
}

/* expand filename via %, ~, and $ interpretation */
/* returns pointer to static area */
/* Note that there is a 1-deep cache of ~name interpretation */

char *filexp(const char *text)
{
    // sbuf exists so that we can have a const input
    static char sbuf[CBUFLEN];
    strcpy(sbuf, text);
    char *s = sbuf;
    static char filename[CBUFLEN];
    char scrbuf[CBUFLEN];
    char* d;

    /* interpret any % escapes */
    dointerp(filename,sizeof filename,s,nullptr,nullptr);
    s = filename;
    if (*s == '~') {	/* does destination start with ~? */
        if (!*(++s) || *s == '/')
        {
            sprintf(scrbuf, "%s%s", g_home_dir, s);
            /* swap $HOME for it */
            strcpy(filename, scrbuf);
        }
        else if (*s == '~' && (!s[1] || s[1] == '/'))
        {
            d = getenv("TRNPREFIX");
            if (!d)
                d = INSTALLPREFIX;
            sprintf(scrbuf, "%s%s", d, s + 1);
        }
        else
        {
#ifdef TILDENAME
            for (d = scrbuf; isalnum(*s); s++, d++)
                *d = *s;
            *d = '\0';
            if (s_tildedir && !strcmp(s_tildename, scrbuf))
            {
                strcpy(scrbuf, s_tildedir);
                strcat(scrbuf, s);
                strcpy(filename, scrbuf);
            }
            else
            {
                if (s_tildename)
                    free(s_tildename);
                if (s_tildedir)
                    free(s_tildedir);
                s_tildedir = nullptr;
                s_tildename = savestr(scrbuf);
#ifdef HAS_GETPWENT	/* getpwnam() is not the paragon of efficiency */
                {
                    struct passwd *pwd = getpwnam(s_tildename);
                    if (pwd == nullptr)
                    {
                        printf("%s is an unknown user. Using default.\n", s_tildename) FLUSH;
                        return nullptr;
                    }
                    sprintf(scrbuf, "%s%s", pwd->pw_dir, s);
                    s_tildedir = savestr(pwd->pw_dir);
                    strcpy(filename, scrbuf);
                    endpwent();
                }
#else			/* this will run faster, and is less D space */
                { /* just be sure LOGDIRFIELD is correct */
                    FILE *pfp = fopen(filexp(PASSFILE), "r");
                    char tmpbuf[512];

                    if (pfp)
                    {
                        while (fgets(tmpbuf, 512, pfp) != nullptr)
                        {
                            d = cpytill(scrbuf, tmpbuf, ':');
                            if (!strcmp(scrbuf, s_tildename))
                            {
                                for (int i = LOGDIRFIELD - 2; i; i--)
                                {
                                    if (d)
                                        d = strchr(d + 1, ':');
                                }
                                if (d)
                                {
                                    cpytill(scrbuf, d + 1, ':');
                                    s_tildedir = savestr(scrbuf);
                                    strcat(scrbuf, s);
                                    strcpy(filename, scrbuf);
                                }
                                break;
                            }
                        }
                        fclose(pfp);
                    }
                    if (!s_tildedir)
                    {
                        printf("%s is an unknown user. Using default.\n", s_tildename) FLUSH;
                        return nullptr;
                    }
                }
#endif
            }
#else /* !TILDENAME */
            if (g_verbose)
                fputs("~loginname not implemented.\n", stdout) FLUSH;
            else
                fputs("~login not impl.\n", stdout) FLUSH;
#endif
        }
    }
    else if (*s == '$')
    { /* starts with some env variable? */
        d = scrbuf;
        *d++ = '%';
        if (s[1] == '{')
            strcpy(d, s + 2);
        else
        {
            *d++ = '{';
            for (s++; isalnum(*s); s++)
                *d++ = *s;
            /* skip over token */
            *d++ = '}';
            strcpy(d, s);
        }
        /* this might do some extra '%'s, but that's how the Mercedes Benz */
        dointerp(filename, sizeof filename, scrbuf, nullptr, nullptr);
    }
    return filename;
}

/* return ptr to little string in big string, nullptr if not found */

char *in_string(char *big, const char *little, bool case_matters)
{
    const char* s;
    const char* x;

    for (char *t = big; *t; t++) {
	for (x=t,s=little; *s; x++,s++) {
	    if (!*x)
		return nullptr;
	    if (case_matters == true) {
		if (*s != *x)
		    break;
	    } else {
		char c,d;
		if (isupper(*s)) 
		    c = tolower(*s);
		else
		    c = *s;
		if (isupper(*x)) 
		    d = tolower(*x);
		else
		    d = *x;
		if ( c != d )
		    break;
	   }
	}
	if (!*s)
	    return t;
    }
    return nullptr;
}

#ifndef HAS_STRCASECMP
static Uchar casemap[256] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
    0x78,0x79,0x7A,0x7B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
    0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
    0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,
    0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,
    0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,
    0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
    0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};
#endif

#ifndef HAS_STRCASECMP
int strcasecmp(const char *s1, const char *s2)
{
    do {
	if (casemap[(Uchar)*s1++] != casemap[(Uchar)*s2])
	    return casemap[(Uchar)s1[-1]] - casemap[(Uchar)*s2];
    } while (*s2++ != '\0');
    return 0;
}
#endif

#ifndef HAS_STRCASECMP
int strncasecmp(const char *s1, const char *s2, int len)
{
    while (len--) {
	if (casemap[(Uchar)*s1++] != casemap[(Uchar)*s2])
	    return casemap[(Uchar)s1[-1]] - casemap[(Uchar)*s2];
	if (*s2++ == '\0')
	    break;
    }
    return 0;
}
#endif

char *read_auth_file(const char *file, char **pass_ptr)
{
    char* strptr[2];
    char buf[1024];
    strptr[1] = nullptr;
    strptr[0] = nullptr;
    FILE *fp = fopen(file, "r");
    if (fp != nullptr) {
        for (int i = 0; i < 2; i++) {
	    if (fgets(buf, sizeof buf, fp) != nullptr) {
		char* cp = buf + strlen(buf) - 1;
		if (*cp == '\n')
		    *cp = '\0';
		strptr[i] = savestr(buf);
	    }
	}
	fclose(fp);
    }
    *pass_ptr = strptr[1];
    return strptr[0];
}

#ifdef MSDOS
int getuid()
{
    return 2;
}
#endif
