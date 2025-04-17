/* This file Copyright 1992 by Clifford A. Adams */
/* sadesc.c
 *
 */

#include "config/common.h"
#include "trn/sadesc.h"

#include "trn/list.h"
#include "trn/cache.h"
#include "trn/head.h"    /* currently used for fast author fetch when group is threaded */
#include "trn/rt-util.h" /* compress_from() */
#include "trn/samain.h"
#include "trn/sathread.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/score.h"
#include "trn/terminal.h" /* for standout */

#include <cstdio>
#include <cstdlib>
#include <cstring>

static char s_sa_buf[LBUFLEN];    /* misc. buffer */

/* returns statchars in temp space... */
// int line;            /* which status line (1 = first) */
const char *sa_get_stat_chars(long a, int line)
{
    static char char_buf[16];

/* Debug */
#if 0
    std::printf("entry: sa_get_statchars(%d,%d)\n",(int)a,line);
#endif

#if 0
    /* old 5-column status */
    switch (line)
    {
    case 1:
        std::strcpy(char_buf,".....");
        if (sa_marked(a))
        {
            char_buf[4] = 'x';
        }
        if (sa_selected1(a))
        {
            char_buf[3] = '+';
        }
        if (was_read(g_sa_ents[a].artnum))
        {
            char_buf[0] = '-';
        }
        else
        {
            char_buf[0] = '+';
        }
        break;

    default:
        std::strcpy(char_buf,"     ");
        break;
    } /* switch */
#else
    switch (line)
    {
    case 1:
        std::strcpy(char_buf,"...");
        if (sa_marked(a))
        {
            char_buf[2] = 'x';
        }
        if (sa_selected1(a))
        {
            char_buf[1] = '*';
        }
        if (was_read(g_sa_ents[a].artnum))
        {
            char_buf[0] = '-';
        }
        else
        {
            char_buf[0] = '+';
        }
        break;

    default:
        std::strcpy(char_buf,"   ");
        break;
    } /* switch */
#endif
    return char_buf;
}

const char *sa_desc_subject(long e)
{
    static char sa_subj_buf[256];

    /* fetchlines saves its arguments */
    char *s = fetch_lines(g_sa_ents[e].artnum, SUBJ_LINE);

    if (!s || !*s)
    {
        if (s)
        {
            std::free(s);
        }
        std::sprintf(sa_subj_buf,"(no subject)");
        return sa_subj_buf;
    }
    std::strncpy(sa_subj_buf,s,250);
    std::free(s);
    char *s1 = sa_subj_buf;
    if (*s1 == 'r' || *s1 == 'R')
    {
        if (*++s1 == 'e' || *s1 == 'E')
        {
            if (*++s1 == ':')
            {
                *s1 = '>';              /* more cosmetic "Re:" */
                return s1;
            }
        }
    }
    return sa_subj_buf;
}

