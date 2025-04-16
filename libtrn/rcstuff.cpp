/* rcstuff.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/rcstuff.h"

#include "trn/autosub.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "util/env.h"
#include "trn/final.h"
#include "trn/hash.h"
#include "trn/init.h"
#include "trn/last.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/nntp.h"
#include "nntp/nntpclient.h"
#include "trn/only.h"
#include "trn/rcln.h"
#include "trn/rt-page.h"
#include "trn/rt-select.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

HASHTABLE  *g_newsrc_hash{};
MULTIRC    *g_sel_page_mp{};
MULTIRC    *g_sel_next_mp{};
LIST       *g_multirc_list{};    /* a list of all MULTIRCs */
MULTIRC    *g_multirc{};         /* the current MULTIRC */
bool        g_paranoid{};        /* did we detect some inconsistency in .newsrc? */
addnew_type g_addnewbydefault{ADDNEW_ASK}; //
bool        g_checkflag{};       /* -c */
bool        g_suppress_cn{};     /* -s */
int         g_countdown{5};      /* how many lines to list before invoking -s */
bool        g_fuzzy_get{};       /* -G */
bool        g_append_unsub{};    /* -I */

enum
{
    RI_ID = 1,
    RI_NEWSRC = 2,
    RI_ADDGROUPS = 3
};

static INI_WORDS s_rcgroups_ini[] = {
    { 0, "RCGROUPS", nullptr },
    { 0, "ID", nullptr },
    { 0, "Newsrc", nullptr },
    { 0, "Add Groups", nullptr },
    { 0, nullptr, nullptr }
};
static bool        s_foundany{};
static const char *s_cantrecreate{"Can't recreate %s -- restoring older version.\n"
                                  "Perhaps you are near or over quota?\n"};

static bool    clear_ngitem(char *cp, int arg);
static bool    lock_newsrc(NEWSRC *rp);
static void    unlock_newsrc(NEWSRC *rp);
static bool    open_newsrc(NEWSRC *rp);
static void    init_ngnode(LIST *list, LISTNODE *node);
static void    parse_rcline(NGDATA *np);
static NGDATA *add_newsgroup(NEWSRC *rp, const char *ngn, char_int c);
static int     rcline_cmp(const char *key, int keylen, HASHDATUM data);

inline NGDATA *ngdata_ptr(int ngnum)
{
    return (NGDATA *) listnum2listitem(g_ngdata_list, ngnum);
}

static MULTIRC *rcstuff_init_data()
{
    MULTIRC* mptr = nullptr;

    g_multirc_list = new_list(0,0,sizeof(MULTIRC),20,LF_ZERO_MEM|LF_SPARSE,nullptr);

    if (g_trnaccess_mem)
    {
        char* section;
        char* cond;
        char**vals = prep_ini_words(s_rcgroups_ini);
        char *s = g_trnaccess_mem;
        while ((s = next_ini_section(s, &section, &cond)) != nullptr)
        {
            if (*cond && !check_ini_cond(cond))
            {
                continue;
            }
            if (string_case_compare(section, "group ", 6))
            {
                continue;
            }
            int i = std::atoi(section + 6);
            i = std::max(i, 0);
            s = parse_ini_section(s, s_rcgroups_ini);
            if (!s)
            {
                break;
            }
            NEWSRC *rp = new_newsrc(vals[RI_ID], vals[RI_NEWSRC], vals[RI_ADDGROUPS]);
            if (rp)
            {
                MULTIRC *mp = multirc_ptr(i);
                NEWSRC * prev_rp = mp->first;
                if (!prev_rp)
                {
                    mp->first = rp;
                }
                else
                {
                    while (prev_rp->next)
                    {
                        prev_rp = prev_rp->next;
                    }
                    prev_rp->next = rp;
                }
                mp->num = i;
                if (!mptr)
                {
                    mptr = mp;
                }
            }
            /*else
                ;$$complain?*/
        }
        std::free(vals);
        safefree0(g_trnaccess_mem);
    }
    return mptr;
}

bool rcstuff_init()
{
    MULTIRC *mptr = rcstuff_init_data();

    if (g_use_newsrc_selector && !g_checkflag)
    {
        return true;
    }

    s_foundany = false;
    if (mptr && !use_multirc(mptr))
    {
        use_next_multirc(mptr);
    }
    if (!g_multirc)
    {
        mptr = multirc_ptr(0);
        mptr->first = new_newsrc("default",nullptr,nullptr);
        if (!use_multirc(mptr))
        {
            std::printf("Couldn't open any newsrc groups.  Is your access file ok?\n");
            finalize(1);
        }
    }
    if (g_checkflag)                    /* were we just checking? */
    {
        finalize(s_foundany);           /* tell them what we found */
    }
    return s_foundany;
}

void rcstuff_final()
{
    if (g_multirc)
    {
        unuse_multirc(g_multirc);
        g_multirc = nullptr;
    }
    if (g_multirc_list)
    {
        delete_list(g_multirc_list);
        g_multirc_list = nullptr;
    }
    s_rcgroups_ini[0].checksum = 0;
    s_rcgroups_ini[0].help_str = nullptr;
}

NEWSRC *new_newsrc(const char *name, const char *newsrc, const char *add_ok)
{
    if (!name || !*name)
    {
        return nullptr;
    }

    if (!newsrc || !*newsrc)
    {
        newsrc = get_val_const("NEWSRC");
        if (!newsrc)
        {
            newsrc = RCNAME;
        }
    }

    DATASRC *dp = get_datasrc(name);
    if (!dp)
    {
        return nullptr;
    }

    NEWSRC *rp = (NEWSRC*)safemalloc(sizeof(NEWSRC));
    std::memset((char*)rp,0,sizeof (NEWSRC));
    rp->datasrc = dp;
    rp->name = savestr(filexp(newsrc));
    char tmpbuf[CBUFLEN];
    std::sprintf(tmpbuf, RCNAME_OLD, rp->name);
    rp->oldname = savestr(tmpbuf);
    std::sprintf(tmpbuf, RCNAME_NEW, rp->name);
    rp->newname = savestr(tmpbuf);

    switch (add_ok ? *add_ok : 'y')
    {
    case 'n':
    case 'N':
        break;
    default:
        if (dp->flags & DF_ADD_OK)
        {
            rp->flags |= RF_ADD_NEWGROUPS;
        }
        /* FALL THROUGH */
    case 'm':
    case 'M':
        rp->flags |= RF_ADD_GROUPS;
        break;
    }
    return rp;
}

