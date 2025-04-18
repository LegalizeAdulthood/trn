/* rcln.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/rcln.h"

#include "trn/ngdata.h"
#include "trn/datasrc.h"
#include "trn/rcstuff.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum
{
    MAX_DIGITS = 7
};

bool g_to_read_quiet{};

void rcln_init()
{
}

void catch_up(NewsgroupData *np, int leave_count, int output_level)
{
    char tmpbuf[128];

    if (leave_count)
    {
        if (output_level)
        {
            if (g_verbose)
            {
                std::printf("\nMarking all but %d articles in %s as read.\n",
                      leave_count,np->rc_line);
            }
            else
            {
                std::printf("\nAll but %d marked as read.\n", leave_count);
            }
        }
        check_expired(np, get_newsgroup_size(np) - leave_count + 1);
        set_to_read(np, ST_STRICT);
    }
    else
    {
        if (output_level)
        {
            if (g_verbose)
            {
                std::printf("\nMarking %s as all read.\n", np->rc_line);
            }
            else
            {
                std::fputs("\nMarked read\n", stdout);
            }
        }
        std::sprintf(tmpbuf,"%s: 1-%ld", np->rc_line,(long)get_newsgroup_size(np));
        std::free(np->rc_line);
        np->rc_line = save_str(tmpbuf);
        *(np->rc_line + np->num_offset - 1) = '\0';
        if (g_newsgroup_min_to_read > TR_NONE && np->to_read > TR_NONE)
        {
            g_newsgroup_to_read--;
        }
        np->to_read = TR_NONE;
    }
    np->rc->flags |= RF_RC_CHANGED;
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
}

/* add an article number to a newsgroup, if it isn't already read */

