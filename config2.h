/* config2.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#ifdef HAS_GETPWENT
#   include <pwd.h>
#endif

#ifdef I_UNISTD
#   include <unistd.h>
#endif

#include <stdlib.h>

#ifdef USE_DEBUGGING_MALLOC
#   include "malloc.h"
#   define safemalloc malloc
#   define saferealloc realloc
#endif

#include <string.h>

#ifndef S_ISDIR
#   define S_ISDIR(m)  ( ((m) & S_IFMT) == S_IFDIR )
#endif
#ifndef S_ISCHR
#   define S_ISCHR(m)  ( ((m) & S_IFMT) == S_IFCHR )
#endif
#ifndef S_ISREG
#   define S_ISREG(m)  ( ((m) & S_IFMT) == S_IFREG )
#endif
#ifndef isalnum
#   define isalnum(c) (isalpha(c) || isdigit(c))
#endif

#ifdef MSDOS
#include "msdos.h"
#endif

/* some handy defs */

using char_int = int;

#define Ctl(ch) (ch & 037)

#define strNE(s1,s2) (strcmp(s1,s2))
#define strEQ(s1,s2) (!strcmp(s1,s2))
#define strnNE(s1,s2,l) (strncmp(s1,s2,l))
#define strnEQ(s1,s2,l) (!strncmp(s1,s2,l))

#ifdef HAS_STRCASECMP
#define strcaseCMP(s1,s2) strcasecmp(s1,s2)
#define strcaseNE(s1,s2) (strcasecmp(s1,s2))
#define strcaseEQ(s1,s2) (!strcasecmp(s1,s2))
#define strncaseCMP(s1,s2,l) strncasecmp(s1,s2,l)
#define strncaseNE(s1,s2,l) (strncasecmp(s1,s2,l))
#define strncaseEQ(s1,s2,l) (!strncasecmp(s1,s2,l))
#else
#define strcaseCMP(s1,s2) trn_casecmp(s1,s2)
#define strcaseNE(s1,s2) (trn_casecmp(s1,s2))
#define strcaseEQ(s1,s2) (!trn_casecmp(s1,s2))
#define strncaseCMP(s1,s2,l) trn_ncasecmp(s1,s2,l)
#define strncaseNE(s1,s2,l) (trn_ncasecmp(s1,s2,l))
#define strncaseEQ(s1,s2,l) (!trn_ncasecmp(s1,s2,l))
#endif

#ifndef FILE_REF
#   define FILE_REF(s) (*(s) == '/' ? '/' : 0)
#endif

/* how to open binary format files */
#ifndef FOPEN_RB
#   define FOPEN_RB "r"
#endif
#ifndef FOPEN_WB
#   define FOPEN_WB "w"
#endif