bool use_multirc(MULTIRC *mp)
{
    bool had_trouble = false;
    bool had_success = false;

    for (NEWSRC *rp = mp->first; rp; rp = rp->next)
    {
        if ((rp->datasrc->flags & DF_UNAVAILABLE) || !lock_newsrc(rp) //
            || !open_datasrc(rp->datasrc) || !open_newsrc(rp))
        {
            unlock_newsrc(rp);
            had_trouble = true;
        }
        else
        {
            rp->datasrc->flags |= DF_ACTIVE;
            rp->flags |= RF_ACTIVE;
            had_success = true;
        }
    }
    if (had_trouble)
    {
        get_anything();
    }
    if (!had_success)
    {
        return false;
    }
    g_multirc = mp;
#ifdef NO_FILELINKS
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
#endif
    return true;
}

void unuse_multirc(MULTIRC *mptr)
{
    if (!mptr)
    {
        return;
    }

    write_newsrcs(mptr);

    for (NEWSRC *rp = mptr->first; rp; rp = rp->next)
    {
        unlock_newsrc(rp);
        rp->flags &= ~RF_ACTIVE;
        rp->datasrc->flags &= ~DF_ACTIVE;
    }
    if (g_ngdata_list)
    {
        close_cache();
        hashdestroy(g_newsrc_hash);
        walk_list(g_ngdata_list, clear_ngitem, 0);
        delete_list(g_ngdata_list);
        g_ngdata_list = nullptr;
        g_first_ng = nullptr;
        g_last_ng = nullptr;
        g_ngptr = nullptr;
        g_current_ng = nullptr;
        g_recent_ng = nullptr;
        g_starthere = nullptr;
        g_sel_page_np = nullptr;
    }
    g_ngdata_cnt = 0;
    g_newsgroup_cnt = 0;
    g_newsgroup_toread = 0;
    g_multirc = nullptr;
}

bool use_next_multirc(MULTIRC *mptr)
{
    MULTIRC* mp = multirc_ptr(mptr->num);

    unuse_multirc(mptr);

    for (;;)
    {
        mp = multirc_next(mp);
        if (!mp)
        {
            mp = multirc_low();
        }
        if (mp == mptr)
        {
            use_multirc(mptr);
            return false;
        }
        if (use_multirc(mp))
        {
            break;
        }
    }
    return true;
}

bool use_prev_multirc(MULTIRC *mptr)
{
    MULTIRC* mp = multirc_ptr(mptr->num);

    unuse_multirc(mptr);

    for (;;)
    {
        mp = multirc_prev(mp);
        if (!mp)
        {
            mp = multirc_high();
        }
        if (mp == mptr)
        {
            use_multirc(mptr);
            return false;
        }
        if (use_multirc(mp))
        {
            break;
        }
    }
    return true;
}

char *multirc_name(MULTIRC *mp)
{
    if (mp->first->next)
    {
        return "<each-newsrc>";
    }
    char *cp = std::strrchr(mp->first->name, '/');
    if (cp != nullptr)
    {
        return cp + 1;
    }
    return mp->first->name;
}

static bool clear_ngitem(char *cp, int arg)
{
    NGDATA* ncp = (NGDATA*)cp;

    if (ncp->rcline != nullptr)
    {
        if (!g_checkflag)
        {
            std::free(ncp->rcline);
        }
        ncp->rcline = nullptr;
    }
    return false;
}

/* make sure there is no trn out there reading this newsrc */

static bool lock_newsrc(NEWSRC *rp)
{
    long processnum = 0;

    if (g_checkflag)
    {
        return true;
    }

    char *s = filexp(RCNAME);
    if (!std::strcmp(rp->name,s))
    {
        rp->lockname = savestr(filexp(LOCKNAME));
    }
    else
    {
        std::sprintf(g_buf, RCNAME_INFO, rp->name);
        rp->infoname = savestr(g_buf);
        std::sprintf(g_buf, RCNAME_LOCK, rp->name);
        rp->lockname = savestr(g_buf);
    }

    char *runninghost;
    if (std::FILE *fp = std::fopen(rp->lockname, "r"))
    {
        if (std::fgets(g_buf, LBUFLEN, fp))
        {
            processnum = std::atol(g_buf);
            if (std::fgets(g_buf, LBUFLEN, fp) && *g_buf //
                && *(s = g_buf + std::strlen(g_buf) - 1) == '\n')
            {
                *s = '\0';
                runninghost = g_buf;
            }
        }
        std::fclose(fp);
    }
    if (processnum)
    {
#ifndef MSDOS
        if (g_verbose)
        {
            std::printf("\nThe requested newsrc is locked by process %ld on host %s.\n",
                   processnum, runninghost);
        }
        else
        {
            std::printf("\nNewsrc locked by %ld on host %s.\n",processnum,runninghost);
        }
        termdown(2);
        if (std::strcmp(runninghost, g_local_host))
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "Since that's not the same host as this one (%s), we must\n"
                       "assume that process still exists.  To override this check, remove\n"
                       "the lock file: %s\n",
                       g_local_host, rp->lockname);
            }
            else
            {
                std::printf("\nThis host (%s) doesn't match.\nCan't unlock %s.\n",
                       g_local_host, rp->lockname);
            }
            termdown(2);
            if (g_bizarre)
            {
                resetty();
            }
            finalize(0);
        }
        if (processnum == g_our_pid)
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "Hey, that *my* pid!  Your access file is trying to use the same newsrc\n"
                       "more than once.\n");
            }
            else
            {
                std::printf("\nAccess file error (our pid detected).\n");
            }
            termdown(2);
            return false;
        }
        if (kill(processnum, 0) != 0)
        {
            /* Process is apparently gone */
            sleep(2);
            if (g_verbose)
            {
                std::fputs("\n"
                      "That process does not seem to exist anymore.  The count of read articles\n"
                      "may be incorrect in the last newsgroup accessed by that other (defunct)\n"
                      "process.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\nProcess crashed.\n",stdout);
            }
            if (!g_lastngname.empty())
            {
                if (g_verbose)
                {
                    std::printf("(The last newsgroup accessed was %s.)\n\n",
                           g_lastngname.c_str());
                }
                else
                {
                    std::printf("(In %s.)\n\n",g_lastngname.c_str());
                }
            }
            termdown(2);
            get_anything();
            newline();
        }
        else
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "It looks like that process still exists.  To override this, remove\n"
                       "the lock file: %s\n",
                       rp->lockname);
            }
            else
            {
                std::printf("\nCan't unlock %s.\n", rp->lockname);
            }
            termdown(2);
            if (g_bizarre)
            {
                resetty();
            }
            finalize(0);
        }
