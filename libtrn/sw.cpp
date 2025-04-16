/* sw.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/fdio.h>

#include "config/common.h"
#include "trn/sw.h"

#include "util/env.h"
#include "trn/head.h"
#include "trn/init.h"
#include "trn/intrp.h"
#include "trn/ng.h"
#include "trn/ngdata.h"
#include "trn/only.h"
#include "trn/opt.h"
#include "trn/rcstuff.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static char **s_init_environment_strings{};
static int    s_init_environment_cnt{};
static int    s_init_environment_max{};

void sw_file(char **tcbufptr)
{
    int initfd = open(*tcbufptr,0);

    if (initfd >= 0)
    {
        stat_t switch_file_stat{};
        fstat(initfd,&switch_file_stat);
        if (switch_file_stat.st_size >= TCBUF_SIZE-1)
        {
            *tcbufptr = saferealloc(*tcbufptr,(MEM_SIZE)switch_file_stat.st_size+1);
        }
        if (switch_file_stat.st_size)
        {
            int len = read(initfd,*tcbufptr,(int)switch_file_stat.st_size);
            (*tcbufptr)[len] = '\0';
            sw_list(*tcbufptr);
        }
        else
        {
            **tcbufptr = '\0';
        }
        close(initfd);
    }
}

/* decode a list of space separated switches */

void sw_list(char *swlist)
{
    char* p;
    char inquote = 0;

    char *s = swlist;
    p = swlist;
    while (*s)                          /* "String, or nothing" */
    {
        if (!inquote && std::isspace(*s))    /* word delimiter? */
        {
            while (true)
            {
                s = skip_space(s);
                if (*s != '#')
                {
                    break;
                }
                s = skip_ne(s, '\n');
            }
            if (p != swlist)
            {
                *p++ = '\0';            /* chop here */
            }
        }
        else if (inquote == *s)
        {
            s++;                        /* delete trailing quote */
            inquote = 0;                /* no longer quoting */
        }
        else if (!inquote && (*s == '"' || *s == '\''))
        {
                                        /* OK, I know when I am not wanted */
            inquote = *s++;             /* remember & del single or double */
        }
        else if (*s == '\\')            /* quoted something? */
        {
            if (*++s != '\n')           /* newline? */
            {
                s = interp_backslash(p, s);
                p++;
            }
            s++;
        }
        else
        {
            *p++ = *s++;                /* normal char */
        }
    }
    *p++ = '\0';
    *p = '\0';                          /* put an extra null on the end */
    if (inquote)
    {
        std::printf("Unmatched %c in switch\n",inquote);
        termdown(1);
    }
    for (char *c = swlist; *c; /* p += strlen(p)+1 */)
    {
        decode_switch(c);
        while (*c++)
        {
                                        /* point at null + 1 */
        }
    }
}

/* decode a single switch */

