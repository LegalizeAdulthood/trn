/* bits.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"

#include "trn/bits.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
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

int g_dm_count{};

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

    char *s = skip_eq(g_newsgroup_ptr->rc_line + g_newsgroup_ptr->num_offset, ' ');
                                        /* find numbers in rc line */
    long i = std::strlen(s);
#ifndef lint
    if (i >= LINE_BUF_LEN-2)                 /* bigger than g_buf? */
    {
        mybuf = safe_malloc((MemorySize) (i + 2));
    }
#endif
    std::strcpy(mybuf,s);                    /* make scratch copy of line */
    if (mybuf[0])
    {
        mybuf[i++] = ',';               /* put extra comma on the end */
    }
    mybuf[i] = '\0';
    s = mybuf;                          /* initialize the for loop below */
    if (set_first_art(s))
    {
        s = std::strchr(s,',') + 1;
        for (i = article_first(g_abs_first); i < g_first_art; i = article_next(i))
        {
            article_ptr(i)->flags &= ~AF_UNREAD;
        }
        g_first_art = i;
    }
    else
    {
        g_first_art = article_first(g_first_art);
    }
    unread = 0;
#ifdef DEBUG
    if (debug & DEB_CTLAREA_BITMAP)
    {
        std::printf("\n%s\n",mybuf);
        term_down(2);
        for (i = article_first(g_absfirst); i < g_firstart; i = article_next(i))
        {
            if (article_unread(i))
            {
                std::printf("%ld ",(long)i);
            }
        }
    }