int add_art_num(DataSource *dp, ArticleNum art_num, const char *newsgroup_name)
{
    char*   s;
    char*   t;
    char*   maxt = nullptr;
    ArticleNum min = 0;
    ArticleNum max = -1;
    ArticleNum lastnum = 0;
    char*   mbuf;
    bool    morenum;

    if (!art_num)
    {
        return 0;
    }
    NewsgroupData *np = find_newsgroup(newsgroup_name);
    if (np == nullptr)                  /* not found in newsrc? */
    {
        return 0;
    }
    if (dp != np->rc->data_source)          /* punt on cross-host xrefs */
    {
#ifdef DEBUG
        if (debug & DEB_XREF_MARKER)
        {
            std::printf("Cross-host xref to group %s ignored.\n",ngnam);
        }
#endif
        return 0;
    }
    if (!np->num_offset)
    {
        return 0;
    }
#ifndef ANCIENT_NEWS
    if (!np->abs_first)
    {
        /* Trim down the list due to expires if we haven't done so yet. */
        set_to_read(np, ST_LAX);
    }
#endif
#if 0
    if (artnum > np->ngmax + 200)       /* allow for incoming articles */
    {
        std::printf("\nCorrupt Xref line!!!  %ld --> %s(1..%ld)\n",
            artnum,ngnam,
            np->ngmax);
        g_paranoid = true;              /* paranoia reigns supreme */
        return -1;                      /* hope this was the first newsgroup */
    }
#endif

    if (np->to_read == TR_BOGUS)
    {
        return 0;
    }
    if (art_num > np->ng_max)
    {
        if (np->to_read > TR_NONE)
        {
            np->to_read += art_num - np->ng_max;
        }
        np->ng_max = art_num;
    }
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("%ld->\n%s%c%s\n",(long)artnum,np->rcline, np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
    s = skip_eq(np->rc_line + np->num_offset, ' '); /* skip spaces */
    t = s;
    while (std::isdigit(*s) && art_num >= (min = std::atol(s)))
    {
                                        /* while it might have been read */
        t = skip_digits(s);             /* skip number */
        if (*t == '-')                  /* is it a range? */
        {
            t++;                        /* skip to next number */
            if (art_num <= (max = std::atol(t)))
            {
                return 0;               /* it is in range => already read */
            }
            lastnum = max;              /* remember it */
            maxt = t;                   /* remember position in case we */
                                        /* want to overwrite the max */
            t = skip_digits(t);         /* skip second number */
        }
        else
        {
            if (art_num == min)          /* explicitly a read article? */
            {
                return 0;
            }
            lastnum = min;              /* remember what the number was */
            maxt = nullptr;             /* last one was not a range */
        }
        while (*t && !std::isdigit(*t))
        {
            t++;                        /* skip comma and any spaces */
        }
        s = t;
    }

    /* we have not read it, so insert the article number before s */

    morenum = std::isdigit(*s);              /* will it need a comma after? */
    *(np->rc_line + np->num_offset - 1) = np->subscribe_char;
    mbuf = safe_malloc((MemorySize)(std::strlen(s)+(s - np->rc_line)+MAX_DIGITS+2+1));
    std::strcpy(mbuf,np->rc_line);            /* make new rc line */
    if (maxt && lastnum && art_num == lastnum+1)
                                        /* can we just extend last range? */
    {
        t = mbuf + (maxt - np->rc_line); /* then overwrite previous max */
    }
    else
    {
        t = mbuf + (t - np->rc_line);    /* point t into new line instead */
        if (lastnum)                    /* have we parsed any line? */
        {
            if (!morenum)               /* are we adding to the tail? */
            {
                *t++ = ',';             /* supply comma before */
            }
            if (!maxt && art_num == lastnum+1 && *(t-1) == ',')
                                        /* adjacent singletons? */
            {
                *(t-1) = '-';           /* turn them into a range */
            }
        }
    }
    if (morenum)                        /* is there more to life? */
    {
        if (min == art_num+1)            /* can we consolidate further? */
        {
            bool range_before = (*(t-1) == '-');
            char *nextmax = skip_digits(s);
            bool  range_after = *nextmax++ == '-';

            if (range_before)
            {
                *t = '\0';              /* artnum is redundant */
            }
            else
            {
                std::sprintf(t,"%ld-",(long)art_num);/* artnum will be new min */
            }

            if (range_after)
            {
                s = nextmax;            /* *s is redundant */
            }
        }
        else
        {
            std::sprintf(t,"%ld,",(long)art_num);     /* put the number and comma */
        }
    }
    else
    {
        std::sprintf(t,"%ld",(long)art_num);  /* put the number there (wherever) */
    }
    std::strcat(t,s);                        /* copy remainder of line */
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("%s\n",mbuf);
    }
#endif
    std::free(np->rc_line);
    np->rc_line = mbuf;          /* pull the switcheroo */
    *(np->rc_line + np->num_offset - 1) = '\0';
                                        /* wipe out : or ! */
    if (np->to_read > TR_NONE)   /* lest we turn unsub into bogus */
    {
        np->to_read--;
    }
    return 0;
}

/* delete an article number from a newsgroup, if it is there */

