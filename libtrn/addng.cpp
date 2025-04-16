/* addng.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/addng.h"

#include "nntp/nntpclient.h"
#include "trn/List.h"
#include "trn/ngdata.h"
#include "trn/autosub.h"
#include "trn/datasrc.h"
#include "trn/hash.h"
#include "trn/nntp.h"
#include "trn/only.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <ctime>

AddGroup *g_first_addgroup{};
AddGroup *g_last_addgroup{};
AddGroup *g_sel_page_gp{};
AddGroup *g_sel_next_gp{};
bool      g_quickstart{};           /* -q */
bool      g_use_add_selector{true}; //

static int s_addgroup_cnt{};

static int  addng_cmp(const char *key, int keylen, HashDatum data);
static int  build_addgroup_list(int keylen, HashDatum *data, int extra);
static void process_list(GetNewsgroupFlags flag);
static void new_nntp_groups(DataSource *dp);
static void new_local_groups(DataSource *dp);
static void add_to_hash(HASHTABLE *ng, const char *name, int toread, char_int ch);
static void add_to_list(const char *name, int toread, char_int ch);
static int  list_groups(int keylen, HashDatum *data, int add_matching);
static void scanline(char *actline, bool add_matching);
static int  agorder_number(const AddGroup **app1, const AddGroup **app2);
static int  agorder_groupname(const AddGroup **app1, const AddGroup **app2);
static int  agorder_count(const AddGroup **app1, const AddGroup **app2);

static int addng_cmp(const char *key, int keylen, HashDatum data)
{
    return memcmp(key, ((AddGroup *)data.dat_ptr)->name, keylen);
}

static int build_addgroup_list(int keylen, HashDatum *data, int extra)
{
    AddGroup* node = (AddGroup*)data->dat_ptr;

    node->num = s_addgroup_cnt++;
    node->next = nullptr;
    node->prev = g_last_addgroup;
    if (g_last_addgroup)
    {
        g_last_addgroup->next = node;
    }
    else
    {
        g_first_addgroup = node;
    }
    g_last_addgroup = node;
    return 0;
}

void addng_init()
{
}

bool find_new_groups()
{
    NewsgroupNum const oldcnt = g_newsgroup_cnt;      /* remember # newsgroups */

    /* Skip this check if the -q flag was given. */
    if (g_quickstart)
    {
        return false;
    }

    for (Newsrc const *rp = g_multirc->first; rp; rp = rp->next)
    {
        if (all_bits(rp->flags, RF_ADD_NEWGROUPS | RF_ACTIVE))
        {
            if (rp->datasrc->flags & DF_REMOTE)
            {
                new_nntp_groups(rp->datasrc);
            }
            else
            {
                new_local_groups(rp->datasrc);
            }
        }
    }
    g_addnewbydefault = ADDNEW_ASK;

    process_list(GNG_RELOC);

    return oldcnt != g_newsgroup_cnt;
}

static void process_list(GetNewsgroupFlags flag)
{
    if (flag == GNG_NONE)
    {
        std::sprintf(g_cmd_buf,
                "\n"
                "Unsubscribed but mentioned in your current newsrc%s:\n",
                g_multirc->first->next ? "s" : "");
        print_lines(g_cmd_buf, STANDOUT);
    }
    AddGroup *node = g_first_addgroup;
    if (node != nullptr && flag != GNG_NONE && g_use_add_selector)
    {
        addgroup_selector(flag);
    }
    while (node)
    {
        if (flag == GNG_NONE)
        {
            std::sprintf(g_cmd_buf, "%s\n", node->name);
            print_lines(g_cmd_buf, NOMARKING);
        }
        else if (!g_use_add_selector)
        {
            get_ng(node->name, flag); /* add newsgroup -- maybe */
        }
        AddGroup *prevnode = node;
        node = node->next;
        std::free((char*)prevnode);
    }
    g_first_addgroup = nullptr;
    g_last_addgroup = nullptr;
    s_addgroup_cnt = 0;
}