#endif
    }
    std::FILE *fp = std::fopen(rp->lockname, "w");
    if (fp == nullptr)
    {
        std::printf(g_cantcreate,rp->lockname);
        sig_catcher(0);
    }
    std::fprintf(fp,"%ld\n%s\n",g_our_pid,g_local_host);
    std::fclose(fp);
    return true;
}

static void unlock_newsrc(NEWSRC *rp)
{
    safefree0(rp->infoname);
    if (rp->lockname)
    {
        remove(rp->lockname);
        std::free(rp->lockname);
        rp->lockname = nullptr;
    }
}

static bool open_newsrc(NEWSRC *rp)
{
    /* make sure the .newsrc file exists */

    std::FILE *rcfp = std::fopen(rp->name, "r");
    if (rcfp == nullptr)
    {
        rcfp = std::fopen(rp->name,"w+");
        if (rcfp == nullptr)
        {
            std::printf("\nCan't create %s.\n", rp->name);
            termdown(2);
            return false;
        }
        const char *some_buf = SUBSCRIPTIONS;
        if ((rp->datasrc->flags & DF_REMOTE) //
            && nntp_list("SUBSCRIPTIONS", "", 0) == 1)
        {
            do
            {
                std::fputs(g_ser_line,rcfp);
                std::fputc('\n',rcfp);
                if (nntp_gets(g_ser_line, sizeof g_ser_line) == NGSR_ERROR)
                {
                    break;
                }
            } while (!nntp_at_list_end(g_ser_line));
        }
        else if (*some_buf)
        {
            if (std::FILE *fp = std::fopen(filexp(some_buf), "r"))
            {
                while (std::fgets(g_buf, sizeof g_buf, fp))
                {
                    std::fputs(g_buf, rcfp);
                }
                std::fclose(fp);
            }
        }
        std::fseek(rcfp, 0L, 0);
    }
    else
    {
        /* File exists; if zero length and backup isn't, complain */
        stat_t newsrc_stat{};
        if (fstat(fileno(rcfp), &newsrc_stat) < 0)
        {
            std::perror(rp->name);
            return false;
        }
        if (newsrc_stat.st_size == 0 //
            && stat(rp->oldname, &newsrc_stat) >= 0 && newsrc_stat.st_size > 0)
        {
            std::printf("Warning: %s is zero length but %s is not.\n",
                   rp->name,rp->oldname);
            std::printf("Either recover your newsrc or else remove the backup copy.\n");
            termdown(2);
            return false;
        }
        /* unlink backup file name and backup current name */
        remove(rp->oldname);
#ifndef NO_FILELINKS
        safelink(rp->name,rp->oldname);
#endif
    }

    if (!g_ngdata_list)
    {
        /* allocate memory for rc file globals */
        g_ngdata_list = new_list(0, 0, sizeof (NGDATA), 200, LF_NONE, init_ngnode);
        g_newsrc_hash = hashcreate(3001, rcline_cmp);
    }

    NGDATA*   prev_np;
    if (g_ngdata_cnt)
    {
        prev_np = ngdata_ptr(g_ngdata_cnt - 1);
    }
    else
    {
        prev_np = nullptr;
    }

    /* read in the .newsrc file */

    char* some_buf;
    while ((some_buf = get_a_line(g_buf, LBUFLEN, false, rcfp)) != nullptr)
    {
        long length = g_len_last_line_got; /* side effect of get_a_line */
        if (length <= 1)                   /* only a newline??? */
        {
            continue;
        }
        NGDATA *np = ngdata_ptr(g_ngdata_cnt++);
        if (prev_np)
        {
            prev_np->next = np;
        }
        else
        {
            g_first_ng = np;
        }
        np->prev = prev_np;
        prev_np = np;
        np->rc = rp;
        g_newsgroup_cnt++;
        if (some_buf[length-1] == '\n')
        {
            some_buf[--length] = '\0';  /* wipe out newline */
        }
        if (some_buf == g_buf)
        {
            np->rcline = savestr(some_buf);  /* make semipermanent copy */
        }
        else
        {
            /*NOSTRICT*/
#ifndef lint
            some_buf = saferealloc(some_buf,(MEM_SIZE)(length+1));
#endif
            np->rcline = some_buf;
        }
        if (is_hor_space(*some_buf)                   //
            || !std::strncmp(some_buf, "options", 7)) /* non-useful line? */
        {
            np->toread = TR_JUNK;
            np->subscribechar = ' ';
            np->numoffset = 0;
            continue;
        }
        parse_rcline(np);
        HASHDATUM data = hashfetch(g_newsrc_hash, np->rcline, np->numoffset - 1);
        if (data.dat_ptr)
        {
            np->toread = TR_IGNORE;
            continue;
        }
        if (np->subscribechar == NEGCHAR)
        {
            np->toread = TR_UNSUB;
            sethash(np);
            continue;
        }
        g_newsgroup_toread++;

        /* now find out how much there is to read */

        if (!inlist(g_buf) || (g_suppress_cn && s_foundany && !g_paranoid))
        {
            np->toread = TR_NONE;       /* no need to calculate now */
        }
        else
        {
            set_toread(np, ST_LAX);
        }
        if (np->toread > TR_NONE)       /* anything unread? */
        {
            if (!s_foundany)
            {
                g_starthere = np;
                s_foundany = true;      /* remember that fact*/
            }
            if (g_suppress_cn)          /* if no listing desired */
            {
                if (g_checkflag)        /* if that is all they wanted */
                {
                    finalize(1);        /* then bomb out */
                }
            }
            else
            {
                if (g_verbose)
                {
                    std::printf("Unread news in %-40s %5ld article%s\n",
                        np->rcline,(long)np->toread,plural(np->toread));
                }
                else
                {
                    std::printf("%s: %ld article%s\n",
                        np->rcline,(long)np->toread,plural(np->toread));
                }
                termdown(1);
                if (g_int_count)
                {
                    g_countdown = 1;
                    g_int_count = 0;
                }
                if (g_countdown)
                {
                    if (!--g_countdown)
                    {
                        std::fputs("etc.\n",stdout);
                        if (g_checkflag)
                        {
                            finalize(1);
                        }
                        g_suppress_cn = true;
                    }
                }
            }
        }
        sethash(np);
    }
    if (prev_np)
    {
        prev_np->next = nullptr;
        g_last_ng = prev_np;
    }
    std::fclose(rcfp);                       /* close .newsrc */
#ifdef NO_FILELINKS
    remove(rp->oldname);
    rename(rp->name,rp->oldname);
    rp->flags |= RF_RCCHANGED;
#endif
    if (rp->infoname)
    {
        std::FILE *info = std::fopen(rp->infoname, "r");
        if (info != nullptr)
        {
            if (std::fgets(g_buf, sizeof g_buf, info) != nullptr)
            {
                long actnum;
                long descnum;
                g_buf[std::strlen(g_buf)-1] = '\0';
                char *s = std::strchr(g_buf, ':');
                if (s != nullptr && s[1] == ' ' && s[2])
                {
                    g_lastngname = s+2;
                }
                if (std::fscanf(info, "New-Group-State: %ld,%ld,%ld", //
                                &g_lastnewtime, &actnum, &descnum) == 3)
                {
                    rp->datasrc->act_sf.recent_cnt = actnum;
                    rp->datasrc->desc_sf.recent_cnt = descnum;
                }
            }
            std::fclose(info);
        }
    }
    else
    {
        readlast();
        if (rp->datasrc->flags & DF_REMOTE)
        {
            rp->datasrc->act_sf.recent_cnt = g_lastactsiz;
            rp->datasrc->desc_sf.recent_cnt = g_lastextranum;
        }
        else
        {
            rp->datasrc->act_sf.recent_cnt = g_lastextranum;
            rp->datasrc->desc_sf.recent_cnt = 0;
        }
    }
    rp->datasrc->lastnewgrp = g_lastnewtime;

    if (g_paranoid && !g_checkflag)
    {
        cleanup_newsrc(rp);
    }
    return true;
}