#ifdef MCHASE
void sub_art_num(DTASRC *dp, ART_NUM artnum, char *ngnam)
{
    NGDATA* np;
    char* s;
    char* t;
    ART_NUM min, max;
    char* mbuf;
    int curlen;

    if (!artnum)
    {
        return;
    }
    np = find_ng(ngnam);
    if (np == nullptr)                  /* not found in newsrc? */
    {
        return;
    }
    if (dp != np->rc->g_datasrc)                /* punt on cross-host xrefs */
    {
        return;
    }
    if (!np->numoffset)
    {
        return;
    }
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("%ld<-\n%s%c%s\n",(long)artnum,np->rcline,np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
    s = np->rcline + np->numoffset;
    s = skip_eq(s, ' ');                /* skip spaces */

    /* a little optimization, since it is almost always the last number */

    for (t=s; *t; t++)                  /* find end of string */
    {
    }
    curlen = t - np->rcline;
    for (t--; std::isdigit(*t); t--)         /* find previous delim */
    {
    }
    if (*t == ',' && std::atol(t + 1) == artnum)
    {
        *t = '\0';
        if (np->toread >= TR_NONE)
        {
            ++np->toread;
        }
#ifdef DEBUG
        if (debug & DEB_XREF_MARKER)
        {
            std::printf("%s%c %s\n",np->rcline,np->subscribechar,s);
        }
#endif
        return;
    }

    /* not the last number, oh well, we may need the length anyway */

    while (std::isdigit(*s) && artnum >= (min = std::atol(s)))
    {
                                        /* while it might have been read */
        t = skip_digits(s);             /* skip number */
        if (*t == '-')                  /* is it a range? */
        {
            t++;                        /* skip to next number */
            max = std::atol(t);
            t = skip_digit(t);          /* skip second number */
            if (artnum <= max)
            {
                                        /* it is in range => already read */
                if (artnum == min)
                {
                    min++;
                    artnum = 0;
                }
                else if (artnum == max)
                {
                    max--;
                    artnum = 0;
                }
                *(np->rcline + np->numoffset - 1) = np->subscribechar;
                mbuf = safe_malloc((MemorySize)(curlen+(artnum?(MAX_DIGITS+1)*2+1:1+1)));
                *s = '\0';
                std::strcpy(mbuf,np->rcline);        /* make new rc line */
                s = mbuf + (s - np->rcline);
                                        /* point s into mbuf now */
                if (artnum)             /* split into two ranges? */
                {
                    prange(s,min,artnum-1);
                    s += std::strlen(s);
                    *s++ = ',';
                    prange(s,artnum+1,max);
                }
                else                    /* only one range */
                {
                    prange(s,min,max);
                }
                std::strcat(s,t);            /* copy remainder over */
#ifdef DEBUG
                if (debug & DEB_XREF_MARKER)
                {
                    std::printf("%s\n",mbuf);
                }
#endif
                std::free(np->rcline);
                np->rcline = mbuf;      /* pull the switcheroo */
                *(np->rcline + np->numoffset - 1) = '\0';
                                        /* wipe out : or ! */
                if (np->toread >= TR_NONE)
                {
                    np->toread++;
                }
                return;
            }
        }
        else
        {
            if (artnum == min)          /* explicitly a read article? */
            {
                if (*t == ',')          /* pick a comma, any comma */
                {
                    t++;
                }
                else if (s[-1] == ',')
                {
                    s--;
                }
                else if (s[-2] == ',')  /* (in case of space) */
                {
                    s -= 2;
                }
                std::strcpy(s,t);            /* no need to realloc */
                if (np->toread >= TR_NONE)
                {
                    np->toread++;
                }
#ifdef DEBUG
                if (debug & DEB_XREF_MARKER)
                {
                    std::printf("%s%c%s\n",np->rcline,np->subscribechar,
                      np->rcline + np->numoffset);
                }
#endif
                return;
            }
        }
        while (*t && !std::isdigit(*t))      /* skip comma and any spaces */
        {
            t++;
        }
        s = t;
    }
}

void prange(char *where, ART_NUM min, ART_NUM max)
{
    if (min == max)
    {
        std::sprintf(where,"%ld",(long)min);
    }
    else
    {
        std::sprintf(where,"%ld-%ld",(long)min,(long)max);
    }
}
#endif

/* calculate the number of unread articles for a newsgroup */

void set_to_read(NewsgroupData *np, bool lax_high_check)
{
    char*   c;
    char    tmpbuf[64];
    char*   mybuf = tmpbuf;
    bool    virgin_ng = (!np->abs_first);
    ArticleNum ngsize = get_newsgroup_size(np);
    ArticleNum unread = ngsize;
    ArticleNum newmax;

    if (ngsize == TR_BOGUS)
    {
        if (!g_to_read_quiet)
        {
            std::printf("\nInvalid (bogus) newsgroup found: %s\n",np->rc_line);
        }
        g_paranoid = true;
        if (virgin_ng || np->to_read >= g_newsgroup_min_to_read)
        {
            g_newsgroup_to_read--;
            g_missing_count++;
        }
        np->to_read = TR_BOGUS;
        return;
    }
    if (virgin_ng)
    {
        std::sprintf(tmpbuf," 1-%ld",(long)ngsize);
        if (std::strcmp(tmpbuf,np->rc_line+np->num_offset) != 0)
        {
            check_expired(np,np->abs_first);        /* this might realloc rcline */
        }
    }
    char *nums = np->rc_line + np->num_offset;
    int   length = std::strlen(nums);
    if (length+MAX_DIGITS+1 > sizeof tmpbuf)
    {
        mybuf = safe_malloc((MemorySize) (length + MAX_DIGITS + 1));
    }
    std::strcpy(mybuf,nums);
    mybuf[length++] = ',';
    mybuf[length] = '\0';
    char *s = skip_space(mybuf);
    for ( ; (c = std::strchr(s,',')) != nullptr ; s = ++c)    /* for each range */
    {
        *c = '\0';                  /* keep index from running off */
        char *h = std::strchr(s, '-');
        if (h != nullptr) /* find - in range, if any */
        {
            unread -= (newmax = atol(h + 1)) - atol(s) + 1;
        }
        else
        {
            newmax = std::atol(s);
        }
        if (newmax != 0)
        {
            unread--;           /* recalculate length */
        }
        if (newmax > ngsize)    /* paranoia check */
        {
            if (!lax_high_check && newmax > ngsize)
            {
                unread = -1;
                break;
            }
            unread += newmax - ngsize;
            np->ng_max = newmax;
            ngsize = newmax;
        }
    }
    if (unread < 0)                     /* SOMEONE RESET THE NEWSGROUP!!! */
    {
        unread = (ArticleUnread)ngsize;    /* assume nothing carried over */
        if (!g_to_read_quiet)
        {
            std::printf("\nSomebody reset %s -- assuming nothing read.\n",
                   np->rc_line);
        }
        *(np->rc_line + np->num_offset) = '\0';
        g_paranoid = true;          /* enough to make a guy paranoid */
        np->rc->flags |= RF_RC_CHANGED;
    }
    if (np->subscribe_char == NEGCHAR)
    {
        unread = TR_UNSUB;
    }

    if (unread >= g_newsgroup_min_to_read)
    {
        if (!virgin_ng && np->to_read < g_newsgroup_min_to_read)
        {
            g_newsgroup_to_read++;
        }
    }
    else if (unread <= 0)
    {
        if (np->to_read > g_newsgroup_min_to_read)
        {
            g_newsgroup_to_read--;
            if (virgin_ng)
            {
                g_missing_count++;
            }
        }
    }
    np->to_read = (ArticleUnread)unread;    /* remember how many are left */

    if (mybuf != tmpbuf)
    {
        std::free(mybuf);
    }
}

/* make sure expired articles are marked as read */

void check_expired(NewsgroupData *np, ArticleNum a1st)
{
    char   *s;
    ArticleNum num;
    ArticleNum lastnum = 0;
    char   * mbuf;
    char* cp;
    int len;

    if (a1st<=1)
    {
        return;
    }
#ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("1-%ld->\n%s%c%s\n",(long)(a1st-1),np->rcline,np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
    s = skip_space(np->rc_line + np->num_offset);
    while (*s && (num = std::atol(s)) <= a1st)
    {
        s = skip_digits(s);
        while (*s && !std::isdigit(*s))
        {
            s++;
        }
        lastnum = num;
    }
    len = std::strlen(s);
    if (len && s[-1] == '-')                    /* landed in a range? */
    {
        if (lastnum != 1)
        {
            if (3+len <= (int)std::strlen(np->rc_line+np->num_offset))
            {
                mbuf = np->rc_line;
            }
            else
            {
                mbuf = safe_malloc((MemorySize)(np->num_offset+3+len+1));
                std::strcpy(mbuf, np->rc_line);
            }
            cp = mbuf + np->num_offset;
            *cp++ = ' ';
            *cp++ = '1';
            *cp++ = '-';
            safe_copy(cp, s, len+1);
            if (np->rc_line != mbuf)
            {
                std::free(np->rc_line);
                np->rc_line = mbuf;
            }
            np->rc->flags |= RF_RC_CHANGED;
        }
    }
    else
    {
        /* s now points to what should follow the first range */
        char numbuf[32];

        std::sprintf(numbuf," 1-%ld",(long)(a1st - (lastnum != a1st)));
        int nlen = std::strlen(numbuf) + (len != 0);

        if (s - np->rc_line >= np->num_offset + nlen)
        {
            mbuf = np->rc_line;
        }
        else
        {
            mbuf = safe_malloc((MemorySize)(np->num_offset+nlen+len+1));
            std::strcpy(mbuf,np->rc_line);
        }

        cp = mbuf + np->num_offset;
        std::strcpy(cp, numbuf);
        cp += nlen;

        if (len)
        {
            cp[-1] = ',';
            if (cp != s)
            {
                safe_copy(cp, s, len + 1);
            }
        }

        if (!g_check_flag && np->rc_line == mbuf)
        {
            np->rc_line = safe_realloc(np->rc_line, (MemorySize) (cp - mbuf + len + 1));
        }
        else
        {
            if (!g_check_flag)
            {
                std::free(np->rc_line);
            }
            np->rc_line = mbuf;
        }
        np->rc->flags |= RF_RC_CHANGED;
    }

#ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("%s%c%s\n",np->rcline,np->subscribechar,
          np->rcline + np->numoffset);
    }
