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

List       *g_newsgroup_data_list{};       /* a list of NGDATA */
int         g_newsgroup_data_count{};        //
NewsgroupNum      g_newsgroup_count{};     /* all newsgroups in our current newsrc(s) */
NewsgroupNum      g_newsgroup_to_read{};  //
ArticleUnread  g_newsgroup_min_to_read{1};    /* == TR_ONE or TR_NONE */
NewsgroupData     *g_first_newsgroup{};          //
NewsgroupData     *g_last_newsgroup{};           //
NewsgroupData     *g_newsgroup_ptr{};             /* current newsgroup data ptr */
NewsgroupData     *g_current_newsgroup{};        /* stable current newsgroup so we can ditz with g_ngptr */
NewsgroupData     *g_recent_newsgroup{};         /* the prior newsgroup we visited */
NewsgroupData     *g_start_here{};         /* set to the first newsgroup with unread news on startup */
NewsgroupData     *g_sel_page_np{};       //
NewsgroupData     *g_sel_next_np{};       //
ArticleNum     g_abs_first{};          /* 1st real article in current newsgroup */
ArticleNum     g_first_art{};          /* minimum unread article number in newsgroup */
ArticleNum     g_last_art{};           /* maximum article number in newsgroup */
ArticleUnread  g_missing_count{};     /* for reports on missing articles */
std::string g_moderated;           //
bool        g_redirected{};        //
std::string g_redirected_to;       //
bool        g_threaded_group{};    //
NewsgroupData     *g_ng_go_newsgroup_ptr{};       //
ArticleNum     g_ng_go_art_num{};      //
bool        g_novice_delays{true}; /* +f */
bool        g_in_ng{};             /* true if in a newsgroup */

static int ngorder_number(const NewsgroupData **npp1, const NewsgroupData **npp2);
static int ngorder_groupname(const NewsgroupData **npp1, const NewsgroupData **npp2);
static int ngorder_count(const NewsgroupData **npp1, const NewsgroupData **npp2);

void newsgroup_data_init()
{
}

/* set current newsgroup */

void set_newsgroup(NewsgroupData *np)
{
    g_newsgroup_ptr = np;
    if (g_newsgroup_ptr)
    {
        set_ngname(g_newsgroup_ptr->rc_line);
    }
}

int access_newsgroup()
{
    ArticleNum old_first = g_newsgroup_ptr->abs_first;

    if (g_data_source->flags & DF_REMOTE)
    {
        int ret = nntp_group(g_ngname.c_str(),g_newsgroup_ptr);
        if (ret == -2)
        {
            return -2;
        }
        if (ret <= 0)
        {
            g_newsgroup_ptr->to_read = TR_BOGUS;
            return 0;
        }
        g_last_art = get_newsgroup_size(g_newsgroup_ptr);
        if (g_last_art < 0) /* Impossible... */
        {
            return 0;
        }
        g_abs_first = g_newsgroup_ptr->abs_first;
        if (g_abs_first > old_first)
        {
            check_expired(g_newsgroup_ptr, g_abs_first);
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
            g_newsgroup_ptr->to_read = TR_NONE;
            return 0;
        }

        /* chdir to newsgroup subdirectory */
        if (change_dir(g_ngdir))
        {
            std::printf(g_nocd,g_ngdir.c_str());
            return 0;
        }
        g_last_art = get_newsgroup_size(g_newsgroup_ptr);
        if (g_last_art < 0) /* Impossible... */
        {
            return 0;
        }
        g_abs_first = g_newsgroup_ptr->abs_first;
    }

    g_dm_count = 0;
    g_missing_count = 0;
    g_in_ng = true;                     /* tell the world we are here */

    build_cache();
    return 1;
}

void chdir_news_dir()
{
    if (change_dir(g_data_source->spool_dir) || (!(g_data_source->flags & DF_REMOTE) && change_dir(g_ngdir)))
    {
        std::printf(g_nocd,g_ngdir.c_str());
        sig_catcher(0);
    }
}

