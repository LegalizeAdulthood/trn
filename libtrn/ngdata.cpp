/* ngdata.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/fdio.h>
#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/ngdata.h"

#include "trn/list.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/change_dir.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/rthread.h"
#include "trn/scanart.h"
#include "trn/score.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

List       *g_ngdata_list{};       /* a list of NGDATA */
int         g_ngdata_cnt{};        //
NewsgroupNum      g_newsgroup_cnt{};     /* all newsgroups in our current newsrc(s) */
NewsgroupNum      g_newsgroup_toread{};  //
ArticleUnread  g_ng_min_toread{1};    /* == TR_ONE or TR_NONE */
NewsgroupData     *g_first_ng{};          //
NewsgroupData     *g_last_ng{};           //
NewsgroupData     *g_ngptr{};             /* current newsgroup data ptr */
NewsgroupData     *g_current_ng{};        /* stable current newsgroup so we can ditz with g_ngptr */
NewsgroupData     *g_recent_ng{};         /* the prior newsgroup we visited */
NewsgroupData     *g_starthere{};         /* set to the first newsgroup with unread news on startup */
NewsgroupData     *g_sel_page_np{};       //
NewsgroupData     *g_sel_next_np{};       //
ArticleNum     g_absfirst{};          /* 1st real article in current newsgroup */
ArticleNum     g_firstart{};          /* minimum unread article number in newsgroup */
ArticleNum     g_lastart{};           /* maximum article number in newsgroup */
ArticleUnread  g_missing_count{};     /* for reports on missing articles */
std::string g_moderated;           //
bool        g_redirected{};        //
std::string g_redirected_to;       //
bool        g_threaded_group{};    //
NewsgroupData     *g_ng_go_ngptr{};       //
ArticleNum     g_ng_go_artnum{};      //
bool        g_novice_delays{true}; /* +f */
bool        g_in_ng{};             /* true if in a newsgroup */

static int ngorder_number(const NewsgroupData **npp1, const NewsgroupData **npp2);
static int ngorder_groupname(const NewsgroupData **npp1, const NewsgroupData **npp2);
static int ngorder_count(const NewsgroupData **npp1, const NewsgroupData **npp2);

void ngdata_init()
{
}

/* set current newsgroup */

void set_ng(NewsgroupData *np)
{
    g_ngptr = np;
    if (g_ngptr)
    {
        set_ngname(g_ngptr->rcline);
    }
}

int access_ng()
{
    ArticleNum old_first = g_ngptr->abs1st;

    if (g_data_source->flags & DF_REMOTE)
    {
        int ret = nntp_group(g_ngname.c_str(),g_ngptr);
        if (ret == -2)
        {
            return -2;
        }
        if (ret <= 0)
        {
            g_ngptr->toread = TR_BOGUS;
            return 0;
        }
        g_lastart = getngsize(g_ngptr);
        if (g_lastart < 0) /* Impossible... */
        {
            return 0;
        }
        g_absfirst = g_ngptr->abs1st;
        if (g_absfirst > old_first)
        {
            checkexpired(g_ngptr, g_absfirst);
        }
    }
    else
    {
        if (eaccess(g_ngdir.c_str(),5))                 /* directory read protected? */
        {
            if (eaccess(g_ngdir.c_str(), 0))
            {
                if (g_verbose)
                {
                    std::printf("\nNewsgroup %s does not have a spool directory!\n", g_ngname.c_str());
                }
                else
                {
                    std::printf("\nNo spool for %s!\n", g_ngname.c_str());
                }
                termdown(2);
            }
            else
            {
                if (g_verbose)
                {
                    std::printf("\nNewsgroup %s is not currently accessible.\n", g_ngname.c_str());
                }
                else
                {
                    std::printf("\n%s not readable.\n", g_ngname.c_str());
                }
                termdown(2);
            }
            /* make this newsgroup temporarily invisible */
            g_ngptr->toread = TR_NONE;
            return 0;
        }

        /* chdir to newsgroup subdirectory */
        if (change_dir(g_ngdir))
        {
            std::printf(g_nocd,g_ngdir.c_str());
            return 0;
        }
        g_lastart = getngsize(g_ngptr);
        if (g_lastart < 0) /* Impossible... */
        {
            return 0;
        }
        g_absfirst = g_ngptr->abs1st;
    }

    g_dm_count = 0;
    g_missing_count = 0;
    g_in_ng = true;                     /* tell the world we are here */

    build_cache();
    return 1;
}