/* Initialize the memory for an entire node's worth of article's */
static void init_ngnode(LIST *list, LISTNODE *node)
{
    std::memset(node->data,0,list->items_per_node * list->item_size);
    NGDATA *np = (NGDATA*)node->data;
    for (ART_NUM i = node->low; i <= node->high; i++, np++)
    {
        np->num = i;
    }
}

static void parse_rcline(NGDATA *np)
{
    char* s;

    for (s=np->rcline; *s && *s!=':' && *s!=NEGCHAR && !std::isspace(*s); s++)
    {
    }
    int len = s - np->rcline;
    if ((!*s || std::isspace(*s)) && !g_checkflag)
    {
#ifndef lint
        np->rcline = saferealloc(np->rcline,(MEM_SIZE)len + 3);
#endif
        s = np->rcline + len;
        std::strcpy(s, ": ");
    }
    if (*s == ':' && s[1] && s[2] == '0')
    {
        np->flags |= NF_UNTHREADED;
        s[2] = '1';
    }
    np->subscribechar = *s;         /* salt away the : or ! */
    np->numoffset = len + 1;        /* remember where the numbers are */
    *s = '\0';                      /* null terminate newsgroup name */
}

void abandon_ng(NGDATA *np)
{
    char* some_buf = nullptr;

    /* open newsrc backup copy and try to find the prior value for the group. */
    std::FILE *rcfp = std::fopen(np->rc->oldname, "r");
    if (rcfp != nullptr)
    {
        int length = np->numoffset - 1;

        while ((some_buf = get_a_line(g_buf, LBUFLEN, false, rcfp)) != nullptr)
        {
            if (g_len_last_line_got <= 0)
            {
                continue;
            }
            some_buf[g_len_last_line_got-1] = '\0'; /* wipe out newline */
            if ((some_buf[length] == ':' || some_buf[length] == NEGCHAR) //
                && !std::strncmp(np->rcline, some_buf, length))
            {
                break;
            }
            if (some_buf != g_buf)
            {
                std::free(some_buf);
            }
        }
        std::fclose(rcfp);
    }
    else if (errno != ENOENT)
    {
        std::printf("Unable to open %s.\n", np->rc->oldname);
        termdown(1);
        return;
    }
    if (some_buf == nullptr)
    {
        some_buf = np->rcline + np->numoffset;
        if (*some_buf == ' ')
        {
            some_buf++;
        }
        *some_buf = '\0';
        np->abs1st = 0;         /* force group to be re-calculated */
    }
    else
    {
        std::free(np->rcline);
        if (some_buf == g_buf)
        {
            np->rcline = savestr(some_buf);
        }
        else
        {
            /*NOSTRICT*/
#ifndef lint
            some_buf = saferealloc(some_buf, (MEM_SIZE)(g_len_last_line_got));
#endif /* lint */
            np->rcline = some_buf;
        }
    }
    parse_rcline(np);
    if (np->subscribechar == NEGCHAR)
    {
        np->subscribechar = ':';
    }
    np->rc->flags |= RF_RCCHANGED;
    set_toread(np, ST_LAX);
}

/* try to find or add an explicitly specified newsgroup */
/* returns true if found or added, false if not. */
/* assumes that we are chdir'ed to NEWSSPOOL */