void grow_newsgroup(ArticleNum new_last)
{
    g_force_grow = false;
    if (new_last > g_last_art)
    {
        ArticleNum tmpart = g_art;
        g_newsgroup_ptr->to_read += (ArticleUnread)(new_last-g_last_art);
        ArticleNum tmpfirst = g_last_art + 1;
        /* Increase the size of article scan arrays. */
        sa_grow(g_last_art,new_last);
        do
        {
            g_last_art++;
            article_ptr(g_last_art)->flags |= AF_EXISTS|AF_UNREAD;
        } while (g_last_art < new_last);
        g_article_list->high = g_last_art;
        thread_grow();
        /* Score all new articles now just in case they weren't done above. */
        sc_fill_score_list(tmpfirst,new_last);
        if (g_verbose)
        {
            std::sprintf(g_buf, "%ld more article%s arrived -- processing memorized commands...\n\n",
                    (long) (g_last_art - tmpfirst + 1), (g_last_art > tmpfirst ? "s have" : " has"));
        }
        else                    /* my, my, how clever we are */
        {
            std::strcpy(g_buf, "More news -- auto-processing...\n\n");
        }
        termdown(2);
        if (g_kf_state & KFS_NORMAL_LINES)
        {
            bool forcelast_save = g_force_last;
            Article* artp_save = g_artp;
            kill_unwanted(tmpfirst,g_buf,true);
            g_artp = artp_save;
            g_force_last = forcelast_save;
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
    return string_case_compare((*npp1)->rc_line, (*npp2)->rc_line) * g_sel_direction;
}

static int ngorder_count(const NewsgroupData **npp1, const NewsgroupData **npp2)
{
    int eq = (int)((*npp1)->to_read - (*npp2)->to_read);
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
    if (!g_first_newsgroup || !g_first_newsgroup->next)
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

    NewsgroupData **ng_list = (NewsgroupData**)safemalloc(g_newsgroup_count * sizeof(NewsgroupData*));
    lp = ng_list;
    for (NewsgroupData* np = g_first_newsgroup; np; np = np->next)
    {
        *lp++ = np;
    }
    TRN_ASSERT(lp - ng_list == g_newsgroup_count);

    std::qsort(ng_list, g_newsgroup_count, sizeof (NewsgroupData*), (int(*)(void const *, void const *))sort_procedure);

    g_first_newsgroup = ng_list[0];
    g_first_newsgroup->prev = nullptr;
    lp = ng_list;
    for (int i = g_newsgroup_count; --i; lp++)
    {
        lp[0]->next = lp[1];
        lp[1]->prev = lp[0];
    }
    g_last_newsgroup = lp[0];
    g_last_newsgroup->next = nullptr;
    std::free((char*)ng_list);
}

void newsgroup_skip()
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
            (void) fetch_subj(g_art, false);
            ArticleNum artnum = g_art + PREFETCH_SIZE - 1;
            artnum = std::min(artnum, g_last_art);
            while (g_art <= artnum)
            {
                if (g_artp->flags & AF_EXISTS)
                {
                    return;
                }
                g_art = article_next(g_art);
                g_artp = article_ptr(g_art);
            }
        } while (g_art <= g_last_art);
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

ArticleNum get_newsgroup_size(NewsgroupData *gp)
{
    char tmpbuf[LBUFLEN];
    long last;
    long first;
    char ch;

    char *nam = gp->rc_line;
    int   len = gp->num_offset - 1;

    if (!find_active_group(gp->rc->data_source, tmpbuf, nam, len, gp->ng_max))
    {
        if (gp->subscribe_char == ':')
        {
            gp->subscribe_char = NEGCHAR;
            gp->rc->flags |= RF_RC_CHANGED;
            g_newsgroup_to_read--;
        }
        return TR_BOGUS;
    }

#ifdef ANCIENT_NEWS
    std::sscanf(tmpbuf+len+1, "%ld %c", &last, &ch);
    first = 1;
#else
    std::sscanf(tmpbuf+len+1, "%ld %ld %c", &last, &first, &ch);
#endif
    if (!gp->abs_first)
    {
        gp->abs_first = (ArticleNum) first;
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
    if (last <= gp->ng_max)
    {
        return gp->ng_max;
    }
    return gp->ng_max = (ArticleNum)last;
}
