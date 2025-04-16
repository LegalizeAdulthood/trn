/* bits.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"

#include "trn/bits.h"

#include "nntp/nntpclient.h"
#include "trn/List.h"
#include "trn/ngdata.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rthread.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

int g_dmcount{};

static long s_chase_count{};

static bool yank_article(char *ptr, int arg);
static bool check_chase(char *ptr, int until_key);
static int chase_xref(ArticleNum artnum, int markread);
#ifdef VALIDATE_XREF_SITE
static bool valid_xref_site(ART_NUM artnum, char *site);
#endif

void bits_init()
{
}

void rc_to_bits()
{
    char*   mybuf = g_buf; /* place to decode rc line */
    char*   c;
    char*   h;
    ArticleNum unread;
    Article*ap;

    /* modify the article flags to reflect what has already been read */

    char *s = skip_eq(g_ngptr->rcline + g_ngptr->numoffset, ' ');
                                        /* find numbers in rc line */
    long i = std::strlen(s);
#ifndef lint
    if (i >= LBUFLEN-2)                 /* bigger than g_buf? */
    {
        mybuf = safemalloc((MEM_SIZE) (i + 2));
    }
#endif
    std::strcpy(mybuf,s);                    /* make scratch copy of line */
    if (mybuf[0])
    {
        mybuf[i++] = ',';               /* put extra comma on the end */
    }
    mybuf[i] = '\0';
    s = mybuf;                          /* initialize the for loop below */
    if (set_firstart(s))
    {
        s = std::strchr(s,',') + 1;
        for (i = article_first(g_absfirst); i < g_firstart; i = article_next(i))
        {
            article_ptr(i)->flags &= ~AF_UNREAD;
        }
        g_firstart = i;
    }
    else
    {
        g_firstart = article_first(g_firstart);
    }
    unread = 0;
#ifdef DEBUG
    if (debug & DEB_CTLAREA_BITMAP)
    {
        std::printf("\n%s\n",mybuf);
        termdown(2);
        for (i = article_first(g_absfirst); i < g_firstart; i = article_next(i))
        {
            if (article_unread(i))
            {
                std::printf("%ld ",(long)i);
            }
        }
    }
#endif
    i = g_firstart;
    for ( ; (c = std::strchr(s,',')) != nullptr; s = ++c)    /* for each range */
    {
        ArticleNum max;
        *c = '\0';                      /* do not let index see past comma */
        h = std::strchr(s,'-');
        ArticleNum min = std::atol(s);
        min = std::max(min, g_firstart);    /* make sure range is in range */
        if (min > g_lastart)
        {
            min = g_lastart + 1;
        }
        for (; i < min; i = article_next(i))
        {
            ap = article_ptr(i);
            if (ap->flags & AF_EXISTS)
            {
                if (ap->autofl & AUTO_KILL_MASK)
                {
                    ap->flags &= ~AF_UNREAD;
                }
                else
                {
                    ap->flags |= AF_UNREAD;
                    unread++;
                    if (ap->autofl & AUTO_SEL_MASK)
                    {
                        select_article(ap, ap->autofl);
                    }
                }
            }
        }
        if (!h)
        {
            max = min;
        }
        else if ((max = std::atol(h+1)) < min)
        {
            max = min - 1;
        }
        max = std::min(max, g_lastart);
        /* mark all arts in range as read */
        for ( ; i <= max; i = article_next(i))
        {
            article_ptr(i)->flags &= ~AF_UNREAD;
        }
#ifdef DEBUG
        if (debug & DEB_CTLAREA_BITMAP)
        {
            std::printf("\n%s\n",s);
            termdown(2);
            for (i = g_absfirst; i <= g_lastart; i++)
            {
                if (!was_read(i))
                {
                    std::printf("%ld ",(long)i);
                }
            }
        }
#endif
        i = article_next(max);
    }
    for (; i <= g_lastart; i = article_next(i))
    {
        ap = article_ptr(i);
        if (ap->flags & AF_EXISTS)
        {
            if (ap->autofl & AUTO_KILL_MASK)
            {
                ap->flags &= ~AF_UNREAD;
            }
            else
            {
                ap->flags |= AF_UNREAD;
                unread++;
                if (ap->autofl & AUTO_SEL_MASK)
                {
                    select_article(ap, ap->autofl);
                }
            }
        }
    }
#ifdef DEBUG
    if (debug & DEB_CTLAREA_BITMAP)
    {
        std::fputs("\n(hit CR)",stdout);
        termdown(1);
        std::fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    if (mybuf != g_buf)
    {
        std::free(mybuf);
    }
    g_ngptr->toread = unread;
}