bool get_ng(const char *what, getnewsgroup_flags flags)
{
    char* ntoforget;
    char promptbuf[128];

    if (g_verbose)
    {
        ntoforget = "Type n to forget about this newsgroup.\n";
    }
    else
    {
        ntoforget = "n to forget it.\n";
    }
    if (std::strchr(what, '/'))
    {
        dingaling();
        std::printf("\nBad newsgroup name.\n");
        termdown(2);
      check_fuzzy_match:
        if (g_fuzzy_get && (flags & GNG_FUZZY))
        {
            flags &= ~GNG_FUZZY;
            if (find_close_match())
            {
                what = g_ngname.c_str();
            }
            else
            {
                return false;
            }
        } else
        {
            return false;
        }
    }
    set_ngname(what);
    g_ngptr = find_ng(g_ngname.c_str());
    if (g_ngptr == nullptr)             /* not in .newsrc? */
    {
        NEWSRC* rp;
        for (rp = g_multirc->first; rp; rp = rp->next)
        {
            if (!all_bits(rp->flags, RF_ADD_GROUPS | RF_ACTIVE))
            {
                continue;
            }
            /*$$ this may scan a datasrc multiple times... */
            if (find_actgrp(rp->datasrc,g_buf,g_ngname.c_str(),g_ngname.length(),(ART_NUM)0))
            {
                break; /*$$ let them choose which server */
            }
        }
        if (!rp)
        {
            dingaling();
            if (g_verbose)
            {
                std::printf("\nNewsgroup %s does not exist!\n", g_ngname.c_str());
            }
            else
            {
                std::printf("\nNo %s!\n", g_ngname.c_str());
            }
            termdown(2);
            if (g_novice_delays)
            {
                sleep(2);
            }
            goto check_fuzzy_match;
        }
        addnew_type autosub;
        if (g_mode != MM_INITIALIZING || !(autosub = auto_subscribe(g_ngname.c_str())))
        {
            autosub = g_addnewbydefault;
        }
        if (autosub)
        {
            if (g_append_unsub)
            {
                std::printf("(Adding %s to end of your .newsrc %ssubscribed)\n",
                       g_ngname.c_str(), (autosub == ADDNEW_SUB)? "" : "un");
                termdown(1);
                g_ngptr = add_newsgroup(rp, g_ngname.c_str(), autosub);
            }
            else
            {
                if (autosub == ADDNEW_SUB)
                {
                    std::printf("(Subscribing to %s)\n", g_ngname.c_str());
                    termdown(1);
                    g_ngptr = add_newsgroup(rp, g_ngname.c_str(), autosub);
                }
                else
                {
                    std::printf("(Ignoring %s)\n", g_ngname.c_str());
                    termdown(1);
                    return false;
                }
            }
            flags &= ~GNG_RELOC;
        }
        else
        {
            if (g_verbose)
            {
                std::sprintf(promptbuf, "\nNewsgroup %s not in .newsrc -- subscribe?", g_ngname.c_str());
            }
            else
            {
                std::sprintf(promptbuf,"\nSubscribe %s?",g_ngname.c_str());
            }
reask_add:
            in_char(promptbuf,MM_ADD_NEWSGROUP_PROMPT,"ynYN");
            printcmd();
            newline();
            if (*g_buf == 'h')
            {
                if (g_verbose)
                {
                    std::printf("Type y or SP to subscribe to %s.\n"
                           "Type Y to subscribe to this and all remaining new groups.\n"
                           "Type N to leave all remaining new groups unsubscribed.\n",
                           g_ngname.c_str());
                    termdown(3);
                }
                else
                {
                    std::fputs("y or SP to subscribe, Y to subscribe all new groups, N to unsubscribe all\n",
                          stdout);
                    termdown(1);
                }
                std::fputs(ntoforget,stdout);
                termdown(1);
                goto reask_add;
            }
            else if (*g_buf == 'n' || *g_buf == 'q')
            {
                if (g_append_unsub)
                {
                    g_ngptr = add_newsgroup(rp, g_ngname.c_str(), NEGCHAR);
                }
                return false;
            }
            else if (*g_buf == 'y')
            {
                g_ngptr = add_newsgroup(rp, g_ngname.c_str(), ':');
                flags |= GNG_RELOC;
            }
            else if (*g_buf == 'Y')
            {
                g_addnewbydefault = ADDNEW_SUB;
                if (g_append_unsub)
                {
                    std::printf("(Adding %s to end of your .newsrc subscribed)\n",
                           g_ngname.c_str());
                }
                else
                {
                    std::printf("(Subscribing to %s)\n", g_ngname.c_str());
                }
                termdown(1);
                g_ngptr = add_newsgroup(rp, g_ngname.c_str(), ':');
                flags &= ~GNG_RELOC;
            }
            else if (*g_buf == 'N')
            {
                g_addnewbydefault = ADDNEW_UNSUB;
                if (g_append_unsub)
                {
                    std::printf("(Adding %s to end of your .newsrc unsubscribed)\n",
                           g_ngname.c_str());
                    termdown(1);
                    g_ngptr = add_newsgroup(rp, g_ngname.c_str(), NEGCHAR);
                    flags &= ~GNG_RELOC;
                }
                else
                {
                    std::printf("(Ignoring %s)\n", g_ngname.c_str());
                    termdown(1);
                    return false;
                }
            }
            else
            {
                std::fputs(g_hforhelp,stdout);
                termdown(1);
                settle_down();
                goto reask_add;
            }
        }
    }
    else if (g_mode == MM_INITIALIZING)         /* adding new groups during init? */
    {
        return false;
    }
    else if (g_ngptr->subscribechar == NEGCHAR)  /* unsubscribed? */
    {
        if (g_verbose)
        {
            std::sprintf(promptbuf, "\nNewsgroup %s is unsubscribed -- resubscribe?", g_ngname.c_str());
        }
        else
        {
            std::sprintf(promptbuf, "\nResubscribe %s?", g_ngname.c_str());
        }
reask_unsub:
        in_char(promptbuf,MM_RESUBSCRIBE_PROMPT,"yn");
        printcmd();
        newline();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::printf("Type y or SP to resubscribe to %s.\n", g_ngname.c_str());
            }
            else
            {
                std::fputs("y or SP to resubscribe.\n", stdout);
            }
            std::fputs(ntoforget,stdout);
            termdown(2);
            goto reask_unsub;
        }
        else if (*g_buf == 'n' || *g_buf == 'q')
        {
            return false;
        }
        else if (*g_buf == 'y')
        {
            char *cp = g_ngptr->rcline + g_ngptr->numoffset;
            g_ngptr->flags = (*cp && cp[1] == '0' ? NF_UNTHREADED : NF_NONE);
            g_ngptr->subscribechar = ':';
            g_ngptr->rc->flags |= RF_RCCHANGED;
            flags &= ~GNG_RELOC;
        }
        else
        {
            std::fputs(g_hforhelp,stdout);
            termdown(1);
            settle_down();
            goto reask_unsub;
        }
    }

    /* now calculate how many unread articles in newsgroup */

    set_toread(g_ngptr, ST_STRICT);
    if (flags & GNG_RELOC)
    {
        if (!relocate_newsgroup(g_ngptr,-1))
        {
            return false;
        }
    }
    return g_ngptr->toread >= TR_NONE;
}