void chdir_newsdir()
{
    if (change_dir(g_data_source->spool_dir) || (!(g_data_source->flags & DF_REMOTE) && change_dir(g_ngdir)))
    {
        std::printf(g_nocd,g_ngdir.c_str());
        sig_catcher(0);
    }
}

void grow_ng(ArticleNum newlast)
{
    g_forcegrow = false;
    if (newlast > g_lastart)
    {
        ArticleNum tmpart = g_art;
        g_ngptr->toread += (ArticleUnread)(newlast-g_lastart);
        ArticleNum tmpfirst = g_lastart + 1;
        /* Increase the size of article scan arrays. */
        sa_grow(g_lastart,newlast);
        do
        {
            g_lastart++;
            article_ptr(g_lastart)->flags |= AF_EXISTS|AF_UNREAD;
        } while (g_lastart < newlast);
        g_article_list->high = g_lastart;
        thread_grow();
        /* Score all new articles now just in case they weren't done above. */
        sc_fill_scorelist(tmpfirst,newlast);
        if (g_verbose)
        {
            std::sprintf(g_buf, "%ld more article%s arrived -- processing memorized commands...\n\n",
                    (long) (g_lastart - tmpfirst + 1), (g_lastart > tmpfirst ? "s have" : " has"));
        }
        else                    /* my, my, how clever we are */
        {
            std::strcpy(g_buf, "More news -- auto-processing...\n\n");
        }
        termdown(2);
        if (g_kf_state & KFS_NORMAL_LINES)
        {
            bool forcelast_save = g_forcelast;
            Article* artp_save = g_artp;
            kill_unwanted(tmpfirst,g_buf,true);
            g_artp = artp_save;
            g_forcelast = forcelast_save;
        }
        g_art = tmpart;
    }
}

static int ngorder_number(const NewsgroupData **npp1, const NewsgroupData **npp2)
{
    return (int)((*npp1)->num - (*npp2)->num) * g_sel_direction;
}

static int ngorder_groupname(const NewsgroupData **npp1, const NewsgroupData **npp2)
{
    return string_case_compare((*npp1)->rcline, (*npp2)->rcline) * g_sel_direction;
}

static int ngorder_count(const NewsgroupData **npp1, const NewsgroupData **npp2)
{
    int eq = (int)((*npp1)->toread - (*npp2)->toread);
    if (eq != 0)
    {
        return eq * g_sel_direction;
    }
    return (int)((*npp1)->num - (*npp2)->num);
}

/* Sort the newsgroups into the chosen order.
*/
void sort_newsgroups()
{
    NewsgroupData**lp;
    int (*  sort_procedure)(const NewsgroupData **npp1,const NewsgroupData **npp2);

    /* If we don't have at least two newsgroups, we're done! */
    if (!g_first_ng || !g_first_ng->next)
    {
        return;
    }

    switch (g_sel_sort)
    {
    case SS_NATURAL:
    default:
        sort_procedure = ngorder_number;
        break;

    case SS_STRING:
        sort_procedure = ngorder_groupname;
        break;

    case SS_COUNT:
        sort_procedure = ngorder_count;
        break;
    }

    NewsgroupData **ng_list = (NewsgroupData**)safemalloc(g_newsgroup_cnt * sizeof(NewsgroupData*));
    lp = ng_list;
    for (NewsgroupData* np = g_first_ng; np; np = np->next)
    {
        *lp++ = np;
    }
    TRN_ASSERT(lp - ng_list == g_newsgroup_cnt);

    std::qsort(ng_list, g_newsgroup_cnt, sizeof (NewsgroupData*), (int(*)(void const *, void const *))sort_procedure);

    g_first_ng = ng_list[0];
    g_first_ng->prev = nullptr;
    lp = ng_list;
    for (int i = g_newsgroup_cnt; --i; lp++)
    {
        lp[0]->next = lp[1];
        lp[1]->prev = lp[0];
    }
    g_last_ng = lp[0];
    g_last_ng->next = nullptr;
    std::free((char*)ng_list);
}