bool set_firstart(const char *s)
{
    s = skip_eq(s, ' ');
    if (!std::strncmp(s,"1-",2))                     /* can we save some time here? */
    {
        g_firstart = std::atol(s+2)+1;               /* process first range thusly */
        g_firstart = std::max(g_firstart, g_absfirst);
        return true;
    }

    g_firstart = g_absfirst;
    return false;
}

/* reconstruct the .newsrc line in a human readable form */

void bits_to_rc()
{
    char* mybuf = g_buf;
    ArticleNum i;
    ArticleNum count=0;
    int safelen = LBUFLEN - 32;

    std::strcpy(g_buf,g_ngptr->rcline);            /* start with the newsgroup name */
    char *s = g_buf + g_ngptr->numoffset - 1; /* use s for buffer pointer */
    *s++ = g_ngptr->subscribechar;            /* put the requisite : or !*/
    for (i = article_first(g_absfirst); i <= g_lastart; i = article_next(i))
    {
        if (article_unread(i))
        {
            break;
        }
    }
    std::sprintf(s," 1-%ld,",(long)i-1);
    s += std::strlen(s);
    for (; i<=g_lastart; i++)   /* for each article in newsgroup */
    {
        if (s-mybuf > safelen)          /* running out of room? */
        {
            safelen *= 2;
            if (mybuf == g_buf)         /* currently static? */
            {
                *s = '\0';
                mybuf = safemalloc((MEM_SIZE)safelen + 32);
                std::strcpy(mybuf,g_buf);    /* so we must copy it */
                s = mybuf + (s-g_buf);
                                        /* fix the pointer, too */
            }
            else                        /* just grow in place, if possible */
            {
                int oldlen = s - mybuf;
                mybuf = saferealloc(mybuf,(MEM_SIZE)safelen + 32);
                s = mybuf + oldlen;
            }
        }
        if (!was_read(i))               /* still unread? */
        {
            count++;                    /* then count it */
        }
        else                            /* article was read */
        {
            std::sprintf(s,"%ld",(long)i); /* put out the min of the range */
            s += std::strlen(s);           /* keeping house */
            ArticleNum oldi = i;         /* remember this spot */
            do
            {
                i++;
            } while (i <= g_lastart && was_read(i));
                                        /* find 1st unread article or end */
            i--;                        /* backup to last read article */
            if (i > oldi)               /* range of more than 1? */
            {
                std::sprintf(s,"-%ld,",(long)i);
                                        /* then it out as a range */
                s += std::strlen(s);         /* and housekeep */
            }
            else
            {
                *s++ = ',';             /* otherwise, just a comma will do */
            }
        }
    }
    if (*(s-1) == ',')                  /* is there a final ','? */
    {
        s--;                            /* take it back */
    }
    *s++ = '\0';                        /* and terminate string */
#ifdef DEBUG
    if ((debug & DEB_NEWSRC_LINE) && !g_panic)
    {
        std::printf("%s: %s\n",g_ngptr->rcline,g_ngptr->rcline+g_ngptr->numoffset);
        std::printf("%s\n",mybuf);
        termdown(2);
    }
#endif
    std::free(g_ngptr->rcline);              /* return old rc line */
    if (mybuf == g_buf)
    {
        g_ngptr->rcline = safemalloc((MEM_SIZE)(s-g_buf)+1);
                                        /* grab a new rc line */
        std::strcpy(g_ngptr->rcline, g_buf); /* and load it */
    }
    else
    {
        mybuf = saferealloc(mybuf,(MEM_SIZE)(s-mybuf)+1);
                                        /* be nice to the heap */
        g_ngptr->rcline = mybuf;
    }
    *(g_ngptr->rcline + g_ngptr->numoffset - 1) = '\0';
    if (g_ngptr->subscribechar == NEGCHAR)/* did they unsubscribe? */
    {
        g_ngptr->toread = TR_UNSUB;     /* make line invisible */
    }
    else
    {
        g_ngptr->toread = (ART_UNREAD)count; /* otherwise, remember the count */
    }
    g_ngptr->rc->flags |= RF_RCCHANGED;
}