#endif
    i = g_first_art;
    for ( ; (c = std::strchr(s,',')) != nullptr; s = ++c)    /* for each range */
    {
        ArticleNum max;
        *c = '\0';                      /* do not let index see past comma */
        h = std::strchr(s,'-');
        ArticleNum min = std::atol(s);
        min = std::max(min, g_first_art);    /* make sure range is in range */
        if (min > g_last_art)
        {
            min = g_last_art + 1;
        }
        for (; i < min; i = article_next(i))
        {
            ap = article_ptr(i);
            if (ap->flags & AF_EXISTS)
            {
                if (ap->auto_flags & AUTO_KILL_MASK)
                {
                    ap->flags &= ~AF_UNREAD;
                }
                else
                {
                    ap->flags |= AF_UNREAD;
                    unread++;
                    if (ap->auto_flags & AUTO_SEL_MASK)
                    {
                        select_article(ap, ap->auto_flags);
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
        max = std::min(max, g_last_art);
        /* mark all arts in range as read */
        for ( ; i <= max; i = article_next(i))
        {
            article_ptr(i)->flags &= ~AF_UNREAD;
        }
#ifdef DEBUG
        if (debug & DEB_CTLAREA_BITMAP)
        {
            std::printf("\n%s\n",s);
            term_down(2);
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
    for (; i <= g_last_art; i = article_next(i))
    {
        ap = article_ptr(i);
        if (ap->flags & AF_EXISTS)
        {
            if (ap->auto_flags & AUTO_KILL_MASK)
            {
                ap->flags &= ~AF_UNREAD;
            }
            else
            {
                ap->flags |= AF_UNREAD;
                unread++;
                if (ap->auto_flags & AUTO_SEL_MASK)
                {
                    select_article(ap, ap->auto_flags);
                }
            }
        }
    }
#ifdef DEBUG
    if (debug & DEB_CTLAREA_BITMAP)
    {
        std::fputs("\n(hit CR)",stdout);
        term_down(1);
        std::fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    if (mybuf != g_buf)
    {
        std::free(mybuf);
    }
    g_newsgroup_ptr->to_read = unread;
}

bool set_first_art(const char *s)
{
    s = skip_eq(s, ' ');
    if (!std::strncmp(s,"1-",2))                     /* can we save some time here? */
    {
        g_first_art = std::atol(s+2)+1;               /* process first range thusly */
        g_first_art = std::max(g_first_art, g_abs_first);
        return true;
    }

    g_first_art = g_abs_first;
    return false;
}

/* reconstruct the .newsrc line in a human readable form */

void bits_to_rc()
{
    char* mybuf = g_buf;
    ArticleNum i;
    ArticleNum count=0;
    int safelen = LINE_BUF_LEN - 32;

    std::strcpy(g_buf,g_newsgroup_ptr->rc_line);            /* start with the newsgroup name */
    char *s = g_buf + g_newsgroup_ptr->num_offset - 1; /* use s for buffer pointer */
    *s++ = g_newsgroup_ptr->subscribe_char;            /* put the requisite : or !*/
    for (i = article_first(g_abs_first); i <= g_last_art; i = article_next(i))
    {
        if (article_unread(i))
        {
            break;
        }
    }
    std::sprintf(s," 1-%ld,",(long)i-1);
    s += std::strlen(s);
    for (; i<=g_last_art; i++)   /* for each article in newsgroup */
    {
        if (s-mybuf > safelen)          /* running out of room? */
        {
            safelen *= 2;
            if (mybuf == g_buf)         /* currently static? */
            {
                *s = '\0';
                mybuf = safe_malloc((MemorySize)safelen + 32);
                std::strcpy(mybuf,g_buf);    /* so we must copy it */
                s = mybuf + (s-g_buf);
                                        /* fix the pointer, too */
            }
            else                        /* just grow in place, if possible */
            {
                int oldlen = s - mybuf;
                mybuf = safe_realloc(mybuf,(MemorySize)safelen + 32);
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
            } while (i <= g_last_art && was_read(i));
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
        term_down(2);
    }
#endif
    std::free(g_newsgroup_ptr->rc_line);              /* return old rc line */
    if (mybuf == g_buf)
    {
        g_newsgroup_ptr->rc_line = safe_malloc((MemorySize)(s-g_buf)+1);
                                        /* grab a new rc line */
        std::strcpy(g_newsgroup_ptr->rc_line, g_buf); /* and load it */
    }
    else
    {
        mybuf = safe_realloc(mybuf,(MemorySize)(s-mybuf)+1);
                                        /* be nice to the heap */
        g_newsgroup_ptr->rc_line = mybuf;
    }
    *(g_newsgroup_ptr->rc_line + g_newsgroup_ptr->num_offset - 1) = '\0';
    if (g_newsgroup_ptr->subscribe_char == NEGCHAR)/* did they unsubscribe? */
    {
        g_newsgroup_ptr->to_read = TR_UNSUB;     /* make line invisible */
    }
    else
    {
        g_newsgroup_ptr->to_read = (ArticleUnread)count; /* otherwise, remember the count */
    }
    g_newsgroup_ptr->rc->flags |= RF_RC_CHANGED;
}

void find_existing_articles()
{
    ArticleNum an;
    Article* ap;

    if (g_data_source->flags & DF_REMOTE)
    {
        /* Parse the LISTGROUP output and remember everything we find */
        if (nntp_art_nums())
        {
            for (ap = article_ptr(article_first(g_abs_first));
                 ap && article_num(ap) <= g_last_art;
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
                if (an < g_abs_first)
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
            if (!g_data_source->ov_opened || g_data_source->over_dir != nullptr)
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
            for (an = g_abs_first; an < g_first_cached; an++)
            {
                ap = article_ptr(an);
                if (!(ap->flags2 & AF2_BOGUS))
                {
                    ap->flags |= AF_EXISTS;
                }
            }
            for (an = g_last_cached + 1; an <= g_last_art; an++)
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
            for (an = g_abs_first; an <= g_last_art; an++)
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
        ArticleNum first = g_last_art+1;
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
        for (ap = article_ptr(article_first(g_abs_first));
             ap && article_num(ap) <= g_last_art;
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
                if (an <= g_last_art && an >= g_abs_first)
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

        g_newsgroup_ptr->abs_first = first;
        g_newsgroup_ptr->ng_max = last;

        if (first > g_abs_first)
        {
            check_expired(g_newsgroup_ptr,first);
            for (g_abs_first = article_first(g_abs_first);
                 g_abs_first < first;
                 g_abs_first = article_next(g_abs_first))
            {
                one_missing(article_ptr(g_abs_first));
            }
            g_abs_first = first;
        }
        g_last_art = last;
    }

    g_first_art = std::max(g_first_art, g_abs_first);
    if (g_first_art > g_last_art)
    {
        g_first_art = g_last_art + 1;
    }
    g_first_cached = std::max(g_first_cached, g_abs_first);
    if (g_last_cached < g_abs_first)
    {
        g_last_cached = g_abs_first - 1;
    }
}

/* mark an article unread, keeping track of toread[] */

void one_more(Article *ap)
{
    if (!(ap->flags & AF_UNREAD))
    {
        ArticleNum artnum = article_num(ap);
        check_first(artnum);
        ap->flags |= AF_UNREAD;
        ap->flags &= ~AF_DEL;
        g_newsgroup_ptr->to_read++;
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

void one_less(Article *ap)
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
        if (g_newsgroup_ptr->to_read > TR_NONE)
        {
            g_newsgroup_ptr->to_read--;
        }
    }
}

void one_less_art_num(ArticleNum art_num)
{
    Article* ap = article_find(art_num);
    if (ap)
    {
        one_less(ap);
    }
}

void one_missing(Article *ap)
{
    g_missing_count += (ap->flags & AF_UNREAD) != 0;
    one_less(ap);
    ap->flags = (ap->flags & ~(AF_HAS_RE|AF_YANK_BACK|AF_EXISTS))
              | AF_CACHED|AF_THREADED;
}

/* mark an article as unread, with possible xref chasing */

void unmark_as_read(Article *ap)
{
    one_more(ap);
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
    one_less(ap);
    if (!g_olden_days && !empty(ap->xrefs) && !(ap->flags & AF_K_CHASE))
    {
        ap->flags |= AF_K_CHASE;
        s_chase_count++;
    }
}

/* temporarily mark article as read.  When newsgroup is exited, articles */
/* will be marked as unread.  Called via M command */

void delay_unmark(Article *ap)
{
    if (!(ap->flags & AF_YANK_BACK))
    {
        ap->flags |= AF_YANK_BACK;
        g_dm_count++;
    }
}

/* mark article as read.  If article is cross referenced to other */
/* newsgroups, mark them read there also. */

void mark_as_read(Article *ap)
{
    one_less(ap);
    if (!empty(ap->xrefs) && !(ap->flags & AF_K_CHASE))
    {
        ap->flags |= AF_K_CHASE;
        s_chase_count++;
    }
    g_check_count++;             /* get more worried about crashes */
}

void mark_missing_articles()
{
    for (Article *ap = article_ptr(article_first(g_abs_first));
         ap && article_num(ap) <= g_last_art;
         ap = article_nextp(ap))
    {
        if (!(ap->flags & AF_EXISTS))
        {
            one_missing(ap);
        }
    }
}

/* keep g_firstart pointing at the first unread article */

void check_first(ArticleNum min)
{
    min = std::max(min, g_abs_first);
    g_first_art = std::min(min, g_first_art);
}

/* bring back articles marked with M */
void yank_back()
{
    if (g_dm_count)                      /* delayed unmarks pending? */
    {
        if (g_panic)
        {
        }
        else if (g_general_mode == GM_SELECTOR)
        {
            std::sprintf(g_msg, "Returned %ld Marked article%s.", (long) g_dm_count, plural(g_dm_count));
        }
        else
        {
            std::printf("\nReturning %ld Marked article%s...\n",(long)g_dm_count,
                plural(g_dm_count));
            term_down(2);
        }
        article_walk(yank_article, 0);
        g_dm_count = 0;
    }
}

static bool yank_article(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if (ap->flags & AF_YANK_BACK)
    {
        unmark_as_read(ap);
        if (g_selected_only)
        {
            select_article(ap, AUTO_KILL_NONE);
        }
        ap->flags &= ~AF_YANK_BACK;
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
        set_spin(SPIN_BACKGROUND);
    }

    article_walk(check_chase, until_key);
    s_chase_count = 0;
    return true;
}

static bool check_chase(char *ptr, int until_key)
{
    Article* ap = (Article*)ptr;

    if (ap->flags & AF_K_CHASE)
    {
        chase_xref(article_num(ap),true);
        ap->flags &= ~AF_K_CHASE;
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

    if (g_data_source->flags & DF_NO_XREFS)
    {
        return 0;
    }

    if (in_background())
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
            term_down(1);
            g_output_chase_phrase = false;
        }
        std::putchar('.');
        std::fflush(stdout);
    }

    char *xref_buf = fetch_cache(artnum, XREF_LINE, FILL_CACHE);
    if (!xref_buf || !*xref_buf)
    {
        return 0;
    }

    xref_buf = save_str(xref_buf);
# ifdef DEBUG
    if (debug & DEB_XREF_MARKER)
    {
        std::printf("Xref: %s\n",xref_buf);
        term_down(1);
    }
# endif
    curxref = copy_till(tmpbuf,xref_buf,' ') + 1;
# ifdef VALIDATE_XREF_SITE
    if (valid_xref_site(artnum,tmpbuf))
# endif
    {
        while (*curxref)            /* for each newsgroup */
        {
            curxref = copy_till(tmpbuf,curxref,' ');
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
            if (!std::strcmp(tmpbuf,g_newsgroup_name.c_str()))  /* is this the current newsgroup? */
            {
                if (x < g_abs_first || x > g_last_art)
                {
                    continue;
                }
                if (markread)
                {
                    one_less(article_ptr(x)); /* take care of old C newses */
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
                    if (add_art_num(g_data_source,x,tmpbuf))
                    {
                        break;
                    }
                }
# ifdef MCHASE
                else
                {
                    sub_art_num(g_datasrc,x,tmpbuf);
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
        inews_site = save_str(sitebuf);
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
        inews_site = save_str(s+7);
    }
#endif /* ANCIENT_NEWS */
    else
    {
        inews_site = save_str("");
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
        term_down(1);
    }
#endif
    return false;
}
# endif /* VALIDATE_XREF_SITE */