/* add a newsgroup to the newsrc file (eventually) */

static NGDATA *add_newsgroup(NEWSRC *rp, const char *ngn, char_int c)
{
    NGDATA *np = ngdata_ptr(g_ngdata_cnt++);
    np->prev = g_last_ng;
    if (g_last_ng)
    {
        g_last_ng->next = np;
    }
    else
    {
        g_first_ng = np;
    }
    np->next = nullptr;
    g_last_ng = np;
    g_newsgroup_cnt++;

    np->rc = rp;
    np->numoffset = std::strlen(ngn) + 1;
    np->rcline = safemalloc((MEM_SIZE)(np->numoffset + 2));
    std::strcpy(np->rcline,ngn);             /* and copy over the name */
    std::strcpy(np->rcline + np->numoffset, " ");
    np->subscribechar = c;              /* subscribe or unsubscribe */
    if (c != NEGCHAR)
    {
        g_newsgroup_toread++;
    }
    np->toread = TR_NONE;               /* just for prettiness */
    sethash(np);                        /* so we can find it again */
    rp->flags |= RF_RCCHANGED;
    return np;
}

bool relocate_newsgroup(NGDATA *move_np, NG_NUM newnum)
{
    NGDATA* np;
    int i;
    const char* dflt = (move_np!=g_current_ng ? "$^.Lq" : "$^Lq");
    sel_sort_mode save_sort = g_sel_sort;

    if (g_sel_newsgroupsort != SS_NATURAL)
    {
        if (newnum < 0)
        {
            /* ask if they want to keep the current order */
            in_char("Sort newsrc(s) using current sort order?",MM_DELETE_BOGUS_NEWSGROUPS_PROMPT, "yn"); /*$$ !'D' */
            printcmd();
            newline();
            if (*g_buf == 'y')
            {
                set_selector(SM_NEWSGROUP, SS_NATURAL);
            }
            else
            {
                g_sel_sort = SS_NATURAL;
                g_sel_direction = 1;
                sort_newsgroups();
            }
        }
        else
        {
            g_sel_sort = SS_NATURAL;
            g_sel_direction = 1;
            sort_newsgroups();
        }
    }

    g_starthere = nullptr;                      /* Disable this optimization */
    if (move_np != g_last_ng)
    {
        if (move_np->prev)
        {
            move_np->prev->next = move_np->next;
        }
        else
        {
            g_first_ng = move_np->next;
        }
        move_np->next->prev = move_np->prev;

        move_np->prev = g_last_ng;
        move_np->next = nullptr;
        g_last_ng->next = move_np;
        g_last_ng = move_np;
    }

    /* Renumber the groups according to current order */
    for (np = g_first_ng, i = 0; np; np = np->next, i++)
    {
        np->num = i;
    }
    move_np->rc->flags |= RF_RCCHANGED;

    if (newnum < 0)
    {
      reask_reloc:
        unflush_output();               /* disable any ^O in effect */
        if (g_verbose)
        {
            std::printf("\nPut newsgroup where? [%s] ", dflt);
        }
        else
        {
            std::printf("\nPut where? [%s] ", dflt);
        }
        std::fflush(stdout);
        termdown(1);
      reinp_reloc:
        eat_typeahead();
        getcmd(g_buf);
        if (errno || *g_buf == '\f')    /* if return from stop signal */
        {
            goto reask_reloc;           /* give them a prompt again */
        }
        setdef(g_buf,dflt);
        printcmd();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "\n"
                       "Type ^ to put the newsgroup first (position 0).\n"
                       "Type $ to put the newsgroup last (position %d).\n",
                       g_newsgroup_cnt - 1);
                std::printf("Type . to put it before the current newsgroup.\n"
                       "Type -newsgroup name to put it before that newsgroup.\n"
                       "Type +newsgroup name to put it after that newsgroup.\n"
                       "Type a number between 0 and %d to put it at that position.\n",
                       g_newsgroup_cnt - 1);
                std::printf("Type L for a listing of newsgroups and their positions.\n"
                       "Type q to abort the current action.\n");
            }
            else
            {
                std::printf("\n"
                       "\n"
                       "^ to put newsgroup first (pos 0).\n"
                       "$ to put last (pos %d).\n",
                       g_newsgroup_cnt - 1);
                std::printf(". to put before current newsgroup.\n"
                       "-newsgroup to put before newsgroup.\n"
                       "+newsgroup to put after.\n"
                       "number in 0-%d to put at that pos.\n"
                       "L for list of newsrc.\n"
                       "q to abort\n",
                       g_newsgroup_cnt - 1);
            }
            termdown(10);
            goto reask_reloc;
        }
        else if (*g_buf == 'q')
        {
            return false;
        }
        else if (*g_buf == 'L')
        {
            newline();
            list_newsgroups();
            goto reask_reloc;
        }
        else if (std::isdigit(*g_buf))
        {
            if (!finish_command(true))  /* get rest of command */
            {
                goto reinp_reloc;
            }
            newnum = std::atol(g_buf);
            newnum = std::max(newnum, 0);
            if (newnum >= g_newsgroup_cnt)
            {
                newnum = g_newsgroup_cnt - 1;
            }
        }
        else if (*g_buf == '^')
        {
            newline();
            newnum = 0;
        }
        else if (*g_buf == '$')
        {
            newnum = g_newsgroup_cnt-1;
        }
        else if (*g_buf == '.')
        {
            newline();
            newnum = g_current_ng->num;
        }
        else if (*g_buf == '-' || *g_buf == '+')
        {
            if (!finish_command(true))  /* get rest of command */
            {
                goto reinp_reloc;
            }
            np = find_ng(g_buf+1);
            if (np == nullptr)
            {
                std::fputs("Not found.",stdout);
                goto reask_reloc;
            }
            newnum = np->num;
            if (*g_buf == '+')
            {
                newnum++;
            }
        }
        else
        {
            std::printf("\n%s",g_hforhelp);
            termdown(2);
            settle_down();
            goto reask_reloc;
        }
    }
    if (newnum < g_newsgroup_cnt - 1)
    {
        for (np = g_first_ng; np; np = np->next)
        {
            if (np->num >= newnum)
            {
                break;
            }
        }
        if (!np || np == move_np)
        {
            return false;               /* This can't happen... */
        }

        g_last_ng = move_np->prev;
        g_last_ng->next = nullptr;

        move_np->prev = np->prev;
        move_np->next = np;

        if (np->prev)
        {
            np->prev->next = move_np;
        }
        else
        {
            g_first_ng = move_np;
        }
        np->prev = move_np;

        move_np->num = newnum++;
        for (; np; np = np->next, newnum++)
        {
            np->num = newnum;
        }
    }
    if (g_sel_newsgroupsort != SS_NATURAL)
    {
        g_sel_sort = g_sel_newsgroupsort;
        sort_newsgroups();
        g_sel_sort = save_sort;
    }
    return true;
}

