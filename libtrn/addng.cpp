/* addng.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/addng.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
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

AddGroup *g_first_add_group{};
AddGroup *g_last_add_group{};
AddGroup *g_sel_page_gp{};
AddGroup *g_sel_next_gp{};
bool      g_quick_start{};           // -q
bool      g_use_add_selector{true}; //

static int s_add_group_count{};

static int  add_newsgroup_cmp(const char *key, int key_len, HashDatum data);
static int  build_add_group_list(int key_len, HashDatum *data, int extra);
static void process_list(GetNewsgroupFlags flag);
static void new_nntp_groups(DataSource *dp);
static void new_local_groups(DataSource *dp);
static void add_to_hash(HashTable *ng, const char *name, int to_read, char_int ch);
static void add_to_list(const char *name, int to_read, char_int ch);
static int  list_groups(int key_len, HashDatum *data, int add_matching);
static void scan_active_line(char *active_line, bool add_matching);
static int  add_group_order_number(const AddGroup **app1, const AddGroup **app2);
static int  add_group_order_group_name(const AddGroup **app1, const AddGroup **app2);
static int  add_group_order_count(const AddGroup **app1, const AddGroup **app2);

static int add_newsgroup_cmp(const char *key, int key_len, HashDatum data)
{
    return std::memcmp(key, ((AddGroup *)data.dat_ptr)->name, key_len);
}

static int build_add_group_list(int key_len, HashDatum *data, int extra)
{
    AddGroup* node = (AddGroup*)data->dat_ptr;

    value_of(node->num) = s_add_group_count++;
    node->next = nullptr;
    node->prev = g_last_add_group;
    if (g_last_add_group)
    {
        g_last_add_group->next = node;
    }
    else
    {
        g_first_add_group = node;
    }
    g_last_add_group = node;
    return 0;
}

void add_ng_init()
{
}

bool find_new_groups()
{
    const NewsgroupNum old_cnt = g_newsgroup_count;      // remember # newsgroups

    // Skip this check if the -q flag was given.
    if (g_quick_start)
    {
        return false;
    }

    for (const Newsrc *rp = g_multirc->first; rp; rp = rp->next)
    {
        if (all_bits(rp->flags, RF_ADD_NEW_GROUPS | RF_ACTIVE))
        {
            if (rp->data_source->flags & DF_REMOTE)
            {
                new_nntp_groups(rp->data_source);
            }
            else
            {
                new_local_groups(rp->data_source);
            }
        }
    }
    g_add_new_by_default = ADDNEW_ASK;

    process_list(GNG_RELOC);

    return old_cnt != g_newsgroup_count;
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
    AddGroup *node = g_first_add_group;
    if (node != nullptr && flag != GNG_NONE && g_use_add_selector)
    {
        add_group_selector(flag);
    }
    while (node)
    {
        if (flag == GNG_NONE)
        {
            std::sprintf(g_cmd_buf, "%s\n", node->name);
            print_lines(g_cmd_buf, NO_MARKING);
        }
        else if (!g_use_add_selector)
        {
            get_newsgroup(node->name, flag); // add newsgroup -- maybe
        }
        AddGroup *prev_node = node;
        node = node->next;
        std::free((char*)prev_node);
    }
    g_first_add_group = nullptr;
    g_last_add_group = nullptr;
    s_add_group_count = 0;
}

static void new_nntp_groups(DataSource *dp)
{
    char* s;
    int len;
    bool  found_something = false;
    long  high;
    long  low;

    set_data_source(dp);

    const std::time_t server_time = nntp_time();
    if (server_time == -2)
    {
        return;
    }
    if (nntp_new_groups(dp->last_new_group) < 1)
    {
        std::printf("Can't get new groups from server:\n%s\n", g_ser_line);
        return;
    }
    HashTable *new_newsgroups = hash_create(33, add_newsgroup_cmp);

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
        found_something = true;
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
            if (find_active_group(dp, g_buf, g_ser_line, len, ArticleNum{}))
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
                (void) source_file_append(&dp->act_sf, g_ser_line, len);
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
        NewsgroupData *np = find_newsgroup(g_ser_line);
        if (np != nullptr && np->to_read > TR_UNSUB)
        {
            continue;
        }
        add_to_hash(new_newsgroups, g_ser_line, high-low, auto_subscribe(g_ser_line));
    }
    if (found_something)
    {
        hash_walk(new_newsgroups, build_add_group_list, 0);
        source_file_end_append(&dp->act_sf, dp->extra_name);
        dp->last_new_group = server_time;
    }
    hash_destroy(new_newsgroups);
}

static void new_local_groups(DataSource *dp)
{
    g_data_source = dp;

    const long file_size{static_cast<long>(std::filesystem::file_size(dp->extra_name))};
    // did active.times file grow?
    if (file_size == dp->act_sf.recent_cnt)
    {
        return;
    }

    std::FILE *fp = std::fopen(dp->extra_name, "r");
    if (fp == nullptr)
    {
        std::printf(g_cant_open, dp->extra_name);
        term_down(1);
        return;
    }
    std::time_t last_one = std::time(nullptr) - 24L * 60 * 60 - 1;
    HashTable *new_newsgroups = hash_create(33, add_newsgroup_cmp);

    while (std::fgets(g_buf, LINE_BUF_LEN, fp) != nullptr)
    {
        char *s;
        if ((s = std::strchr(g_buf, ' ')) == nullptr || (last_one = std::atol(s + 1)) < dp->last_new_group)
        {
            continue;
        }
        *s = '\0';
        char tmp_buf[LINE_BUF_LEN];
        if (!find_active_group(g_data_source, tmp_buf, g_buf, s - g_buf, ArticleNum{}))
        {
            continue;
        }
        long high;
        long low;
        char ch;
        std::sscanf(tmp_buf + (s-g_buf) + 1, "%ld %ld %c", &high, &low, &ch);
        if (ch == 'x' || ch == '=')
        {
            continue;
        }
        NewsgroupData *np = find_newsgroup(g_buf);
        if (np != nullptr)
        {
            continue;
        }
        add_to_hash(new_newsgroups, g_buf, high-low, auto_subscribe(g_buf));
    }
    std::fclose(fp);

    hash_walk(new_newsgroups, build_add_group_list, 0);
    hash_destroy(new_newsgroups);
    dp->last_new_group = last_one+1;
    dp->act_sf.recent_cnt = file_size;
}

static void add_to_hash(HashTable *ng, const char *name, int to_read, char_int ch)
{
    HashDatum      data;
    const unsigned name_len = std::strlen(name);
    data.dat_len = name_len + sizeof (AddGroup);
    AddGroup *node = (AddGroup*)safe_malloc(data.dat_len);
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
    value_of(node->to_read) = (to_read < 0) ? 0 : to_read;
    std::strcpy(node->name, name);
    node->data_src = g_data_source;
    node->next = nullptr;
    node->prev = nullptr;
    hash_store(ng, name, name_len, data);
}

static void add_to_list(const char *name, int to_read, char_int ch)
{
    AddGroup* node = g_first_add_group;

    while (node)
    {
        if (!std::strcmp(node->name,name))
        {
            return;
        }
        node = node->next;
    }

    node = (AddGroup*)safe_malloc(std::strlen(name) + sizeof (AddGroup));
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
    value_of(node->to_read) = (to_read < 0) ? 0 : to_read;
    value_of(node->num) = s_add_group_count++;
    std::strcpy(node->name, name);
    node->data_src = g_data_source;
    node->next = nullptr;
    node->prev = g_last_add_group;
    if (g_last_add_group)
    {
        g_last_add_group->next = node;
    }
    else
    {
        g_first_add_group = node;
    }
    g_last_add_group = node;
}

bool scan_active(bool add_matching)
{
    const NewsgroupNum old_count = g_newsgroup_count;      // remember # of newsgroups

    if (!add_matching)
    {
        print_lines("Completely unsubscribed newsgroups:\n", STANDOUT);
    }

    for (DataSource *dp = data_source_first(); dp && !empty(dp->name); dp = data_source_next(dp))
    {
        if (!(dp->flags & DF_OPEN))
        {
            continue;
        }
        set_data_source(dp);
        if (dp->act_sf.fp)
        {
            hash_walk(dp->act_sf.hp, list_groups, add_matching);
        }
        else
        {
            if (g_max_newsgroup_to_do != 1)
            {
                std::strcpy(g_buf, "*");
            }
            else
            {
                if (g_newsgroup_to_do[0][0] == '^')
                {
                    std::sprintf(g_buf, "%s*", &g_newsgroup_to_do[0][1]);
                }
                else
                {
                    std::sprintf(g_buf, "*%s*", g_newsgroup_to_do[0]);
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
                    scan_active_line(g_ser_line,add_matching);
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
        set_data_source(g_newsgroup_ptr->rc->data_source);
    }

    return old_count != g_newsgroup_count;
}

static int list_groups(int key_len, HashDatum *data, int add_matching)
{
    char     *bp = ((ListNode *) data->dat_ptr)->data + data->dat_len;
    const int line_len = std::strchr(bp, '\n') - bp + 1;
    (void) std::memcpy(g_buf,bp,line_len);
    g_buf[line_len] = '\0';
    scan_active_line(g_buf,add_matching);
    return 0;
}

static void scan_active_line(char *active_line, bool add_matching)
{
    char *s = std::strchr(active_line, ' ');
    if (s == nullptr)
    {
        return;
    }

    *s++ = '\0';                // this buffer is expendable
    long high;
    long low;
    char ch;
    high = 0;
    low = 1;
    ch = 'y';
    std::sscanf(s, "%ld %ld %c", &high, &low, &ch);
    if (ch == 'x' || !std::strncmp(active_line,"to.",3))
    {
        return;
    }
    if (!in_list(active_line))
    {
        return;
    }
    NewsgroupData *np = find_newsgroup(active_line);
    if (np != nullptr && np->to_read > TR_UNSUB)
    {
        return;
    }
    if (add_matching || np)
    {
        // it's not in a newsrc
        add_to_list(active_line, high-low, 0);
    }
    else
    {
        std::strcat(active_line,"\n");
        print_lines(active_line, NO_MARKING);
    }
}

static int add_group_order_number(const AddGroup **app1, const AddGroup **app2)
{
    const NewsgroupNum eq = (*app1)->num - (*app2)->num;
    return eq.value_of() > 0? g_sel_direction : -g_sel_direction;
}

static int add_group_order_group_name(const AddGroup **app1, const AddGroup **app2)
{
    return string_case_compare((*app1)->name, (*app2)->name) * g_sel_direction;
}

static int add_group_order_count(const AddGroup **app1, const AddGroup **app2)
{
    const ArticleNum eq = (*app1)->to_read - (*app2)->to_read;
    if (eq != ArticleNum{})
    {
        return eq > ArticleNum{} ? g_sel_direction : -g_sel_direction;
    }
    return add_group_order_group_name(app1, app2);
}

// Sort the newsgroups into the chosen order.
void sort_add_groups()
{
    AddGroup* ap;
    AddGroup** lp;
    int (*sort_procedure)(const AddGroup**, const AddGroup**);

    switch (g_sel_sort)
    {
    case SS_NATURAL:
    default:
        sort_procedure = add_group_order_number;
        break;
    case SS_STRING:
        sort_procedure = add_group_order_group_name;
        break;
    case SS_COUNT:
        sort_procedure = add_group_order_count;
        break;
    }

    AddGroup **ag_list = (AddGroup**)safe_malloc(s_add_group_count * sizeof(AddGroup*));
    for (lp = ag_list, ap = g_first_add_group; ap; ap = ap->next)
    {
        *lp++ = ap;
    }
    TRN_ASSERT(lp - ag_list == s_add_group_count);

    std::qsort(ag_list, s_add_group_count, sizeof(AddGroup*), (int(*)(const void *, const void *)) sort_procedure);

    ap = ag_list[0];
    g_first_add_group = ag_list[0];
    ap->prev = nullptr;
    lp = ag_list;
    for (int i = s_add_group_count; --i; ++lp)
    {
        lp[0]->next = lp[1];
        lp[1]->prev = lp[0];
    }
    g_last_add_group = lp[0];
    g_last_add_group->next = nullptr;
    std::free((char*)ag_list);
}
