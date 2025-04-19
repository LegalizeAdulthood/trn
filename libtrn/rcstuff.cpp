/* rcstuff.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/rcstuff.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/autosub.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/hash.h"
#include "trn/init.h"
#include "trn/last.h"
#include "trn/nntp.h"
#include "trn/only.h"
#include "trn/rcln.h"
#include "trn/rt-page.h"
#include "trn/rt-select.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

HashTable *g_newsrc_hash{};
Multirc   *g_sel_page_mp{};
Multirc   *g_sel_next_mp{};
List      *g_multirc_list{};                 // a list of all MULTIRCs
Multirc   *g_multirc{};                      // the current MULTIRC
bool       g_paranoid{};                     // did we detect some inconsistency in .newsrc?
AddNewType g_add_new_by_default{ADDNEW_ASK}; //
bool       g_check_flag{};                   // -c
bool       g_suppress_cn{};                  // -s
int        g_countdown{5};                   // how many lines to list before invoking -s
bool       g_fuzzy_get{};                    // -G
bool       g_append_unsub{};                 // -I

enum
{
    RI_ID = 1,
    RI_NEWSRC = 2,
    RI_ADD_GROUPS = 3
};

static IniWords s_rcgroups_ini[] = {
    { 0, "RCGROUPS", nullptr },
    { 0, "ID", nullptr },
    { 0, "Newsrc", nullptr },
    { 0, "Add Groups", nullptr },
    { 0, nullptr, nullptr }
};
static bool        s_found_any{};
static const char *s_cant_recreate{"Can't recreate %s -- restoring older version.\n"
                                   "Perhaps you are near or over quota?\n"};

static bool    clear_newsgroup_item(char *cp, int arg);
static bool    lock_newsrc(Newsrc *rp);
static void    unlock_newsrc(Newsrc *rp);
static bool    open_newsrc(Newsrc *rp);
static void    init_newsgroup_node(List *list, ListNode *node);
static void    parse_rcline(NewsgroupData *np);
static NewsgroupData *add_newsgroup(Newsrc *rp, const char *ngn, char_int c);
static int     rcline_cmp(const char *key, int keylen, HashDatum data);

inline NewsgroupData *newsgroup_data_ptr(int ngnum)
{
    return (NewsgroupData *) list_get_item(g_newsgroup_data_list, ngnum);
}

static Multirc *rcstuff_init_data()
{
    Multirc* mptr = nullptr;

    g_multirc_list = new_list(0,0,sizeof(Multirc),20,LF_ZERO_MEM|LF_SPARSE,nullptr);

    if (g_trn_access_mem)
    {
        char* section;
        char* cond;
        char**vals = prep_ini_words(s_rcgroups_ini);
        char *s = g_trn_access_mem;
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
            Newsrc *rp = new_newsrc(vals[RI_ID], vals[RI_NEWSRC], vals[RI_ADD_GROUPS]);
            if (rp)
            {
                Multirc *mp = multirc_ptr(i);
                Newsrc * prev_rp = mp->first;
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
        }
        std::free(vals);
        safe_free0(g_trn_access_mem);
    }
    return mptr;
}

bool rcstuff_init()
{
    Multirc *mptr = rcstuff_init_data();

    if (g_use_newsrc_selector && !g_check_flag)
    {
        return true;
    }

    s_found_any = false;
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
    if (g_check_flag)                    // were we just checking?
    {
        finalize(s_found_any);           // tell them what we found
    }
    return s_found_any;
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

Newsrc *new_newsrc(const char *name, const char *newsrc, const char *add_ok)
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

    DataSource *dp = get_data_source(name);
    if (!dp)
    {
        return nullptr;
    }

    Newsrc *rp = (Newsrc*)safe_malloc(sizeof(Newsrc));
    std::memset((char*)rp,0,sizeof (Newsrc));
    rp->data_source = dp;
    rp->name = save_str(file_exp(newsrc));
    char tmpbuf[CMD_BUF_LEN];
    std::sprintf(tmpbuf, RCNAME_OLD, rp->name);
    rp->old_name = save_str(tmpbuf);
    std::sprintf(tmpbuf, RCNAME_NEW, rp->name);
    rp->new_name = save_str(tmpbuf);

    switch (add_ok ? *add_ok : 'y')
    {
    case 'n':
    case 'N':
        break;

    default:
        if (dp->flags & DF_ADD_OK)
        {
            rp->flags |= RF_ADD_NEW_GROUPS;
        }
        // FALL THROUGH

    case 'm':
    case 'M':
        rp->flags |= RF_ADD_GROUPS;
        break;
    }
    return rp;
}

bool use_multirc(Multirc *mp)
{
    bool had_trouble = false;
    bool had_success = false;

    for (Newsrc *rp = mp->first; rp; rp = rp->next)
    {
        if ((rp->data_source->flags & DF_UNAVAILABLE) || !lock_newsrc(rp) //
            || !open_data_source(rp->data_source) || !open_newsrc(rp))
        {
            unlock_newsrc(rp);
            had_trouble = true;
        }
        else
        {
            rp->data_source->flags |= DF_ACTIVE;
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

void unuse_multirc(Multirc *mptr)
{
    if (!mptr)
    {
        return;
    }

    write_newsrcs(mptr);

    for (Newsrc *rp = mptr->first; rp; rp = rp->next)
    {
        unlock_newsrc(rp);
        rp->flags &= ~RF_ACTIVE;
        rp->data_source->flags &= ~DF_ACTIVE;
    }
    if (g_newsgroup_data_list)
    {
        close_cache();
        hash_destroy(g_newsrc_hash);
        walk_list(g_newsgroup_data_list, clear_newsgroup_item, 0);
        delete_list(g_newsgroup_data_list);
        g_newsgroup_data_list = nullptr;
        g_first_newsgroup = nullptr;
        g_last_newsgroup = nullptr;
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
        g_recent_newsgroup = nullptr;
        g_start_here = nullptr;
        g_sel_page_np = nullptr;
    }
    g_newsgroup_data_count = 0;
    g_newsgroup_count.num = 0;
    g_newsgroup_to_read.num= 0;
    g_multirc = nullptr;
}

bool use_next_multirc(Multirc *mptr)
{
    Multirc* mp = multirc_ptr(mptr->num);

    unuse_multirc(mptr);

    while (true)
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

bool use_prev_multirc(Multirc *mptr)
{
    Multirc *mp = multirc_ptr(mptr->num);

    unuse_multirc(mptr);

    while (true)
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

const char *multirc_name(const Multirc *mp)
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

static bool clear_newsgroup_item(char *cp, int arg)
{
    NewsgroupData* ncp = (NewsgroupData*)cp;

    if (ncp->rc_line != nullptr)
    {
        if (!g_check_flag)
        {
            std::free(ncp->rc_line);
        }
        ncp->rc_line = nullptr;
    }
    return false;
}

// make sure there is no trn out there reading this newsrc

static bool lock_newsrc(Newsrc *rp)
{
    long processnum = 0;

    if (g_check_flag)
    {
        return true;
    }

    char *s = file_exp(RCNAME);
    if (!std::strcmp(rp->name,s))
    {
        rp->lock_name = save_str(file_exp(LOCKNAME));
    }
    else
    {
        std::sprintf(g_buf, RCNAME_INFO, rp->name);
        rp->info_name = save_str(g_buf);
        std::sprintf(g_buf, RCNAME_LOCK, rp->name);
        rp->lock_name = save_str(g_buf);
    }

    char *runninghost;
    if (std::FILE *fp = std::fopen(rp->lock_name, "r"))
    {
        if (std::fgets(g_buf, LINE_BUF_LEN, fp))
        {
            processnum = std::atol(g_buf);
            if (std::fgets(g_buf, LINE_BUF_LEN, fp) && *g_buf //
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
        term_down(2);
        if (std::strcmp(runninghost, g_local_host))
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "Since that's not the same host as this one (%s), we must\n"
                       "assume that process still exists.  To override this check, remove\n"
                       "the lock file: %s\n",
                       g_local_host, rp->lock_name);
            }
            else
            {
                std::printf("\nThis host (%s) doesn't match.\nCan't unlock %s.\n",
                       g_local_host, rp->lock_name);
            }
            term_down(2);
            if (g_bizarre)
            {
                reset_tty();
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
            term_down(2);
            return false;
        }
        if (kill(processnum, 0) != 0)
        {
            // Process is apparently gone
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
            if (!g_last_newsgroup_name.empty())
            {
                if (g_verbose)
                {
                    std::printf("(The last newsgroup accessed was %s.)\n\n",
                           g_last_newsgroup_name.c_str());
                }
                else
                {
                    std::printf("(In %s.)\n\n",g_last_newsgroup_name.c_str());
                }
            }
            term_down(2);
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
                       rp->lock_name);
            }
            else
            {
                std::printf("\nCan't unlock %s.\n", rp->lock_name);
            }
            term_down(2);
            if (g_bizarre)
            {
                reset_tty();
            }
            finalize(0);
        }
#endif
    }
    std::FILE *fp = std::fopen(rp->lock_name, "w");
    if (fp == nullptr)
    {
        std::printf(g_cant_create,rp->lock_name);
        sig_catcher(0);
    }
    std::fprintf(fp,"%ld\n%s\n",g_our_pid,g_local_host);
    std::fclose(fp);
    return true;
}

static void unlock_newsrc(Newsrc *rp)
{
    safe_free0(rp->info_name);
    if (rp->lock_name)
    {
        remove(rp->lock_name);
        std::free(rp->lock_name);
        rp->lock_name = nullptr;
    }
}

static bool open_newsrc(Newsrc *rp)
{
    // make sure the .newsrc file exists

    std::FILE *rcfp = std::fopen(rp->name, "r");
    if (rcfp == nullptr)
    {
        rcfp = std::fopen(rp->name,"w+");
        if (rcfp == nullptr)
        {
            std::printf("\nCan't create %s.\n", rp->name);
            term_down(2);
            return false;
        }
        const char *some_buf = SUBSCRIPTIONS;
        if ((rp->data_source->flags & DF_REMOTE) //
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
            if (std::FILE *fp = std::fopen(file_exp(some_buf), "r"))
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
        // File exists; if zero length and backup isn't, complain
        stat_t newsrc_stat{};
        if (fstat(fileno(rcfp), &newsrc_stat) < 0)
        {
            std::perror(rp->name);
            return false;
        }
        if (newsrc_stat.st_size == 0 //
            && stat(rp->old_name, &newsrc_stat) >= 0 && newsrc_stat.st_size > 0)
        {
            std::printf("Warning: %s is zero length but %s is not.\n",
                   rp->name,rp->old_name);
            std::printf("Either recover your newsrc or else remove the backup copy.\n");
            term_down(2);
            return false;
        }
        // unlink backup file name and backup current name
        remove(rp->old_name);
#ifndef NO_FILELINKS
        safe_link(rp->name,rp->old_name);
#endif
    }

    if (!g_newsgroup_data_list)
    {
        // allocate memory for rc file globals
        g_newsgroup_data_list = new_list(0, 0, sizeof (NewsgroupData), 200, LF_NONE, init_newsgroup_node);
        g_newsrc_hash = hash_create(3001, rcline_cmp);
    }

    NewsgroupData*   prev_np;
    if (g_newsgroup_data_count)
    {
        prev_np = newsgroup_data_ptr(g_newsgroup_data_count - 1);
    }
    else
    {
        prev_np = nullptr;
    }

    // read in the .newsrc file

    char* some_buf;
    while ((some_buf = get_a_line(g_buf, LINE_BUF_LEN, false, rcfp)) != nullptr)
    {
        long length = g_len_last_line_got; // side effect of get_a_line
        if (length <= 1)                   // only a newline???
        {
            continue;
        }
        NewsgroupData *np = newsgroup_data_ptr(g_newsgroup_data_count++);
        if (prev_np)
        {
            prev_np->next = np;
        }
        else
        {
            g_first_newsgroup = np;
        }
        np->prev = prev_np;
        prev_np = np;
        np->rc = rp;
        g_newsgroup_count.num++;
        if (some_buf[length-1] == '\n')
        {
            some_buf[--length] = '\0';  // wipe out newline
        }
        if (some_buf == g_buf)
        {
            np->rc_line = save_str(some_buf);  // make semi-permanent copy
        }
        else
        {
            // NOSTRICT
#ifndef lint
            some_buf = safe_realloc(some_buf,(MemorySize)(length+1));
#endif
            np->rc_line = some_buf;
        }
        if (is_hor_space(*some_buf)                   //
            || !std::strncmp(some_buf, "options", 7)) // non-useful line?
        {
            np->to_read = TR_JUNK;
            np->subscribe_char = ' ';
            np->num_offset = 0;
            continue;
        }
        parse_rcline(np);
        HashDatum data = hash_fetch(g_newsrc_hash, np->rc_line, np->num_offset - 1);
        if (data.dat_ptr)
        {
            np->to_read = TR_IGNORE;
            continue;
        }
        if (np->subscribe_char == NEGCHAR)
        {
            np->to_read = TR_UNSUB;
            set_hash(np);
            continue;
        }
        g_newsgroup_to_read.num++;

        // now find out how much there is to read

        if (!in_list(g_buf) || (g_suppress_cn && s_found_any && !g_paranoid))
        {
            np->to_read = TR_NONE;       // no need to calculate now
        }
        else
        {
            set_to_read(np, ST_LAX);
        }
        if (np->to_read > TR_NONE)       // anything unread?
        {
            if (!s_found_any)
            {
                g_start_here = np;
                s_found_any = true;      // remember that fact
            }
            if (g_suppress_cn)          // if no listing desired
            {
                if (g_check_flag)        // if that is all they wanted
                {
                    finalize(1);        // then bomb out
                }
            }
            else
            {
                if (g_verbose)
                {
                    std::printf("Unread news in %-40s %5ld article%s\n",
                        np->rc_line,(long)np->to_read,plural(np->to_read));
                }
                else
                {
                    std::printf("%s: %ld article%s\n",
                        np->rc_line,(long)np->to_read,plural(np->to_read));
                }
                term_down(1);
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
                        if (g_check_flag)
                        {
                            finalize(1);
                        }
                        g_suppress_cn = true;
                    }
                }
            }
        }
        set_hash(np);
    }
    if (prev_np)
    {
        prev_np->next = nullptr;
        g_last_newsgroup = prev_np;
    }
    std::fclose(rcfp);                       // close .newsrc
#ifdef NO_FILELINKS
    remove(rp->old_name);
    rename(rp->name,rp->old_name);
    rp->flags |= RF_RC_CHANGED;
#endif
    if (rp->info_name)
    {
        std::FILE *info = std::fopen(rp->info_name, "r");
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
                    g_last_newsgroup_name = s+2;
                }
                if (std::fscanf(info, "New-Group-State: %ld,%ld,%ld", //
                                &g_last_new_time, &actnum, &descnum) == 3)
                {
                    rp->data_source->act_sf.recent_cnt = actnum;
                    rp->data_source->desc_sf.recent_cnt = descnum;
                }
            }
            std::fclose(info);
        }
    }
    else
    {
        read_last();
        if (rp->data_source->flags & DF_REMOTE)
        {
            rp->data_source->act_sf.recent_cnt = g_last_active_size;
            rp->data_source->desc_sf.recent_cnt = g_last_extra_num;
        }
        else
        {
            rp->data_source->act_sf.recent_cnt = g_last_extra_num;
            rp->data_source->desc_sf.recent_cnt = 0;
        }
    }
    rp->data_source->last_new_group = g_last_new_time;

    if (g_paranoid && !g_check_flag)
    {
        cleanup_newsrc(rp);
    }
    return true;
}

// Initialize the memory for an entire node's worth of article's
static void init_newsgroup_node(List *list, ListNode *node)
{
    std::memset(node->data,0,list->items_per_node * list->item_size);
    NewsgroupData *np = (NewsgroupData*)node->data;
    for (ArticleNum i{node->low}; i.num <= node->high; i.num++, np++)
    {
        np->num.num = i.num;
    }
}

static void parse_rcline(NewsgroupData *np)
{
    char* s;

    for (s=np->rc_line; *s && *s!=':' && *s!=NEGCHAR && !std::isspace(*s); s++)
    {
    }
    int len = s - np->rc_line;
    if ((!*s || std::isspace(*s)) && !g_check_flag)
    {
#ifndef lint
        np->rc_line = safe_realloc(np->rc_line,(MemorySize)len + 3);
#endif
        s = np->rc_line + len;
        std::strcpy(s, ": ");
    }
    if (*s == ':' && s[1] && s[2] == '0')
    {
        np->flags |= NF_UNTHREADED;
        s[2] = '1';
    }
    np->subscribe_char = *s;         // salt away the : or !
    np->num_offset = len + 1;        // remember where the numbers are
    *s = '\0';                      // null terminate newsgroup name
}

void abandon_newsgroup(NewsgroupData *np)
{
    char* some_buf = nullptr;

    // open newsrc backup copy and try to find the prior value for the group.
    std::FILE *rcfp = std::fopen(np->rc->old_name, "r");
    if (rcfp != nullptr)
    {
        int length = np->num_offset - 1;

        while ((some_buf = get_a_line(g_buf, LINE_BUF_LEN, false, rcfp)) != nullptr)
        {
            if (g_len_last_line_got <= 0)
            {
                continue;
            }
            some_buf[g_len_last_line_got-1] = '\0'; // wipe out newline
            if ((some_buf[length] == ':' || some_buf[length] == NEGCHAR) //
                && !std::strncmp(np->rc_line, some_buf, length))
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
        std::printf("Unable to open %s.\n", np->rc->old_name);
        term_down(1);
        return;
    }
    if (some_buf == nullptr)
    {
        some_buf = np->rc_line + np->num_offset;
        if (*some_buf == ' ')
        {
            some_buf++;
        }
        *some_buf = '\0';
        np->abs_first = ArticleNum{};         // force group to be re-calculated
    }
    else
    {
        std::free(np->rc_line);
        if (some_buf == g_buf)
        {
            np->rc_line = save_str(some_buf);
        }
        else
        {
            // NOSTRICT
#ifndef lint
            some_buf = safe_realloc(some_buf, (MemorySize)(g_len_last_line_got));
#endif // lint
            np->rc_line = some_buf;
        }
    }
    parse_rcline(np);
    if (np->subscribe_char == NEGCHAR)
    {
        np->subscribe_char = ':';
    }
    np->rc->flags |= RF_RC_CHANGED;
    set_to_read(np, ST_LAX);
}

// try to find or add an explicitly specified newsgroup
// returns true if found or added, false if not.
// assumes that we are chdir'ed to NEWSSPOOL

bool get_newsgroup(const char *what, GetNewsgroupFlags flags)
{
    const char *n_to_forget;
    char prompt_buf[128];

    if (g_verbose)
    {
        n_to_forget = "Type n to forget about this newsgroup.\n";
    }
    else
    {
        n_to_forget = "n to forget it.\n";
    }
    if (std::strchr(what, '/'))
    {
        dingaling();
        std::printf("\nBad newsgroup name.\n");
        term_down(2);
check_fuzzy_match:
        if (g_fuzzy_get && (flags & GNG_FUZZY))
        {
            flags &= ~GNG_FUZZY;
            if (find_close_match())
            {
                what = g_newsgroup_name.c_str();
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    set_newsgroup_name(what);
    g_newsgroup_ptr = find_newsgroup(g_newsgroup_name.c_str());
    if (g_newsgroup_ptr == nullptr)             // not in .newsrc?
    {
        Newsrc* rp;
        for (rp = g_multirc->first; rp; rp = rp->next)
        {
            if (!all_bits(rp->flags, RF_ADD_GROUPS | RF_ACTIVE))
            {
                continue;
            }
            // TODO: this may scan a datasrc multiple times...
            if (find_active_group(rp->data_source,g_buf,g_newsgroup_name.c_str(),g_newsgroup_name.length(),ArticleNum{}))
            {
                break; // TODO: let them choose which server
            }
        }
        if (!rp)
        {
            dingaling();
            if (g_verbose)
            {
                std::printf("\nNewsgroup %s does not exist!\n", g_newsgroup_name.c_str());
            }
            else
            {
                std::printf("\nNo %s!\n", g_newsgroup_name.c_str());
            }
            term_down(2);
            if (g_novice_delays)
            {
                sleep(2);
            }
            goto check_fuzzy_match;
        }
        AddNewType autosub;
        if (g_mode != MM_INITIALIZING || !(autosub = auto_subscribe(g_newsgroup_name.c_str())))
        {
            autosub = g_add_new_by_default;
        }
        if (autosub)
        {
            if (g_append_unsub)
            {
                std::printf("(Adding %s to end of your .newsrc %ssubscribed)\n",
                       g_newsgroup_name.c_str(), (autosub == ADDNEW_SUB)? "" : "un");
                term_down(1);
                g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name.c_str(), autosub);
            }
            else
            {
                if (autosub == ADDNEW_SUB)
                {
                    std::printf("(Subscribing to %s)\n", g_newsgroup_name.c_str());
                    term_down(1);
                    g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name.c_str(), autosub);
                }
                else
                {
                    std::printf("(Ignoring %s)\n", g_newsgroup_name.c_str());
                    term_down(1);
                    return false;
                }
            }
            flags &= ~GNG_RELOC;
        }
        else
        {
            if (g_verbose)
            {
                std::sprintf(prompt_buf, "\nNewsgroup %s not in .newsrc -- subscribe?", g_newsgroup_name.c_str());
            }
            else
            {
                std::sprintf(prompt_buf,"\nSubscribe %s?",g_newsgroup_name.c_str());
            }
reask_add:
            in_char(prompt_buf,MM_ADD_NEWSGROUP_PROMPT,"ynYN");
            print_cmd();
            newline();
            if (*g_buf == 'h')
            {
                if (g_verbose)
                {
                    std::printf("Type y or SP to subscribe to %s.\n"
                           "Type Y to subscribe to this and all remaining new groups.\n"
                           "Type N to leave all remaining new groups unsubscribed.\n",
                           g_newsgroup_name.c_str());
                    term_down(3);
                }
                else
                {
                    std::fputs("y or SP to subscribe, Y to subscribe all new groups, N to unsubscribe all\n",
                          stdout);
                    term_down(1);
                }
                std::fputs(n_to_forget,stdout);
                term_down(1);
                goto reask_add;
            }
            else if (*g_buf == 'n' || *g_buf == 'q')
            {
                if (g_append_unsub)
                {
                    g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name.c_str(), NEGCHAR);
                }
                return false;
            }
            else if (*g_buf == 'y')
            {
                g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name.c_str(), ':');
                flags |= GNG_RELOC;
            }
            else if (*g_buf == 'Y')
            {
                g_add_new_by_default = ADDNEW_SUB;
                if (g_append_unsub)
                {
                    std::printf("(Adding %s to end of your .newsrc subscribed)\n",
                           g_newsgroup_name.c_str());
                }
                else
                {
                    std::printf("(Subscribing to %s)\n", g_newsgroup_name.c_str());
                }
                term_down(1);
                g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name.c_str(), ':');
                flags &= ~GNG_RELOC;
            }
            else if (*g_buf == 'N')
            {
                g_add_new_by_default = ADDNEW_UNSUB;
                if (g_append_unsub)
                {
                    std::printf("(Adding %s to end of your .newsrc unsubscribed)\n",
                           g_newsgroup_name.c_str());
                    term_down(1);
                    g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name.c_str(), NEGCHAR);
                    flags &= ~GNG_RELOC;
                }
                else
                {
                    std::printf("(Ignoring %s)\n", g_newsgroup_name.c_str());
                    term_down(1);
                    return false;
                }
            }
            else
            {
                std::fputs(g_h_for_help,stdout);
                term_down(1);
                settle_down();
                goto reask_add;
            }
        }
    }
    else if (g_mode == MM_INITIALIZING)         // adding new groups during init?
    {
        return false;
    }
    else if (g_newsgroup_ptr->subscribe_char == NEGCHAR)  // unsubscribed?
    {
        if (g_verbose)
        {
            std::sprintf(prompt_buf, "\nNewsgroup %s is unsubscribed -- resubscribe?", g_newsgroup_name.c_str());
        }
        else
        {
            std::sprintf(prompt_buf, "\nResubscribe %s?", g_newsgroup_name.c_str());
        }
reask_unsub:
        in_char(prompt_buf,MM_RESUBSCRIBE_PROMPT,"yn");
        print_cmd();
        newline();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::printf("Type y or SP to resubscribe to %s.\n", g_newsgroup_name.c_str());
            }
            else
            {
                std::fputs("y or SP to resubscribe.\n", stdout);
            }
            std::fputs(n_to_forget,stdout);
            term_down(2);
            goto reask_unsub;
        }
        else if (*g_buf == 'n' || *g_buf == 'q')
        {
            return false;
        }
        else if (*g_buf == 'y')
        {
            char *cp = g_newsgroup_ptr->rc_line + g_newsgroup_ptr->num_offset;
            g_newsgroup_ptr->flags = (*cp && cp[1] == '0' ? NF_UNTHREADED : NF_NONE);
            g_newsgroup_ptr->subscribe_char = ':';
            g_newsgroup_ptr->rc->flags |= RF_RC_CHANGED;
            flags &= ~GNG_RELOC;
        }
        else
        {
            std::fputs(g_h_for_help,stdout);
            term_down(1);
            settle_down();
            goto reask_unsub;
        }
    }

    // now calculate how many unread articles in newsgroup

    set_to_read(g_newsgroup_ptr, ST_STRICT);
    if (flags & GNG_RELOC)
    {
        if (!relocate_newsgroup(g_newsgroup_ptr,NewsgroupNum{-1}))
        {
            return false;
        }
    }
    return g_newsgroup_ptr->to_read >= TR_NONE;
}

// add a newsgroup to the newsrc file (eventually)

static NewsgroupData *add_newsgroup(Newsrc *rp, const char *ngn, char_int c)
{
    NewsgroupData *np = newsgroup_data_ptr(g_newsgroup_data_count++);
    np->prev = g_last_newsgroup;
    if (g_last_newsgroup)
    {
        g_last_newsgroup->next = np;
    }
    else
    {
        g_first_newsgroup = np;
    }
    np->next = nullptr;
    g_last_newsgroup = np;
    g_newsgroup_count.num++;

    np->rc = rp;
    np->num_offset = std::strlen(ngn) + 1;
    np->rc_line = safe_malloc((MemorySize)(np->num_offset + 2));
    std::strcpy(np->rc_line,ngn);             // and copy over the name
    std::strcpy(np->rc_line + np->num_offset, " ");
    np->subscribe_char = c;              // subscribe or unsubscribe
    if (c != NEGCHAR)
    {
        g_newsgroup_to_read.num++;
    }
    np->to_read = TR_NONE;               // just for prettiness
    set_hash(np);                        // so we can find it again
    rp->flags |= RF_RC_CHANGED;
    return np;
}

bool relocate_newsgroup(NewsgroupData *move_np, NewsgroupNum newnum)
{
    NewsgroupData* np;
    int i;
    const char* dflt = (move_np!=g_current_newsgroup ? "$^.Lq" : "$^Lq");
    SelectionSortMode save_sort = g_sel_sort;

    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        if (newnum.num < 0)
        {
            // ask if they want to keep the current order
            in_char("Sort newsrc(s) using current sort order?",MM_DELETE_BOGUS_NEWSGROUPS_PROMPT, "yn"); // TODO: !'D'
            print_cmd();
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

    g_start_here = nullptr;                      // Disable this optimization
    if (move_np != g_last_newsgroup)
    {
        if (move_np->prev)
        {
            move_np->prev->next = move_np->next;
        }
        else
        {
            g_first_newsgroup = move_np->next;
        }
        move_np->next->prev = move_np->prev;

        move_np->prev = g_last_newsgroup;
        move_np->next = nullptr;
        g_last_newsgroup->next = move_np;
        g_last_newsgroup = move_np;
    }

    // Renumber the groups according to current order
    for (np = g_first_newsgroup, i = 0; np; np = np->next, i++)
    {
        np->num.num = i;
    }
    move_np->rc->flags |= RF_RC_CHANGED;

    if (newnum.num < 0)
    {
reask_reloc:
        unflush_output();               // disable any ^O in effect
        if (g_verbose)
        {
            std::printf("\nPut newsgroup where? [%s] ", dflt);
        }
        else
        {
            std::printf("\nPut where? [%s] ", dflt);
        }
        std::fflush(stdout);
        term_down(1);
reinp_reloc:
        eat_typeahead();
        get_cmd(g_buf);
        if (errno || *g_buf == '\f')    // if return from stop signal
        {
            goto reask_reloc;           // give them a prompt again
        }
        set_def(g_buf,dflt);
        print_cmd();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "\n"
                       "Type ^ to put the newsgroup first (position 0).\n"
                       "Type $ to put the newsgroup last (position %d).\n",
                       g_newsgroup_count.num - 1);
                std::printf("Type . to put it before the current newsgroup.\n"
                       "Type -newsgroup name to put it before that newsgroup.\n"
                       "Type +newsgroup name to put it after that newsgroup.\n"
                       "Type a number between 0 and %d to put it at that position.\n",
                       g_newsgroup_count.num - 1);
                std::printf("Type L for a listing of newsgroups and their positions.\n"
                       "Type q to abort the current action.\n");
            }
            else
            {
                std::printf("\n"
                       "\n"
                       "^ to put newsgroup first (pos 0).\n"
                       "$ to put last (pos %d).\n",
                       g_newsgroup_count.num - 1);
                std::printf(". to put before current newsgroup.\n"
                       "-newsgroup to put before newsgroup.\n"
                       "+newsgroup to put after.\n"
                       "number in 0-%d to put at that pos.\n"
                       "L for list of newsrc.\n"
                       "q to abort\n",
                       g_newsgroup_count.num - 1);
            }
            term_down(10);
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
            if (!finish_command(true))  // get rest of command
            {
                goto reinp_reloc;
            }
            newnum.num = std::atol(g_buf);
            newnum = std::max(newnum, NewsgroupNum{});
            if (newnum.num >= g_newsgroup_count.num)
            {
                newnum.num = g_newsgroup_count.num - 1;
            }
        }
        else if (*g_buf == '^')
        {
            newline();
            newnum.num = 0;
        }
        else if (*g_buf == '$')
        {
            newnum.num = g_newsgroup_count.num-1;
        }
        else if (*g_buf == '.')
        {
            newline();
            newnum = g_current_newsgroup->num;
        }
        else if (*g_buf == '-' || *g_buf == '+')
        {
            if (!finish_command(true))  // get rest of command
            {
                goto reinp_reloc;
            }
            np = find_newsgroup(g_buf+1);
            if (np == nullptr)
            {
                std::fputs("Not found.",stdout);
                goto reask_reloc;
            }
            newnum = np->num;
            if (*g_buf == '+')
            {
                newnum.num++;
            }
        }
        else
        {
            std::printf("\n%s",g_h_for_help);
            term_down(2);
            settle_down();
            goto reask_reloc;
        }
    }
    if (newnum.num < g_newsgroup_count.num - 1)
    {
        for (np = g_first_newsgroup; np; np = np->next)
        {
            if (np->num.num >= newnum.num)
            {
                break;
            }
        }
        if (!np || np == move_np)
        {
            return false;               // This can't happen...
        }

        g_last_newsgroup = move_np->prev;
        g_last_newsgroup->next = nullptr;

        move_np->prev = np->prev;
        move_np->next = np;

        if (np->prev)
        {
            np->prev->next = move_np;
        }
        else
        {
            g_first_newsgroup = move_np;
        }
        np->prev = move_np;

        move_np->num.num = newnum.num++;
        for (; np; np = np->next, newnum.num++)
        {
            np->num = newnum;
        }
    }
    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        g_sel_sort = g_sel_newsgroup_sort;
        sort_newsgroups();
        g_sel_sort = save_sort;
    }
    return true;
}

// List out the newsrc with annotations

void list_newsgroups()
{
    NewsgroupData* np;
    NewsgroupNum i;
    char tmpbuf[2048];
    static const char* status[] = {"(READ)","(UNSUB)","(DUP)","(BOGUS)","(JUNK)"};

    page_start();
    print_lines("  #  Status  Newsgroup\n", STANDOUT);
    for (np = g_first_newsgroup, i = NewsgroupNum{}; np && !g_int_count; np = np->next, i.num++)
    {
        if (np->to_read >= 0)
        {
            set_to_read(np, ST_LAX);
        }
        *(np->rc_line + np->num_offset - 1) = np->subscribe_char;
        if (np->to_read > 0)
        {
            std::sprintf(tmpbuf, "%3d %6ld   ", i.num, (long) np->to_read);
        }
        else
        {
            std::sprintf(tmpbuf, "%3d %7s  ", i.num, status[-np->to_read]);
        }
        safe_copy(tmpbuf+13, np->rc_line, sizeof tmpbuf - 13);
        *(np->rc_line + np->num_offset - 1) = '\0';
        if (print_lines(tmpbuf, NO_MARKING) != 0)
        {
            break;
        }
    }
    g_int_count = 0;
}

// find a newsgroup in any newsrc

NewsgroupData *find_newsgroup(const char *ngnam)
{
    HashDatum data = hash_fetch(g_newsrc_hash, ngnam, std::strlen(ngnam));
    return (NewsgroupData*)data.dat_ptr;
}

void cleanup_newsrc(Newsrc *rp)
{
    NewsgroupNum bogosity{};

    if (g_verbose)
    {
        std::printf("Checking out '%s' -- hang on a second...\n", rp->name);
    }
    else
    {
        std::printf("Checking '%s' -- hang on...\n", rp->name);
    }
    term_down(1);
    NewsgroupData* np;
    for (np = g_first_newsgroup; np; np = np->next)
    {
// #ifdef CHECK_ALL_BOGUS TODO: what is this?
        if (np->to_read >= TR_UNSUB)
        {
            set_to_read(np, ST_LAX); // this may reset the group or declare it bogus
        }
// #endif
        if (np->to_read == TR_BOGUS)
        {
            bogosity.num++;
        }
    }
    for (np = g_last_newsgroup; np && np->to_read == TR_BOGUS; np = np->prev)
    {
        bogosity.num--;                     // discount already moved ones
    }
    if (g_newsgroup_count.num > 5 && bogosity.num > g_newsgroup_count.num / 2)
    {
        std::fputs("It looks like the active file is messed up.  Contact your news administrator,\n",
              stdout);
        std::fputs("leave the \"bogus\" groups alone, and they may come back to normal.  Maybe.\n",
              stdout);
        term_down(2);
    }
    else if (bogosity.num)
    {
        if (g_verbose)
        {
            std::printf("Moving bogus newsgroups to the end of '%s'.\n", rp->name);
        }
        else
        {
            std::fputs("Moving boguses to the end.\n", stdout);
        }
        term_down(1);
        while (np)
        {
            NewsgroupData *prev_np = np->prev;
            if (np->to_read == TR_BOGUS)
            {
                relocate_newsgroup(np, NewsgroupNum{g_newsgroup_count.num - 1});
            }
            np = prev_np;
        }
        rp->flags |= RF_RC_CHANGED;
reask_bogus:
        in_char("Delete bogus newsgroups?", MM_DELETE_BOGUS_NEWSGROUPS_PROMPT, "ny");
        print_cmd();
        newline();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::fputs("Type y to delete bogus newsgroups.\n"
                      "Type n or SP to leave them at the end in case they return.\n",
                      stdout);
                term_down(2);
            }
            else
            {
                std::fputs("y to delete, n to keep\n",stdout);
                term_down(1);
            }
            goto reask_bogus;
        }
        else if (*g_buf == 'n' || *g_buf == 'q')
        {
        }
        else if (*g_buf == 'y')
        {
            for (np = g_last_newsgroup; np && np->to_read == TR_BOGUS; np = np->prev)
            {
                hash_delete(g_newsrc_hash, np->rc_line, np->num_offset - 1);
                clear_newsgroup_item((char*)np,0);
                g_newsgroup_count.num--;
            }
            rp->flags |= RF_RC_CHANGED; // TODO: needed?
            g_last_newsgroup = np;
            if (np)
            {
                np->next = nullptr;
            }
            else
            {
                g_first_newsgroup = nullptr;
            }
            if (g_current_newsgroup && !g_current_newsgroup->rc_line)
            {
                g_current_newsgroup = g_first_newsgroup;
            }
            if (g_recent_newsgroup && !g_recent_newsgroup->rc_line)
            {
                g_recent_newsgroup = g_first_newsgroup;
            }
            if (g_newsgroup_ptr && !g_newsgroup_ptr->rc_line)
            {
                g_newsgroup_ptr = g_first_newsgroup;
            }
            if (g_sel_page_np && !g_sel_page_np->rc_line)
            {
                g_sel_page_np = nullptr;
            }
        }
        else
        {
            std::fputs(g_h_for_help,stdout);
            term_down(1);
            settle_down();
            goto reask_bogus;
        }
    }
    g_paranoid = false;
}

// make an entry in the hash table for the current newsgroup

void set_hash(NewsgroupData *np)
{
    HashDatum data;
    data.dat_ptr = (char*)np;
    data.dat_len = np->num_offset - 1;
    hash_store(g_newsrc_hash, np->rc_line, data.dat_len, data);
}

static int rcline_cmp(const char *key, int keylen, HashDatum data)
{
    // We already know that the lengths are equal, just compare the strings
    return std::memcmp(key, ((NewsgroupData*)data.dat_ptr)->rc_line, keylen);
}

// checkpoint the newsrc(s)

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
        bits_to_rc();                   // do not restore M articles
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

// write out the (presumably) revised newsrc(s)

bool write_newsrcs(Multirc *mptr)
{
    SelectionSortMode save_sort = g_sel_sort;
    bool          total_success = true;

    if (!mptr)
    {
        return true;
    }

    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        g_sel_sort = SS_NATURAL;
        g_sel_direction = 1;
        sort_newsgroups();
    }

    for (Newsrc *rp = mptr->first; rp; rp = rp->next)
    {
        if (!(rp->flags & RF_ACTIVE))
        {
            continue;
        }

        if (rp->info_name)
        {
            std::FILE *info = std::fopen(rp->info_name, "w");
            if (info != nullptr)
            {
                std::fprintf(info,"Last-Group: %s\nNew-Group-State: %ld,%ld,%ld\n",
                        g_newsgroup_name.c_str(),rp->data_source->last_new_group,
                        rp->data_source->act_sf.recent_cnt,
                        rp->data_source->desc_sf.recent_cnt);
                std::fclose(info);
            }
        }
        else
        {
            read_last();
            if (rp->data_source->flags & DF_REMOTE)
            {
                g_last_active_size = rp->data_source->act_sf.recent_cnt;
                g_last_extra_num = rp->data_source->desc_sf.recent_cnt;
            }
            else
            {
                g_last_extra_num = rp->data_source->act_sf.recent_cnt;
            }
            g_last_new_time = rp->data_source->last_new_group;
            write_last();
        }

        if (!(rp->flags & RF_RC_CHANGED))
        {
            continue;
        }

        std::FILE *rcfp = std::fopen(rp->new_name, "w");
        if (rcfp == nullptr)
        {
            std::printf(s_cant_recreate,rp->name);
            total_success = false;
            continue;
        }
#ifndef MSDOS
        stat_t perms;
        if (stat(rp->name,&perms)>=0)   // preserve permissions
        {
            chmod(rp->new_name,perms.st_mode&0666);
            chown(rp->new_name,perms.st_uid,perms.st_gid);
        }
#endif
        // write out each line

        for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
        {
            char* delim;
            if (np->rc != rp)
            {
                continue;
            }
            if (np->num_offset)
            {
                delim = np->rc_line + np->num_offset - 1;
                *delim = np->subscribe_char;
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
                term_down(1);
            }
#endif
            if (std::fprintf(rcfp, "%s\n", np->rc_line) < 0)
            {
                std::fclose(rcfp);           // close new newsrc
                goto write_error;
            }
            if (delim)
            {
                *delim = '\0';          // might still need this line
                if ((np->flags & NF_UNTHREADED) && delim[2] == '0')
                {
                    delim[2] = '1';
                }
            }
        }
        std::fflush(rcfp);
        // fclose is the only sure test for full disks via NFS
        if (std::ferror(rcfp))
        {
            std::fclose(rcfp);
            goto write_error;
        }
        if (std::fclose(rcfp) == EOF)
        {
write_error:
            std::printf(s_cant_recreate,rp->name);
            remove(rp->new_name);
            total_success = false;
            continue;
        }
        rp->flags &= ~RF_RC_CHANGED;

        remove(rp->name);
        rename(rp->new_name,rp->name);
    }

    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        g_sel_sort = g_sel_newsgroup_sort;
        sort_newsgroups();
        g_sel_sort = save_sort;
    }
    return total_success;
}

void get_old_newsrcs(Multirc *mptr)
{
    if (mptr)
    {
        for (Newsrc *rp = mptr->first; rp; rp = rp->next)
        {
            if (rp->flags & RF_ACTIVE)
            {
                remove(rp->new_name);
                rename(rp->name,rp->new_name);
                rename(rp->old_name,rp->name);
            }
        }
    }
}