void ng_skip()
{
    if (g_data_source->flags & DF_REMOTE)
    {
        clear();
        if (g_verbose)
        {
            std::fputs("Skipping unavailable article\n", stdout);
        }
        else
        {
            std::fputs("Skipping\n", stdout);
        }
        termdown(1);
        if (g_novice_delays)
        {
            pad(g_just_a_sec/3);
            sleep(1);
        }
        g_art = article_next(g_art);
        g_artp = article_ptr(g_art);
        do
        {
            /* tries to grab PREFETCH_SIZE XHDRS, flagging missing articles */
            (void) fetchsubj(g_art, false);
            ArticleNum artnum = g_art + PREFETCH_SIZE - 1;
            artnum = std::min(artnum, g_lastart);
            while (g_art <= artnum)
            {
                if (g_artp->flags & AF_EXISTS)
                {
                    return;
                }
                g_art = article_next(g_art);
                g_artp = article_ptr(g_art);
            }
        } while (g_art <= g_lastart);
    }
    else
    {
        if (errno != ENOENT)    /* has it not been deleted? */
        {
            clear();
            if (g_verbose)
            {
                std::printf("\n(Article %ld exists but is unreadable.)\n", (long) g_art);
            }
            else
            {
                std::printf("\n(%ld unreadable.)\n", (long) g_art);
            }
            termdown(2);
            if (g_novice_delays)
            {
                pad(g_just_a_sec);
                sleep(2);
            }
        }
        inc_art(g_selected_only,false); /* try next article */
    }
}

/* find the maximum article number of a newsgroup */

ArticleNum getngsize(NewsgroupData *gp)
{
    char tmpbuf[LBUFLEN];
    long last;
    long first;
    char ch;

    char *nam = gp->rcline;
    int   len = gp->numoffset - 1;

    if (!find_active_group(gp->rc->datasrc, tmpbuf, nam, len, gp->ngmax))
    {
        if (gp->subscribechar == ':')
        {
            gp->subscribechar = NEGCHAR;
            gp->rc->flags |= RF_RCCHANGED;
            g_newsgroup_toread--;
        }
        return TR_BOGUS;
    }

#ifdef ANCIENT_NEWS
    std::sscanf(tmpbuf+len+1, "%ld %c", &last, &ch);
    first = 1;
#else
    std::sscanf(tmpbuf+len+1, "%ld %ld %c", &last, &first, &ch);
#endif
    if (!gp->abs1st)
    {
        gp->abs1st = (ArticleNum) first;
    }
    if (!g_in_ng)
    {
        if (g_redirected)
        {
            g_redirected = false;
            g_redirected_to.clear();
        }
        switch (ch)
        {
        case 'n':
            g_moderated = get_val_const("NOPOSTRING"," (no posting)");
            break;

        case 'm':
            g_moderated = get_val_const("MODSTRING", " (moderated)");
            break;

        case 'x':
            g_redirected = true;
            g_redirected_to.clear();
            g_moderated = " (DISABLED)";
            break;

        case '=':
            len = std::strlen(tmpbuf);
            if (tmpbuf[len-1] == '\n')
            {
                tmpbuf[len - 1] = '\0';
            }
            g_redirected = true;
            g_redirected_to = std::strrchr(tmpbuf, '=') + 1;
            g_moderated = " (REDIRECTED)";
            break;

        default:
            g_moderated.clear();
            break;
        }
    }
    if (last <= gp->ngmax)
    {
        return gp->ngmax;
    }
    return gp->ngmax = (ArticleNum)last;
}