/* List out the newsrc with annotations */

void list_newsgroups()
{
    NGDATA* np;
    NG_NUM i;
    char tmpbuf[2048];
    static const char* status[] = {"(READ)","(UNSUB)","(DUP)","(BOGUS)","(JUNK)"};

    page_start();
    print_lines("  #  Status  Newsgroup\n", STANDOUT);
    for (np = g_first_ng, i = 0; np && !g_int_count; np = np->next, i++)
    {
        if (np->toread >= 0)
        {
            set_toread(np, ST_LAX);
        }
        *(np->rcline + np->numoffset - 1) = np->subscribechar;
        if (np->toread > 0)
        {
            std::sprintf(tmpbuf, "%3d %6ld   ", i, (long) np->toread);
        }
        else
        {
            std::sprintf(tmpbuf, "%3d %7s  ", i, status[-np->toread]);
        }
        safecpy(tmpbuf+13, np->rcline, sizeof tmpbuf - 13);
        *(np->rcline + np->numoffset - 1) = '\0';
        if (print_lines(tmpbuf, NOMARKING) != 0)
        {
            break;
        }
    }
    g_int_count = 0;
}

/* find a newsgroup in any newsrc */

NGDATA *find_ng(const char *ngnam)
{
    HASHDATUM data = hashfetch(g_newsrc_hash, ngnam, std::strlen(ngnam));
    return (NGDATA*)data.dat_ptr;
}

void cleanup_newsrc(NEWSRC *rp)
{
    NG_NUM bogosity = 0;

    if (g_verbose)
    {
        std::printf("Checking out '%s' -- hang on a second...\n", rp->name);
    }
    else
    {
        std::printf("Checking '%s' -- hang on...\n", rp->name);
    }
    termdown(1);
    NGDATA* np;
    for (np = g_first_ng; np; np = np->next)
    {
/*#ifdef CHECK_ALL_BOGUS $$ what is this? */
        if (np->toread >= TR_UNSUB)
        {
            set_toread(np, ST_LAX); /* this may reset the group or declare it bogus */
        }
/*#endif*/
        if (np->toread == TR_BOGUS)
        {
            bogosity++;
        }
    }
    for (np = g_last_ng; np && np->toread == TR_BOGUS; np = np->prev)
    {
        bogosity--;                     /* discount already moved ones */
    }
    if (g_newsgroup_cnt > 5 && bogosity > g_newsgroup_cnt / 2)
    {
        std::fputs("It looks like the active file is messed up.  Contact your news administrator,\n",
              stdout);
        std::fputs("leave the \"bogus\" groups alone, and they may come back to normal.  Maybe.\n",
              stdout);
        termdown(2);
    }
    else if (bogosity)
    {
        if (g_verbose)
        {
            std::printf("Moving bogus newsgroups to the end of '%s'.\n", rp->name);
        }
        else
        {
            std::fputs("Moving boguses to the end.\n", stdout);
        }
        termdown(1);
        while (np)
        {
            NGDATA *prev_np = np->prev;
            if (np->toread == TR_BOGUS)
            {
                relocate_newsgroup(np, g_newsgroup_cnt - 1);
            }
            np = prev_np;
        }
        rp->flags |= RF_RCCHANGED;
reask_bogus:
        in_char("Delete bogus newsgroups?", MM_DELETE_BOGUS_NEWSGROUPS_PROMPT, "ny");
        printcmd();
        newline();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::fputs("Type y to delete bogus newsgroups.\n"
                      "Type n or SP to leave them at the end in case they return.\n",
                      stdout);
                termdown(2);
            }
            else
            {
                std::fputs("y to delete, n to keep\n",stdout);
                termdown(1);
            }
            goto reask_bogus;
        }
        else if (*g_buf == 'n' || *g_buf == 'q')
        {
        }
        else if (*g_buf == 'y')
        {
            for (np = g_last_ng; np && np->toread == TR_BOGUS; np = np->prev)
            {
                hashdelete(g_newsrc_hash, np->rcline, np->numoffset - 1);
                clear_ngitem((char*)np,0);
                g_newsgroup_cnt--;
            }
            rp->flags |= RF_RCCHANGED; /*$$ needed? */
            g_last_ng = np;
            if (np)
            {
                np->next = nullptr;
            }
            else
            {
                g_first_ng = nullptr;
            }
            if (g_current_ng && !g_current_ng->rcline)
            {
                g_current_ng = g_first_ng;
            }
            if (g_recent_ng && !g_recent_ng->rcline)
            {
                g_recent_ng = g_first_ng;
            }
            if (g_ngptr && !g_ngptr->rcline)
            {
                g_ngptr = g_first_ng;
            }
            if (g_sel_page_np && !g_sel_page_np->rcline)
            {
                g_sel_page_np = nullptr;
            }
        }
        else
        {
            std::fputs(g_hforhelp,stdout);
            termdown(1);
            settle_down();
            goto reask_bogus;
        }
    }
    g_paranoid = false;
}