static void new_nntp_groups(DataSource *dp)
{
    char* s;
    int len;
    bool  foundSomething = false;
    long  high;
    long  low;

    set_datasrc(dp);

    std::time_t const server_time = nntp_time();
    if (server_time == -2)
    {
        return;
    }
    if (nntp_newgroups(dp->lastnewgrp) < 1)
    {
        std::printf("Can't get new groups from server:\n%s\n", g_ser_line);
        return;
    }
    HASHTABLE *newngs = hashcreate(33, addng_cmp);

    while (true)
    {
        high = 0;
        low = 1;
        if (nntp_gets(g_ser_line, sizeof g_ser_line) == NGSR_ERROR)
        {
            break;
        }
#ifdef DEBUG
        if (debug & DEB_NNTP)
        {
            std::printf("<%s\n", g_ser_line);
        }
#endif
        if (nntp_at_list_end(g_ser_line))
        {
            break;
        }
        foundSomething = true;
        s = std::strchr(g_ser_line, ' ');
        if (s != nullptr)
        {
            len = s - g_ser_line;
        }
        else
        {
            len = std::strlen(g_ser_line);
        }
        if (dp->act_sf.fp)
        {
            if (find_actgrp(dp, g_buf, g_ser_line, len, (ArticleNum) 0))
            {
                if (!s)
                {
                    s = g_buf + len + 1;
                }
            }
            else
            {
                char ch = 'y';
                if (s)
                {
                    std::sscanf(s + 1, "%ld %ld %c", &high, &low, &ch);
                }
                else
                {
                    s = g_ser_line + len;
                }
                std::sprintf(s, " %010ld %05ld %c\n", high, low, ch);
                (void) srcfile_append(&dp->act_sf, g_ser_line, len);
            }
        }
        if (s)
        {
            *s++ = '\0';
            while (std::isdigit(*s) || std::isspace(*s))
            {
                s++;
            }
            if (*s == 'x' || *s == '=')
            {
                continue;
            }
        }
        NewsgroupData *np = find_ng(g_ser_line);
        if (np != nullptr && np->toread > TR_UNSUB)
        {
            continue;
        }
        add_to_hash(newngs, g_ser_line, high-low, auto_subscribe(g_ser_line));
    }
    if (foundSomething)
    {
        hashwalk(newngs, build_addgroup_list, 0);
        srcfile_end_append(&dp->act_sf, dp->extra_name);
        dp->lastnewgrp = server_time;
    }
    hashdestroy(newngs);
}

static void new_local_groups(DataSource *dp)
{
    g_datasrc = dp;

    const long file_size{static_cast<long>(std::filesystem::file_size(dp->extra_name))};
    /* did active.times file grow? */
    if (file_size == dp->act_sf.recent_cnt)
    {
        return;
    }

    std::FILE *fp = std::fopen(dp->extra_name, "r");
    if (fp == nullptr)
    {
        std::printf(g_cantopen, dp->extra_name);
        termdown(1);
        return;
    }
    std::time_t lastone = std::time(nullptr) - 24L * 60 * 60 - 1;
    HASHTABLE *newngs = hashcreate(33, addng_cmp);

    while (std::fgets(g_buf, LBUFLEN, fp) != nullptr)
    {
        char *s;
        if ((s = std::strchr(g_buf, ' ')) == nullptr || (lastone = std::atol(s + 1)) < dp->lastnewgrp)
        {
            continue;
        }
        *s = '\0';
        char tmpbuf[LBUFLEN];
        if (!find_actgrp(g_datasrc, tmpbuf, g_buf, s - g_buf, (ArticleNum)0))
        {
            continue;
        }
        long high;
        long low;
        char ch;
        std::sscanf(tmpbuf + (s-g_buf) + 1, "%ld %ld %c", &high, &low, &ch);
        if (ch == 'x' || ch == '=')
        {
            continue;
        }
        NewsgroupData *np = find_ng(g_buf);
        if (np != nullptr)
        {
            continue;
        }
        add_to_hash(newngs, g_buf, high-low, auto_subscribe(g_buf));
    }
    std::fclose(fp);

    hashwalk(newngs, build_addgroup_list, 0);
    hashdestroy(newngs);
    dp->lastnewgrp = lastone+1;
    dp->act_sf.recent_cnt = file_size;
}

static void add_to_hash(HASHTABLE *ng, const char *name, int toread, char_int ch)
{
    HashDatum data;
    unsigned const namelen = std::strlen(name);
    data.dat_len = namelen + sizeof (AddGroup);
    AddGroup *node = (AddGroup*)safemalloc(data.dat_len);
    data.dat_ptr = (char *)node;
    switch (ch)
    {
    case ':':
        node->flags = AGF_SEL;
        break;
    case '!':
        node->flags = AGF_DEL;
        break;
    default:
        node->flags = AGF_NONE;
        break;
    }
    node->toread = (toread < 0)? 0 : toread;
    std::strcpy(node->name, name);
    node->datasrc = g_datasrc;
    node->next = nullptr;
    node->prev = nullptr;
    hashstore(ng, name, namelen, data);
}

static void add_to_list(const char *name, int toread, char_int ch)
{
    AddGroup* node = g_first_addgroup;

    while (node)
    {
        if (!std::strcmp(node->name,name))
        {
            return;
        }
        node = node->next;
    }

    node = (AddGroup*)safemalloc(std::strlen(name) + sizeof (AddGroup));
    switch (ch)
    {
    case ':':
        node->flags = AGF_SEL;
        break;
    case '!':
        node->flags = AGF_DEL;
        break;
    default:
        node->flags = AGF_NONE;
        break;
    }
    node->toread = (toread < 0)? 0 : toread;
    node->num = s_addgroup_cnt++;
    std::strcpy(node->name, name);
    node->datasrc = g_datasrc;
    node->next = nullptr;
    node->prev = g_last_addgroup;
    if (g_last_addgroup)
    {
        g_last_addgroup->next = node;
    }
    else
    {
        g_first_addgroup = node;
    }
    g_last_addgroup = node;
}