#endif
}

/* Returns true if article is marked as read or does not exist */
/* could use a better name */
bool was_read_group(DataSource *dp, ArticleNum artnum, char *ngnam)
{
    char*   s;
    char*   t;
    ArticleNum min = 0;
    ArticleNum max = -1;

    if (!artnum)
    {
        return true;
    }
    NewsgroupData *np = find_newsgroup(ngnam);
    if (np == nullptr)          /* not found in newsrc? */
    {
        return true;
    }
    if (!np->num_offset)         /* no numbers on line */
    {
        return false;
    }
#if 0
    /* consider this code later */
    if (!np->abs1st)
    {
        /* Trim down the list due to expires if we haven't done so yet. */
        set_toread(np, ST_LAX);
    }
#endif

    if (np->to_read == TR_BOGUS)
    {
        return true;
    }
    if (artnum > np->ng_max)
    {
        return false;           /* probably doesn't exist, however */
    }
    s = skip_eq(np->rc_line + np->num_offset, ' '); /* skip spaces */
    t = s;
    while (std::isdigit(*s) && artnum >= (min = std::atol(s)))
    {
        char*   maxt = nullptr;
        ArticleNum lastnum = 0;
        /* while it might have been read */
        t = skip_digits(s);             /* skip number */
        if (*t == '-')                  /* is it a range? */
        {
            t++;                        /* skip to next number */
            if (artnum <= (max = std::atol(t)))
            {
                return true;            /* it is in range => already read */
            }
            lastnum = max;              /* remember it */
            maxt = t;                   /* remember position in case we */
                                        /* want to overwrite the max */
            t = skip_digits(t);         /* skip second number */
        }
        else
        {
            if (artnum == min)          /* explicitly a read article? */
            {
                return true;
            }
            lastnum = min;              /* remember what the number was */
            maxt = nullptr;             /* last one was not a range */
        }
        while (*t && !std::isdigit(*t))
        {
            t++;                        /* skip comma and any spaces */
        }
        s = t;
    }

    /* we have not read it, so return false */
    return false;
}