void decode_switch(const char *s)
{
    s = skip_space(s);          /* ignore leading spaces */
#ifdef DEBUG
    if (debug)
    {
        std::printf("Switch: %s\n",s);
        termdown(1);
    }
#endif
    if (*s != '-' && *s != '+')         /* newsgroup pattern */
    {
        setngtodo(s);
        if (g_mode == MM_INITIALIZING)
        {
            g_ng_min_toread = 0;
        }
    }
    else                                /* normal switch */
    {
        bool upordown = *s == '-';
        char tmpbuf[LBUFLEN];

        switch (*++s)
        {
        case '/':
            set_option(OI_AUTO_SAVE_NAME, YESorNO(upordown));
            break;

        case '+':
            set_option(OI_USE_ADD_SEL, YESorNO(upordown));
            set_option(OI_USE_NEWSGROUP_SEL, YESorNO(upordown));
            if (upordown)
            {
                set_option(OI_INITIAL_GROUP_LIST, YESorNO(false));
            }
            else
            {
                set_option(OI_USE_NEWSRC_SEL, YESorNO(false));
            }
            break;

        case 'a':
            set_option(OI_BKGND_THREADING, YESorNO(!upordown));
            break;

        case 'A':
            set_option(OI_AUTO_ARROW_MACROS, YESorNO(upordown));
            break;

        case 'b':
            set_option(OI_READ_BREADTH_FIRST, YESorNO(upordown));
            break;

        case 'B':
            set_option(OI_BKGND_SPINNER, YESorNO(upordown));
            break;

        case 'c':
            g_checkflag = upordown;
            break;

        case 'C':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_CHECKPOINT_NEWSRC_FREQUENCY, s);
            break;

        case 'd':
        {
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_SAVE_DIR, s);
            break;
        }

        case 'D':
#ifdef DEBUG
            if (*++s == '=')
            {
                s++;
            }
            if (*s)
            {
                if (upordown)
                {
                    debug |= std::atoi(s);
                }
                else
                {
                    debug &= ~std::atoi(s);
                }
            }
            else
            {
                if (upordown)
                {
                    debug |= 1;
                }
                else
                {
                    debug = 0;
                }
            }
#else
            std::printf("Trn was not compiled with -DDEBUG.\n");
            termdown(1);
#endif
            break;

        case 'e':
            set_option(OI_ERASE_SCREEN, YESorNO(upordown));
            break;

        case 'E':
        {
            if (*++s == '=')
            {
                s++;
            }
            std::strcpy(tmpbuf,s);
            char *tmp = std::strchr(tmpbuf,'=');
            if (tmp)
            {
                *tmp++ = '\0';
                tmp = export_var(tmpbuf,tmp) - (tmp-tmpbuf);
                if (g_mode == MM_INITIALIZING)
                {
                    save_init_environment(tmp);
                }
            }
            else
            {
                tmp = export_var(tmpbuf,"") - std::strlen(tmpbuf) - 1;
                if (g_mode == MM_INITIALIZING)
                {
                    save_init_environment(tmp);
                }
            }
            break;
        }

        case 'f':
            set_option(OI_NOVICE_DELAYS, YESorNO(!upordown));
            break;

        case 'F':
            set_option(OI_CITED_TEXT_STRING, s+1);
            break;

        case 'g':
            set_option(OI_GOTO_LINE_NUM, s+1);
            break;

        case 'G':
            set_option(OI_FUZZY_NEWSGROUP_NAMES, YESorNO(upordown));
            break;

        case 'h':
            if (!s[1])
            {
                /* Free old g_user_htype list */
                while (g_user_htype_cnt > 1)
                {
                    std::free(g_user_htype[--g_user_htype_cnt].name);
                }
                std::memset(g_user_htypeix,0,26);
            }
            /* FALL THROUGH */

        case 'H':
            if (g_checkflag)
            {
                break;
            }
            set_header(s+1,*s == 'h'? HT_HIDE : HT_MAGIC, upordown);
            break;

        case 'i':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_INITIAL_ARTICLE_LINES, s);
            break;

        case 'I':
            set_option(OI_APPEND_UNSUBSCRIBED_GROUPS, YESorNO(upordown));
            break;

        case 'j':
            set_option(OI_FILTER_CONTROL_CHARACTERS, YESorNO(!upordown));
            break;

        case 'J':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_JOIN_SUBJECT_LINES,
                       upordown && *s? s : YESorNO(upordown));
            break;

        case 'k':
            set_option(OI_IGNORE_THRU_ON_SELECT, YESorNO(upordown));
            break;

        case 'K':
            set_option(OI_AUTO_GROW_GROUPS, YESorNO(!upordown));
            break;

        case 'l':
            set_option(OI_MUCK_UP_CLEAR, YESorNO(upordown));
            break;

        case 'L':
            set_option(OI_ERASE_EACH_LINE, YESorNO(upordown));
            break;

        case 'M':
            if (upordown)
            {
                set_option(OI_SAVEFILE_TYPE, "mail");
            }
            break;

        case 'm':
            set_option(OI_PAGER_LINE_MARKING, s+1);
            break;

        case 'N':
            if (upordown)
            {
                set_option(OI_SAVEFILE_TYPE, "norm");
            }
            break;

        case 'o':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_OLD_MTHREADS_DATABASE, s);
            break;

        case 'O':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_NEWS_SEL_MODE, s);
            if (*++s)
            {
                char tmpbuf2[4];
                std::sprintf(tmpbuf2, "%s%c", std::isupper(*s)? "r " : "", *s);
                set_option(OI_NEWS_SEL_ORDER, tmpbuf2);
            }
            break;

        case 'p':
            if (*++s == '=')
            {
                s++;
            }
            if (!upordown)
            {
                s = YESorNO(false);
            }
            else
            {
                switch (*s)
                {
                case '+':
                    s = "thread";
                    break;

                case 'p':
                    s = "parent";
                    break;

                default:
                    s = "subthread";
                    break;
                }
            }
            set_option(OI_SELECT_MY_POSTS, s);
            break;

        case 'q':
            set_option(OI_NEWGROUP_CHECK, YESorNO(!upordown));
            break;

        case 'Q':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_CHARSET, s);
            break;

        case 'r':
            set_option(OI_RESTART_AT_LAST_GROUP, YESorNO(upordown));
            break;

        case 's':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_INITIAL_GROUP_LIST, std::isdigit(*s)? s : YESorNO(false));
            break;

        case 'S':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_SCANMODE_COUNT, s);
            break;

        case 't':
            set_option(OI_TERSE_OUTPUT, YESorNO(upordown));
            break;

        case 'T':
            set_option(OI_EAT_TYPEAHEAD, YESorNO(!upordown));
            break;

        case 'u':
            set_option(OI_COMPRESS_SUBJECTS, YESorNO(!upordown));
            break;

        case 'U':
            g_unsafe_rc_saves = upordown;
            break;

        case 'v':
            set_option(OI_VERIFY_INPUT, YESorNO(upordown));
            break;

        case 'V':
            if (g_mode == MM_INITIALIZING)
            {
                g_tc_LINES = 1000;
                g_tc_COLS = 1000;
                g_erase_screen = false;
            }
            trn_version();
            newline();
            if (g_mode == MM_INITIALIZING)
            {
                std::exit(0);
            }
            break;

        case 'x':
            if (*++s == '=')
            {
                s++;
            }
            if (std::isdigit(*s))
            {
                set_option(OI_ARTICLE_TREE_LINES, s);
                s = skip_digits(s);
            }
            if (*s)
            {
                set_option(OI_NEWS_SEL_STYLES, s);
            }
            set_option(OI_USE_THREADS, YESorNO(upordown));
            break;

        case 'X':
            if (*++s == '=')
            {
                s++;
            }
            if (std::isdigit(*s))
            {
                set_option(OI_USE_NEWS_SEL, s);
                s = skip_digits(s);
            }
            else
            {
                set_option(OI_USE_NEWS_SEL, YESorNO(upordown));
            }
            if (*s)
            {
                set_option(OI_NEWS_SEL_CMDS, s);
            }
            break;

        case 'z':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_DEFAULT_REFETCH_TIME,
                       upordown && *s? s : YESorNO(upordown));
            break;

        default:
            if (g_verbose)
            {
                std::printf("\nIgnoring unrecognized switch: -%c\n", *s);
            }
            else
            {
                std::printf("\nIgnoring -%c\n", *s);
            }
            termdown(2);
            break;
        }
    }
}

void save_init_environment(char *str)
{
    if (s_init_environment_cnt >= s_init_environment_max)
    {
        s_init_environment_max += 32;
        s_init_environment_strings = (char**)
          saferealloc((char*)s_init_environment_strings,
                      s_init_environment_max * sizeof (char*));
    }
    s_init_environment_strings[s_init_environment_cnt++] = str;
}

void write_init_environment(std::FILE *fp)
{
    for (int i = 0; i < s_init_environment_cnt; i++)
    {
        char *s = std::strchr(s_init_environment_strings[i], '=');
        if (!s)
        {
            continue;
        }
        *s = '\0';
        std::fprintf(fp, "%s=%s\n", s_init_environment_strings[i],quote_string(s+1));
        *s = '=';
    }
    s_init_environment_cnt = 0;
    s_init_environment_max = 0;
    std::free(s_init_environment_strings);
    s_init_environment_strings = nullptr;
}