void find_existing_articles()
{
    ArticleNum an;
    Article* ap;

    if (g_datasrc->flags & DF_REMOTE)
    {
        /* Parse the LISTGROUP output and remember everything we find */
        if (nntp_artnums())
        {
            for (ap = article_ptr(article_first(g_absfirst));
                 ap && article_num(ap) <= g_lastart;
                 ap = article_nextp(ap))
            {
                ap->flags &= ~AF_EXISTS;
            }
            while (true)
            {
                if (nntp_gets(g_ser_line, sizeof g_ser_line) == NGSR_ERROR)
                {
                    break;
                }
                if (nntp_at_list_end(g_ser_line))
                {
                    break;
                }
                an = (ArticleNum)std::atol(g_ser_line);
                if (an < g_absfirst)
                {
                    continue;   /* Ignore some wacked-out NNTP servers */
                }
                ap = article_ptr(an);
                if (!(ap->flags2 & AF2_BOGUS))
                {
                    ap->flags |= AF_EXISTS;
                }
#if 0
                s = std::strchr(g_ser_line, ' ');
                if (s)
                {
                    rover_thread(article_ptr(an), s);
                }
#endif
            }
        }
        else if (g_first_subject && g_cached_all_in_range)
        {
            if (!g_datasrc->ov_opened || g_datasrc->over_dir != nullptr)
            {
                for (ap = article_ptr(article_first(g_first_cached));
                     ap && article_num(ap) <= g_last_cached;
                     ap = article_nextp(ap))
                {
                    if (ap->flags & AF_CACHED)
                    {
                        ap->flags |= AF_EXISTS;
                    }
                }
            }
            for (an = g_absfirst; an < g_first_cached; an++)
            {
                ap = article_ptr(an);
                if (!(ap->flags2 & AF2_BOGUS))
                {
                    ap->flags |= AF_EXISTS;
                }
            }
            for (an = g_last_cached + 1; an <= g_lastart; an++)
            {
                ap = article_ptr(an);
                if (!(ap->flags2 & AF2_BOGUS))
                {
                    ap->flags |= AF_EXISTS;
                }
            }
        }
        else
        {
            for (an = g_absfirst; an <= g_lastart; an++)
            {
                ap = article_ptr(an);
                if (!(ap->flags2 & AF2_BOGUS))
                {
                    ap->flags |= AF_EXISTS;
                }
            }
        }
    }
    else
    {
        namespace fs = std::filesystem;
        ArticleNum first = g_lastart+1;
        ArticleNum last = 0;
        fs::path cwd(".");
        char ch;
        long lnum;

        fs::directory_iterator entries(cwd);
        if (fs::directory_iterator() == entries)
        {
            return;
        }

        /* Scan the directory to find which articles are present. */
        for (ap = article_ptr(article_first(g_absfirst));
             ap && article_num(ap) <= g_lastart;
             ap = article_nextp(ap))
        {
            ap->flags &= ~AF_EXISTS;
        }

        for (const fs::directory_entry &entry : entries)
        {
            std::string filename{entry.path().filename().string()};
            if (std::sscanf(filename.c_str(), "%ld%c", &lnum, &ch) == 1)
            {
                an = (ArticleNum)lnum;
                if (an <= g_lastart && an >= g_absfirst)
                {
                    first = std::min(an, first);
                    last = std::max(an, last);
                    ap = article_ptr(an);
                    if (!(ap->flags2 & AF2_BOGUS))
                    {
                        ap->flags |= AF_EXISTS;
                    }
                }
            }
        }

        g_ngptr->abs1st = first;
        g_ngptr->ngmax = last;

        if (first > g_absfirst)
        {
            checkexpired(g_ngptr,first);
            for (g_absfirst = article_first(g_absfirst);
                 g_absfirst < first;
                 g_absfirst = article_next(g_absfirst))
            {
                onemissing(article_ptr(g_absfirst));
            }
            g_absfirst = first;
        }
        g_lastart = last;
    }

    g_firstart = std::max(g_firstart, g_absfirst);
    if (g_firstart > g_lastart)
    {
        g_firstart = g_lastart + 1;
    }
    g_first_cached = std::max(g_first_cached, g_absfirst);
    if (g_last_cached < g_absfirst)
    {
        g_last_cached = g_absfirst - 1;
    }
}

/* mark an article unread, keeping track of toread[] */

void onemore(Article *ap)
{
    if (!(ap->flags & AF_UNREAD))
    {
        ArticleNum artnum = article_num(ap);
        check_first(artnum);
        ap->flags |= AF_UNREAD;
        ap->flags &= ~AF_DEL;
        g_ngptr->toread++;
        if (ap->subj)
        {
            if (g_selected_only)
            {
                if (ap->subj->flags & static_cast<SubjectFlags>(g_sel_mask))
                {
                    ap->flags |= static_cast<ArticleFlags>(g_sel_mask);
                    g_selected_count++;
                }
            }
            else
            {
                ap->subj->flags |= SF_VISIT;
            }
        }
    }
}