bool scanactive(bool add_matching)
{
    NewsgroupNum const oldcnt = g_newsgroup_cnt;      /* remember # of newsgroups */

    if (!add_matching)
    {
        print_lines("Completely unsubscribed newsgroups:\n", STANDOUT);
    }

    for (DataSource *dp = datasrc_first(); dp && !empty(dp->name); dp = datasrc_next(dp))
    {
        if (!(dp->flags & DF_OPEN))
        {
            continue;
        }
        set_datasrc(dp);
        if (dp->act_sf.fp)
        {
            hashwalk(dp->act_sf.hp, list_groups, add_matching);
        }
        else
        {
            if (g_maxngtodo != 1)
            {
                std::strcpy(g_buf, "*");
            }
            else
            {
                if (g_ngtodo[0][0] == '^')
                {
                    std::sprintf(g_buf, "%s*", &g_ngtodo[0][1]);
                }
                else
                {
                    std::sprintf(g_buf, "*%s*", g_ngtodo[0]);
                }
                if (g_buf[std::strlen(g_buf)-2] == '$')
                {
                    g_buf[std::strlen(g_buf) - 2] = '\0';
                }
            }
            if (nntp_list("active", g_buf, std::strlen(g_buf)) == 1)
            {
                while (!nntp_at_list_end(g_ser_line))
                {
                    scanline(g_ser_line,add_matching);
                    if (nntp_gets(g_ser_line, sizeof g_ser_line) == NGSR_ERROR)
                    {
                        break;
                    }
                }
            }
        }
    }

    process_list(add_matching ? GNG_RELOC : GNG_NONE);

    if (g_in_ng)
    {
        set_datasrc(g_ngptr->rc->datasrc);
    }

    return oldcnt != g_newsgroup_cnt;
}

static int list_groups(int keylen, HashDatum *data, int add_matching)
{
    char* bp = ((ListNode*)data->dat_ptr)->data + data->dat_len;
    int const linelen = std::strchr(bp, '\n') - bp + 1;
    (void) memcpy(g_buf,bp,linelen);
    g_buf[linelen] = '\0';
    scanline(g_buf,add_matching);
    return 0;
}

static void scanline(char *actline, bool add_matching)
{
    char *s = std::strchr(actline, ' ');
    if (s == nullptr)
    {
        return;
    }

    *s++ = '\0';                /* this buffer is expendable */
    long high;
    long low;
    char ch;
    high = 0;
    low = 1;
    ch = 'y';
    std::sscanf(s, "%ld %ld %c", &high, &low, &ch);
    if (ch == 'x' || !std::strncmp(actline,"to.",3))
    {
        return;
    }
    if (!inlist(actline))
    {
        return;
    }
    NewsgroupData *np = find_ng(actline);
    if (np != nullptr && np->toread > TR_UNSUB)
    {
        return;
    }
    if (add_matching || np)
    {
        /* it's not in a newsrc */
        add_to_list(actline, high-low, 0);
    }
    else
    {
        std::strcat(actline,"\n");
        print_lines(actline, NOMARKING);
    }
}

static int agorder_number(const AddGroup **app1, const AddGroup **app2)
{
    ArticleNum const eq = (*app1)->num - (*app2)->num;
    return eq > 0? g_sel_direction : -g_sel_direction;
}

static int agorder_groupname(const AddGroup **app1, const AddGroup **app2)
{
    return string_case_compare((*app1)->name, (*app2)->name) * g_sel_direction;
}

static int agorder_count(const AddGroup **app1, const AddGroup **app2)
{
    long const eq = (*app1)->toread - (*app2)->toread;
    if (eq)
    {
        return eq > 0 ? g_sel_direction : -g_sel_direction;
    }
    return agorder_groupname(app1, app2);
}

/* Sort the newsgroups into the chosen order.
*/
void sort_addgroups()
{
    AddGroup* ap;
    AddGroup** lp;
    int (*sort_procedure)(const AddGroup**, const AddGroup**);

    switch (g_sel_sort)
    {
    case SS_NATURAL:
    default:
        sort_procedure = agorder_number;
        break;
    case SS_STRING:
        sort_procedure = agorder_groupname;
        break;
    case SS_COUNT:
        sort_procedure = agorder_count;
        break;
    }

    AddGroup **ag_list = (AddGroup**)safemalloc(s_addgroup_cnt * sizeof(AddGroup*));
    for (lp = ag_list, ap = g_first_addgroup; ap; ap = ap->next)
    {
        *lp++ = ap;
    }
    TRN_ASSERT(lp - ag_list == s_addgroup_cnt);

    std::qsort(ag_list, s_addgroup_cnt, sizeof(AddGroup*), (int(*)(void const *, void const *)) sort_procedure);

    ap = ag_list[0];
    g_first_addgroup = ag_list[0];
    ap->prev = nullptr;
    lp = ag_list;
    for (int i = s_addgroup_cnt; --i; ++lp)
    {
        lp[0]->next = lp[1];
        lp[1]->prev = lp[0];
    }
    g_last_addgroup = lp[0];
    g_last_addgroup->next = nullptr;
    std::free((char*)ag_list);
}
