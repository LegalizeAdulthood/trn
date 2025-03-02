/* This file Copyright 1992 by Clifford A. Adams */
/* sadesc.c
 *
 */

#include "common.h"
#include "sadesc.h"

#include "cache.h"
#include "head.h" /* currently used for fast author fetch when group is threaded */
#include "list.h"
#include "rt-util.h" /* compress_from() */
#include "samain.h"
#include "sathread.h"
#include "scan.h"
#include "scanart.h"
#include "score.h"
#include "terminal.h" /* for standout */

static char s_sa_buf[LBUFLEN];    /* misc. buffer */

/* returns statchars in temp space... */
// int line;            /* which status line (1 = first) */
const char *sa_get_statchars(long a, int line)
{
    static char char_buf[16];

/* Debug */
#if 0
    printf("entry: sa_get_statchars(%d,%d)\n",(int)a,line);
#endif

#if 0
    /* old 5-column status */
    switch (line) {
      case 1:
        strcpy(char_buf,".....");
        if (sa_marked(a))
            char_buf[4] = 'x';
        if (sa_selected1(a))
            char_buf[3] = '+';
        if (was_read(g_sa_ents[a].artnum))
            char_buf[0] = '-';
        else
            char_buf[0] = '+';
        break;
      default:
        strcpy(char_buf,"     ");
        break;
    } /* switch */
#else
    switch (line) {
      case 1:
        strcpy(char_buf,"...");
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
        strcpy(char_buf,"   ");
        break;
    } /* switch */
#endif
    return char_buf;
}

const char *sa_desc_subject(long e)
{
    static char sa_subj_buf[256];

    /* fetchlines saves its arguments */
    char *s = fetchlines(g_sa_ents[e].artnum, SUBJ_LINE);

    if (!s || !*s) {
        if (s)
        {
            free(s);
        }
        sprintf(sa_subj_buf,"(no subject)");
        return sa_subj_buf;
    }
    strncpy(sa_subj_buf,s,250);
    free(s);
    char *s1 = sa_subj_buf;
    if (*s1 == 'r' || *s1 == 'R') {
        if (*++s1 == 'e' || *s1 == 'E') {
            if (*++s1 ==':') {
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

    ART_NUM artnum = g_sa_ents[e].artnum;
    bool    use_standout = false;
    switch (line) {
      case 1:
        desc_buf[0] = '\0';     /* initialize the buffer */
        if (g_sa_mode_desc_artnum) {
            sprintf(s_sa_buf,"%6d ",(int)artnum);
            strcat(desc_buf,s_sa_buf);
        }
        if (g_sc_initialized && g_sa_mode_desc_score) {
            /* we'd like the score now */
            sprintf(s_sa_buf,"[%4d] ",sc_score_art(artnum,true));
            strcat(desc_buf,s_sa_buf);
        }
        if (g_sa_mode_desc_threadcount) {
            sprintf(s_sa_buf,"(%3d) ",sa_subj_thread_count(e));
            strcat(desc_buf,s_sa_buf);
        }
        if (g_sa_mode_desc_author) {
#if 0
            if (trunc)
                sprintf(s_sa_buf,"%s ",padspaces(sa_desc_author(e,16),16));
            else
                sprintf(s_sa_buf,"%s ",sa_desc_author(e,40));
            strcat(desc_buf,s_sa_buf);
#endif
            if (trunc)
            {
                strcat(desc_buf, compress_from(article_ptr(artnum)->from, 16));
            }
            else
            {
                strcat(desc_buf, compress_from(article_ptr(artnum)->from, 200));
            }
            strcat(desc_buf," ");
        }
        if (g_sa_mode_desc_subject) {
            sprintf(s_sa_buf,"%s",sa_desc_subject(e));
            strcat(desc_buf,s_sa_buf);
        }
        break;
      case 2:   /* summary line (test) */
        s = fetchlines(artnum,SUMRY_LINE);
        if (s && *s) { /* we really have one */
            int i;              /* number of spaces to indent */
            char* s2;   /* for indenting */

/* include the following line to use standout mode */
#if 0
            use_standout = true;
#endif
            i = 0;
            /* if variable widths used later, use them */
            if (g_sa_mode_desc_artnum)
            {
                i += 7;
            }
            if (g_sc_initialized && g_sa_mode_desc_score)
            {
                i += 7;
            }
            if (g_sa_mode_desc_threadcount)
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
                sprintf(s2, "Summary: %s%s", g_tc_SO, s);
            }
            else
#endif
            {
                sprintf(s2,"Summary: %s",s);
            }
            break;
        }
        /* otherwise, we might have had a keyword */
        /* FALL THROUGH */
      case 3:   /* Keywords (test) */
        s = fetchlines(artnum,KEYW_LINE);
        if (s && *s) { /* we really have one */
            int i;              /* number of spaces to indent */
            char* s2;   /* for indenting */
/* include the following line to use standout mode */
#if 0
            use_standout = true;
#endif
            i = 0;
            /* if variable widths used later, use them */
            if (g_sa_mode_desc_artnum)
            {
                i += 7;
            }
            if (g_sc_initialized && g_sa_mode_desc_score)
            {
                i += 7;
            }
            if (g_sa_mode_desc_threadcount)
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
                sprintf(s2, "Keys: %s%s", g_tc_SO, s);
            }
            else
#endif
            {
                sprintf(s2,"Keys: %s",s);
            }
            break;
        }
        /* FALL THROUGH */
      default:  /* no line I know of */
        /* later return nullptr */
        sprintf(desc_buf,"Entry %ld: Nonimplemented Description LINE",e);
        break;
    } /* switch (line) */
    if (trunc)
    {
        desc_buf[g_s_desc_cols] = '\0'; /* make sure it's not too long */
    }
#ifdef HAS_TERMLIB
    if (use_standout)
    {
        strcat(desc_buf,g_tc_SE);       /* end standout mode */
    }
#endif
    /* take out bad characters (replace with one space) */
    for (char *t = desc_buf; *t; t++)
    {
        switch (*t) {
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

    ART_NUM artnum = g_sa_ents[e].artnum;
    if (g_sa_mode_desc_summary) {
        s = fetchlines(artnum,SUMRY_LINE);
        if (s && *s)
        {
            num++;      /* just a test */
        }
        if (s)
        {
            free(s);
        }
    }
    if (g_sa_mode_desc_keyw) {
        s = fetchlines(artnum,KEYW_LINE);
        if (s && *s)
        {
            num++;      /* just a test */
        }
        if (s)
        {
            free(s);
        }
    }
    return num;
}