/* mark an article read, keeping track of toread[] */

void oneless(Article *ap)
{
    if (ap->flags & AF_UNREAD)
    {
        ap->flags &= ~AF_UNREAD;
        /* Keep g_selected_count accurate */
        if (ap->flags & static_cast<ArticleFlags>(g_sel_mask))
        {
            g_selected_count--;
            ap->flags &= ~static_cast<ArticleFlags>(g_sel_mask);
        }
        if (g_ngptr->toread > TR_NONE)
        {
            g_ngptr->toread--;
        }
    }
}

void oneless_artnum(ArticleNum artnum)
{
    Article* ap = article_find(artnum);
    if (ap)
    {
        oneless(ap);
    }
}

void onemissing(Article *ap)
{
    g_missing_count += (ap->flags & AF_UNREAD) != 0;
    oneless(ap);
    ap->flags = (ap->flags & ~(AF_HAS_RE|AF_YANKBACK|AF_EXISTS))
              | AF_CACHED|AF_THREADED;
}

/* mark an article as unread, with possible xref chasing */

void unmark_as_read(Article *ap)
{
    onemore(ap);
#ifdef MCHASE
    if (!empty(ap->xrefs) && !(ap->flags & AF_MCHASE))
    {
        ap->flags |= AF_MCHASE;
        s_chase_count++;
    }
#endif
}

/* Mark an article as read in this newsgroup and possibly chase xrefs.
** Don't call this on missing articles.
*/
void set_read(Article *ap)
{
    oneless(ap);
    if (!g_olden_days && !empty(ap->xrefs) && !(ap->flags & AF_KCHASE))
    {
        ap->flags |= AF_KCHASE;
        s_chase_count++;
    }
}

/* temporarily mark article as read.  When newsgroup is exited, articles */
/* will be marked as unread.  Called via M command */

void delay_unmark(Article *ap)
{
    if (!(ap->flags & AF_YANKBACK))
    {
        ap->flags |= AF_YANKBACK;
        g_dmcount++;
    }
}

/* mark article as read.  If article is cross referenced to other */
/* newsgroups, mark them read there also. */

void mark_as_read(Article *ap)
{
    oneless(ap);
    if (!empty(ap->xrefs) && !(ap->flags & AF_KCHASE))
    {
        ap->flags |= AF_KCHASE;
        s_chase_count++;
    }
    g_checkcount++;             /* get more worried about crashes */
}

void mark_missing_articles()
{
    for (Article *ap = article_ptr(article_first(g_absfirst));
         ap && article_num(ap) <= g_lastart;
         ap = article_nextp(ap))
    {
        if (!(ap->flags & AF_EXISTS))
        {
            onemissing(ap);
        }
    }
}

/* keep g_firstart pointing at the first unread article */

void check_first(ArticleNum min)
{
    min = std::max(min, g_absfirst);
    g_firstart = std::min(min, g_firstart);
}

/* bring back articles marked with M */
void yankback()
{
    if (g_dmcount)                      /* delayed unmarks pending? */
    {
        if (g_panic)
        {
        }
        else if (g_general_mode == GM_SELECTOR)
        {
            std::sprintf(g_msg, "Returned %ld Marked article%s.", (long) g_dmcount, plural(g_dmcount));
        }
        else
        {
            std::printf("\nReturning %ld Marked article%s...\n",(long)g_dmcount,
                plural(g_dmcount));
            termdown(2);
        }
        article_walk(yank_article, 0);
        g_dmcount = 0;
    }
}

static bool yank_article(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if (ap->flags & AF_YANKBACK)
    {
        unmark_as_read(ap);
        if (g_selected_only)
        {
            select_article(ap, AUTO_KILL_NONE);
        }
        ap->flags &= ~AF_YANKBACK;
    }
    return false;
}

bool chase_xrefs(bool until_key)
{
    if (!s_chase_count)
    {
        return true;
    }
    if (until_key)
    {
        setspin(SPIN_BACKGROUND);
    }

    article_walk(check_chase, until_key);
    s_chase_count = 0;
    return true;
}