/* NOTE: should redesign later for the "menu" style... */
// long e;              /* entry number */
// bool trunc;          /* should it be truncated? */
const char *sa_get_desc(long e, int line, bool trunc)
{
    static char desc_buf[1024];
    char*       s;

    ArticleNum artnum = g_sa_ents[e].artnum;
    bool    use_standout = false;
    switch (line)
    {
    case 1:
        desc_buf[0] = '\0';     /* initialize the buffer */
        if (g_sa_mode_desc_art_num)
        {
            std::sprintf(s_sa_buf,"%6d ",(int)artnum);
            std::strcat(desc_buf,s_sa_buf);
        }
        if (g_sc_initialized && g_sa_mode_desc_score)
        {
            /* we'd like the score now */
            std::sprintf(s_sa_buf,"[%4d] ",sc_score_art(artnum,true));
            std::strcat(desc_buf,s_sa_buf);
        }
        if (g_sa_mode_desc_thread_count)
        {
            std::sprintf(s_sa_buf,"(%3d) ",sa_subj_thread_count(e));
            std::strcat(desc_buf,s_sa_buf);
        }
        if (g_sa_mode_desc_author)
        {
#if 0
            if (trunc)
            {
                std::sprintf(s_sa_buf,"%s ",padspaces(sa_desc_author(e,16),16));
            }
            else
            {
                std::sprintf(s_sa_buf,"%s ",sa_desc_author(e,40));
            }
            std::strcat(desc_buf,s_sa_buf);
#endif
            if (trunc)
            {
                std::strcat(desc_buf, compress_from(article_ptr(artnum)->from, 16));
            }
            else
            {
                std::strcat(desc_buf, compress_from(article_ptr(artnum)->from, 200));
            }
            std::strcat(desc_buf," ");
        }
        if (g_sa_mode_desc_subject)
        {
            std::sprintf(s_sa_buf,"%s",sa_desc_subject(e));
            std::strcat(desc_buf,s_sa_buf);
        }
        break;

    case 2:   /* summary line (test) */
        s = fetch_lines(artnum,SUMMARY_LINE);
        if (s && *s)   /* we really have one */
        {
            int i;              /* number of spaces to indent */
            char* s2;   /* for indenting */

/* include the following line to use standout mode */
#if 0
            use_standout = true;
#endif
            i = 0;
            /* if variable widths used later, use them */
            if (g_sa_mode_desc_art_num)
            {
                i += 7;
            }
            if (g_sc_initialized && g_sa_mode_desc_score)
            {
                i += 7;
            }
            if (g_sa_mode_desc_thread_count)
            {
                i += 6;
            }
            s2 = desc_buf;
            while (i--)
            {
                *s2++ = ' ';
            }
#ifdef HAS_TERMLIB
            if (use_standout)
            {
                std::sprintf(s2, "Summary: %s%s", g_tc_SO, s);
            }
            else
#endif
            {
                std::sprintf(s2,"Summary: %s",s);
            }
            break;
        }
        /* otherwise, we might have had a keyword */
        /* FALL THROUGH */

    case 3:   /* Keywords (test) */
        s = fetch_lines(artnum,KEYW_LINE);
        if (s && *s)   /* we really have one */
        {
            int i;              /* number of spaces to indent */
            char* s2;   /* for indenting */
/* include the following line to use standout mode */
#if 0
            use_standout = true;
#endif
            i = 0;
            /* if variable widths used later, use them */
            if (g_sa_mode_desc_art_num)
            {
                i += 7;
            }
            if (g_sc_initialized && g_sa_mode_desc_score)
            {
                i += 7;
            }
            if (g_sa_mode_desc_thread_count)
            {
                i += 6;
            }
            s2 = desc_buf;
            while (i--)
            {
                *s2++ = ' ';
            }
#ifdef HAS_TERMLIB
            if (use_standout)
            {
                std::sprintf(s2, "Keys: %s%s", g_tc_SO, s);
            }
            else
#endif
            {
                std::sprintf(s2,"Keys: %s",s);
            }
            break;
        }
        /* FALL THROUGH */

    default:  /* no line I know of */
        /* later return nullptr */
        std::sprintf(desc_buf,"Entry %ld: Nonimplemented Description LINE",e);
        break;
    } /* switch (line) */
    if (trunc)
    {
        desc_buf[g_s_desc_cols] = '\0'; /* make sure it's not too long */
    }
#ifdef HAS_TERMLIB
    if (use_standout)
    {
        std::strcat(desc_buf,g_tc_SE);       /* end standout mode */
    }
#endif
    /* take out bad characters (replace with one space) */
    for (char *t = desc_buf; *t; t++)
    {
        switch (*t)
        {
        case Ctl('h'):
        case '\t':
        case '\n':
        case '\r':
            *t = ' ';
        }
    }
    return desc_buf;
}

/* returns # of lines the article occupies in total... */
// long e;                      /* the entry number */
int sa_ent_lines(long e)
{
    char*s;
    int  num = 1;

    ArticleNum artnum = g_sa_ents[e].artnum;
    if (g_sa_mode_desc_summary)
    {
        s = fetch_lines(artnum,SUMMARY_LINE);
        if (s && *s)
        {
            num++;      /* just a test */
        }
        if (s)
        {
            std::free(s);
        }
    }
    if (g_sa_mode_desc_keyw)
    {
        s = fetch_lines(artnum,KEYW_LINE);
        if (s && *s)
        {
            num++;      /* just a test */
        }
        if (s)
        {
            std::free(s);
        }
    }
    return num;
}