/* make an entry in the hash table for the current newsgroup */

void sethash(NGDATA *np)
{
    HASHDATUM data;
    data.dat_ptr = (char*)np;
    data.dat_len = np->numoffset - 1;
    hashstore(g_newsrc_hash, np->rcline, data.dat_len, data);
}

static int rcline_cmp(const char *key, int keylen, HASHDATUM data)
{
    /* We already know that the lengths are equal, just compare the strings */
    return std::memcmp(key, ((NGDATA*)data.dat_ptr)->rcline, keylen);
}

/* checkpoint the newsrc(s) */

void checkpoint_newsrcs()
{
#ifdef DEBUG
    if (debug & DEB_CHECKPOINTING)
    {
        std::fputs("(ckpt)",stdout);
        std::fflush(stdout);
    }
#endif
    if (g_doing_ng)
    {
        bits_to_rc();                   /* do not restore M articles */
    }
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
#ifdef DEBUG
    if (debug & DEB_CHECKPOINTING)
    {
        std::fputs("(done)",stdout);
        std::fflush(stdout);
    }
#endif
}

/* write out the (presumably) revised newsrc(s) */

bool write_newsrcs(MULTIRC *mptr)
{
    sel_sort_mode save_sort = g_sel_sort;
    bool          total_success = true;

    if (!mptr)
    {
        return true;
    }

    if (g_sel_newsgroupsort != SS_NATURAL)
    {
        g_sel_sort = SS_NATURAL;
        g_sel_direction = 1;
        sort_newsgroups();
    }

    for (NEWSRC *rp = mptr->first; rp; rp = rp->next)
    {
        if (!(rp->flags & RF_ACTIVE))
        {
            continue;
        }

        if (rp->infoname)
        {
            std::FILE *info = std::fopen(rp->infoname, "w");
            if (info != nullptr)
            {
                std::fprintf(info,"Last-Group: %s\nNew-Group-State: %ld,%ld,%ld\n",
                        g_ngname.c_str(),rp->datasrc->lastnewgrp,
                        rp->datasrc->act_sf.recent_cnt,
                        rp->datasrc->desc_sf.recent_cnt);
                std::fclose(info);
            }
        }
        else
        {
            readlast();
            if (rp->datasrc->flags & DF_REMOTE)
            {
                g_lastactsiz = rp->datasrc->act_sf.recent_cnt;
                g_lastextranum = rp->datasrc->desc_sf.recent_cnt;
            }
            else
            {
                g_lastextranum = rp->datasrc->act_sf.recent_cnt;
            }
            g_lastnewtime = rp->datasrc->lastnewgrp;
            writelast();
        }

        if (!(rp->flags & RF_RCCHANGED))
        {
            continue;
        }

        FILE *rcfp = fopen(rp->newname, "w");
        if (rcfp == nullptr)
        {
            std::printf(s_cantrecreate,rp->name);
            total_success = false;
            continue;
        }
#ifndef MSDOS
        stat_t perms;
        if (stat(rp->name,&perms)>=0)   /* preserve permissions */
        {
            chmod(rp->newname,perms.st_mode&0666);
            chown(rp->newname,perms.st_uid,perms.st_gid);
        }
#endif
        /* write out each line*/

        for (NGDATA *np = g_first_ng; np; np = np->next)
        {
            char* delim;
            if (np->rc != rp)
            {
                continue;
            }
            if (np->numoffset)
            {
                delim = np->rcline + np->numoffset - 1;
                *delim = np->subscribechar;
                if ((np->flags & NF_UNTHREADED) && delim[2] == '1')
                {
                    delim[2] = '0';
                }
            }
            else
            {
                delim = nullptr;
            }
#ifdef DEBUG
            if (debug & DEB_NEWSRC_LINE)
            {
                std::printf("%s\n",np->rcline);
                termdown(1);
            }
#endif
            if (std::fprintf(rcfp, "%s\n", np->rcline) < 0)
            {
                std::fclose(rcfp);           /* close new newsrc */
                goto write_error;
            }
            if (delim)
            {
                *delim = '\0';          /* might still need this line */
                if ((np->flags & NF_UNTHREADED) && delim[2] == '0')
                {
                    delim[2] = '1';
                }
            }
        }
        std::fflush(rcfp);
        /* fclose is the only sure test for full disks via NFS */
        if (std::ferror(rcfp))
        {
            std::fclose(rcfp);
            goto write_error;
        }
        if (std::fclose(rcfp) == EOF)
        {
          write_error:
            std::printf(s_cantrecreate,rp->name);
            remove(rp->newname);
            total_success = false;
            continue;
        }
        rp->flags &= ~RF_RCCHANGED;

        remove(rp->name);
        rename(rp->newname,rp->name);
    }

    if (g_sel_newsgroupsort != SS_NATURAL)
    {
        g_sel_sort = g_sel_newsgroupsort;
        sort_newsgroups();
        g_sel_sort = save_sort;
    }
    return total_success;
}

void get_old_newsrcs(MULTIRC *mptr)
{
    if (mptr)
    {
        for (NEWSRC *rp = mptr->first; rp; rp = rp->next)
        {
            if (rp->flags & RF_ACTIVE)
            {
                remove(rp->newname);
                rename(rp->name,rp->newname);
                rename(rp->oldname,rp->name);
            }
        }
    }
}