static bool check_chase(char *ptr, int until_key)
{
    Article* ap = (Article*)ptr;

    if (ap->flags & AF_KCHASE)
    {
        chase_xref(article_num(ap),true);
        ap->flags &= ~AF_KCHASE;
        if (!--s_chase_count)
        {
            return true;
        }
    }
#ifdef MCHASE
    if (ap->flags & AF_MCHASE)
    {
        chase_xref(article_num(ap),true);
        ap->flags &= ~AF_MCHASE;
        if (!--s_chase_count)
        {
            return 1;
        }
    }
#endif
    if (until_key && input_pending())
    {
        return true;
    }
    return false;
}

/* run down xref list and mark as read or unread */

/* The Xref-line-using version */
static int chase_xref(ArticleNum artnum, int markread)
{
    char* xartnum;
    ArticleNum x;
    char *curxref;
    char tmpbuf[128];

    if (g_datasrc->flags & DF_NOXREFS)
    {
        return 0;
    }

    if (inbackground())
    {
        spin(10);
    }
    else
    {
        if (g_output_chase_phrase)
        {
            if (g_verbose)
            {
                std::fputs("\nChasing xrefs", stdout);
            }
            else
            {
                std::fputs("\nXrefs", stdout);
            }
            termdown(1);
            g_output_chase_phrase = false;
        }
        std::putchar('.');
        std::fflush(stdout);
    }

    char *xref_buf = fetchcache(artnum, XREF_LINE, FILL_CACHE);
    if (!xref_buf || !*xref_buf)
    {
        return 0;
    }

    xref_buf = savestr(xref_buf);
# ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("Xref: %s\n",xref_buf);
        termdown(1);
    }
# endif
    curxref = cpytill(tmpbuf,xref_buf,' ') + 1;
# ifdef VALIDATE_XREF_SITE
    if (valid_xref_site(artnum,tmpbuf))
# endif
    {
        while (*curxref)            /* for each newsgroup */
        {
            curxref = cpytill(tmpbuf,curxref,' ');
            xartnum = std::strchr(tmpbuf,':');
            if (!xartnum)
            {
                break;
            }
            *xartnum++ = '\0';
            if (!(x = std::atol(xartnum)))
            {
                continue;
            }
            if (!std::strcmp(tmpbuf,g_ngname.c_str()))  /* is this the current newsgroup? */
            {
                if (x < g_absfirst || x > g_lastart)
                {
                    continue;
                }
                if (markread)
                {
                    oneless(article_ptr(x)); /* take care of old C newses */
                }
# ifdef MCHASE
                else
                {
                    onemore(article_ptr(x));
                }
# endif
            }
            else
            {
                if (markread)
                {
                    if (addartnum(g_datasrc,x,tmpbuf))
                    {
                        break;
                    }
                }
# ifdef MCHASE
                else
                {
                    subartnum(g_datasrc,x,tmpbuf);
                }
# endif
            }
            curxref = skip_space(curxref);
        }
    }
    std::free(xref_buf);
    return 0;
}

/* Make sure the site name on Xref matches what inews thinks the site
 * is.  Check first against last inews_site.  If it matches, fine.
 * If not, fetch inews_site from current Path or Relay-Version line and
 * check again.  This is so that if the new administrator decides
 * to change the system name as known to inews, rn will still do
 * Xrefs correctly--each article need only match itself to be valid.
 */
# ifdef VALIDATE_XREF_SITE
static bool valid_xref_site(ART_NUM artnum, char *site)
{
    static char* inews_site = nullptr;
    char* sitebuf;
    char* s;

    if (inews_site && !strcmp(site,inews_site))
        return true;

    if (inews_site)
        std::free(inews_site);
#ifndef ANCIENT_NEWS
    /* Grab the site from the first component of the Path line */
    sitebuf = fetchlines(artnum,PATH_LINE);
    s = std::strchr(sitebuf, '!');
    if (s != nullptr)
    {
        *s = '\0';
        inews_site = savestr(sitebuf);
    }
#else /* ANCIENT_NEWS */
    /* Grab the site from the Posting-Version line */
    sitebuf = fetchlines(artnum,RVER_LINE);
    s = instr(sitebuf, "; site ", true);
    if (s != nullptr)
    {
        char* t = std::strchr(s+7, '.');
        if (t)
        {
            *t = '\0';
        }
        inews_site = savestr(s+7);
    }
#endif /* ANCIENT_NEWS */
    else
    {
        inews_site = savestr("");
    }
    std::free(sitebuf);

    if (std::!strcmp(site,inews_site))
    {
        return true;
    }

#ifdef DEBUG
    if (debug)
    {
        std::printf("Xref not from %s -- ignoring\n",inews_site);
        termdown(1);
    }
#endif
    return false;
}
# endif /* VALIDATE_XREF_SITE */
