/* datasrc.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/fdio.h>
#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/datasrc.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/edit_dist.h"
#include "trn/hash.h"
#include "trn/nntp.h"
#include "trn/rcstuff.h"
#include "trn/rt-ov.h"
#include "trn/rt-util.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#ifdef I_UTIME
#include <utime.h>
#endif
#ifdef I_SYS_UTIME
#include <sys/utime.h>
#endif
#if !defined(I_UTIME) && !defined(I_SYS_UTIME)
struct utimbuf
{
    std::time_t actime;
    std::time_t modtime;
};
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

List       *g_data_source_list{};                     // a list of all data sources
DataSource *g_data_source{};                          // the current data source
int         g_data_source_cnt{};                      //
char       *g_trn_access_mem{};                       //
std::string g_nntp_auth_file;                         //
time_t      g_def_refetch_secs{DEFAULT_REFETCH_SECS}; // -z

enum
{
    SRCFILE_CHUNK_SIZE = (32 * 1024),
};

enum DataSourceIniIndex
{
    DI_NNTP_SERVER = 1,
    DI_ACTIVE_FILE,
    DI_ACT_REFETCH,
    DI_SPOOL_DIR,
    DI_OVERVIEW_DIR,
    DI_ACTIVE_TIMES,
    DI_GROUP_DESC,
    DI_DESC_REFETCH,
    DI_AUTH_USER,
    DI_AUTH_PASS,
    DI_AUTH_COMMAND,
    DI_XHDR_BROKEN,
    DI_XREFS,
    DI_OVERVIEW_FMT,
    DI_FORCE_AUTH
};

static IniWords s_datasrc_ini[] =
{
    // clang-format off
    { 0, "DATASRC", nullptr },
    { 0, "NNTP Server", nullptr },
    { 0, "Active File", nullptr },
    { 0, "Active File Refetch", nullptr },
    { 0, "Spool Dir", nullptr },
    { 0, "Thread Dir", nullptr },
    { 0, "Overview Dir", nullptr },
    { 0, "Active Times", nullptr },
    { 0, "Group Desc", nullptr },
    { 0, "Group Desc Refetch", nullptr },
    { 0, "Auth User", nullptr },
    { 0, "Auth Password", nullptr },
    { 0, "Auth Command", nullptr },
    { 0, "XHDR Broken", nullptr },
    { 0, "Xrefs", nullptr },
    { 0, "Overview Format File", nullptr },
    { 0, "Force Auth", nullptr },
    { 0, nullptr, nullptr }
    // clang-format on
};

static char       *dir_or_none(DataSource *dp, const char *dir, DataSourceFlags flag);
static char       *file_or_none(char *fn);
static int         source_file_cmp(const char *key, int key_len, HashDatum data);
static int         check_distance(int len, HashDatum *data, int newsrc_ptr);
static int         get_near_miss();
static DataSource *new_data_source(const char *name, char **vals);

void data_source_init()
{
    char** vals = prep_ini_words(s_datasrc_ini);
    char* actname = nullptr;

    g_data_source_list = new_list(0,0,sizeof(DataSource),20,LF_ZERO_MEM,nullptr);

    g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);

    char *machine = get_val("NNTPSERVER");
    if (machine && std::strcmp(machine,"local") != 0)
    {
        vals[DI_NNTP_SERVER] = machine;
        vals[DI_AUTH_USER] = read_auth_file(g_nntp_auth_file.c_str(),
                                            &vals[DI_AUTH_PASS]);
        vals[DI_FORCE_AUTH] = get_val("NNTP_FORCE_AUTH");
        new_data_source("default",vals);
    }

    g_trn_access_mem = read_data_sources(TRNACCESS);
    char *s = read_data_sources(DEFACCESS);
    if (!g_trn_access_mem)
    {
        g_trn_access_mem = s;
    }
    else if (s)
    {
        std::free(s);
    }

    if (!machine)
    {
        machine = file_exp(SERVER_NAME);
        if (FILE_REF(machine))
        {
            machine = nntp_server_name(machine);
        }
        if (!std::strcmp(machine, "local"))
        {
            machine = nullptr;
            actname = ACTIVE;
        }
        prep_ini_words(s_datasrc_ini);  // re-zero the values

        vals[DI_NNTP_SERVER] = machine;
        vals[DI_ACTIVE_FILE] = actname;
        vals[DI_SPOOL_DIR] = NEWSSPOOL;
        vals[DI_OVERVIEW_DIR] = OVERVIEW_DIR;
        vals[DI_OVERVIEW_FMT] = OVERVIEW_FMT;
        vals[DI_ACTIVE_TIMES] = ACTIVE_TIMES;
        vals[DI_GROUP_DESC] = GROUPDESC;
        if (machine)
        {
            vals[DI_AUTH_USER] = read_auth_file(g_nntp_auth_file.c_str(),
                                                &vals[DI_AUTH_PASS]);
            vals[DI_FORCE_AUTH] = get_val("NNTP_FORCE_AUTH");
        }
        new_data_source("default",vals);
    }
    unprep_ini_words(s_datasrc_ini);
}


void data_source_finalize()
{
    if (g_data_source_list)
    {
        for (DataSource *dp = data_source_first(); dp && !empty(dp->name); dp = data_source_next(dp))
        {
            close_data_source(dp);
        }

        delete_list(g_data_source_list);
        g_data_source_list = nullptr;
    }
    g_data_source_cnt = 0;
    g_nntp_auth_file.clear();
}

char *read_data_sources(const char *filename)
{
    char* section;
    char* cond;
    char* filebuf = nullptr;
    char** vals = ini_values(s_datasrc_ini);

    int fd = open(file_exp(filename), 0);
    if (fd >= 0)
    {
        stat_t datasrc_stat{};
        fstat(fd,&datasrc_stat);
        if (datasrc_stat.st_size)
        {
            filebuf = safe_malloc((MemorySize)datasrc_stat.st_size+2);
            int len = read(fd, filebuf, (int)datasrc_stat.st_size);
            (filebuf)[len] = '\0';
            prep_ini_data(filebuf,filename);
            char *s = filebuf;
            while ((s = next_ini_section(s, &section, &cond)) != nullptr)
            {
                if (*cond && !check_ini_cond(cond))
                {
                    continue;
                }
                if (string_case_equal(section, "group ", 6))
                {
                    continue;
                }
                s = parse_ini_section(s, s_datasrc_ini);
                if (!s)
                {
                    break;
                }
                new_data_source(section,vals);
            }
        }
        close(fd);
    }
    return filebuf;
}

DataSource *get_data_source(const char *name)
{
    for (DataSource *dp = data_source_first(); dp && !empty(dp->name); dp = data_source_next(dp))
    {
        if (dp->name == std::string{name})
        {
            return dp;
        }
    }
    return nullptr;
}

static DataSource *new_data_source(const char *name, char **vals)
{
    DataSource* dp = data_source_ptr(g_data_source_cnt++);

    if (vals[DI_NNTP_SERVER])
    {
        dp->flags |= DF_REMOTE;
    }
    else if (!vals[DI_ACTIVE_FILE])
    {
        return nullptr;
    }

    dp->name = save_str(name);
    if (!std::strcmp(name,"default"))
    {
        dp->flags |= DF_DEFAULT;
    }

    const char *v = vals[DI_NNTP_SERVER];
    if (v != nullptr)
    {
        dp->news_id = save_str(v);
        char *cp = std::strchr(dp->news_id, ';');
        if (cp != nullptr)
        {
            *cp = '\0';
            dp->nntp_link.port_number = std::atoi(cp+1);
        }

        v = vals[DI_ACT_REFETCH];
        if (v != nullptr && *v)
        {
            dp->act_sf.refetch_secs = text_to_secs(v, g_def_refetch_secs);
        }
        else if (!vals[DI_ACTIVE_FILE])
        {
            dp->act_sf.refetch_secs = g_def_refetch_secs;
        }
    }
    else
    {
        dp->news_id = save_str(file_exp(vals[DI_ACTIVE_FILE]));
    }

    dp->spool_dir = file_or_none(vals[DI_SPOOL_DIR]);
    if (!dp->spool_dir)
    {
        dp->spool_dir = save_str(g_tmp_dir.c_str());
    }

    dp->over_dir = dir_or_none(dp,vals[DI_OVERVIEW_DIR],DF_TRY_OVERVIEW);
    dp->over_fmt = file_or_none(vals[DI_OVERVIEW_FMT]);
    dp->group_desc = dir_or_none(dp,vals[DI_GROUP_DESC],DF_NONE);
    dp->extra_name = dir_or_none(dp,vals[DI_ACTIVE_TIMES],DF_ADD_OK);
    if (dp->flags & DF_REMOTE)
    {
        // FYI, we know extra_name to be nullptr in this case.
        if (vals[DI_ACTIVE_FILE])
        {
            dp->extra_name = save_str(file_exp(vals[DI_ACTIVE_FILE]));
            stat_t extra_stat{};
            if (stat(dp->extra_name,&extra_stat) >= 0)
            {
                dp->act_sf.last_fetch = extra_stat.st_mtime;
            }
        }
        else
        {
            dp->extra_name = temp_filename();
            dp->flags |= DF_TMP_ACTIVE_FILE;
            if (!dp->act_sf.refetch_secs)
            {
                dp->act_sf.refetch_secs = 1;
            }
        }

        v = vals[DI_DESC_REFETCH];
        if (v != nullptr && *v)
        {
            dp->desc_sf.refetch_secs = text_to_secs(v, g_def_refetch_secs);
        }
        else if (!dp->group_desc)
        {
            dp->desc_sf.refetch_secs = g_def_refetch_secs;
        }
        if (dp->group_desc)
        {
            stat_t desc_stat{};
            if (stat(dp->group_desc,&desc_stat) >= 0)
            {
                dp->desc_sf.last_fetch = desc_stat.st_mtime;
            }
        }
        else
        {
            dp->group_desc = temp_filename();
            dp->flags |= DF_TMP_GROUP_DESC;
            if (!dp->desc_sf.refetch_secs)
            {
                dp->desc_sf.refetch_secs = 1;
            }
        }
    }
    v = vals[DI_FORCE_AUTH];
    if (v != nullptr && (*v == 'y' || *v == 'Y'))
    {
        dp->nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
    }
    v = vals[DI_AUTH_USER];
    if (v != nullptr)
    {
        dp->auth_user = save_str(v);
    }
    v = vals[DI_AUTH_PASS];
    if (v != nullptr)
    {
        dp->auth_pass = save_str(v);
    }
    v = vals[DI_XHDR_BROKEN];
    if (v != nullptr && (*v == 'y' || *v == 'Y'))
    {
        dp->flags |= DF_XHDR_BROKEN;
    }
    v = vals[DI_XREFS];
    if (v != nullptr && (*v == 'n' || *v == 'N'))
    {
        dp->flags |= DF_NO_XREFS;
    }

    return dp;
}

static char *dir_or_none(DataSource *dp, const char *dir, DataSourceFlags flag)
{
    if (!dir || !*dir || !std::strcmp(dir, "remote"))
    {
        dp->flags |= flag;
        if (dp->flags & DF_REMOTE)
        {
            return nullptr;
        }
        if (flag == DF_ADD_OK)
        {
            char* cp = safe_malloc(std::strlen(dp->news_id)+6+1);
            std::sprintf(cp,"%s.times",dp->news_id);
            return cp;
        }
        if (flag == DF_NONE)
        {
            char* cp = std::strrchr(dp->news_id,'/');
            if (!cp)
            {
                return nullptr;
            }
            int len = cp - dp->news_id + 1;
            cp = safe_malloc(len+10+1);
            std::strcpy(cp,dp->news_id);
            std::strcpy(cp+len,"newsgroups");
            return cp;
        }
        return dp->spool_dir;
    }

    if (!std::strcmp(dir,"none"))
    {
        return nullptr;
    }

    dp->flags |= flag;
    dir = file_exp(dir);
    if (!std::strcmp(dir,dp->spool_dir))
    {
        return dp->spool_dir;
    }
    return save_str(dir);
}

static char *file_or_none(char *fn)
{
    if (!fn || !*fn || !std::strcmp(fn,"none") || !std::strcmp(fn,"remote"))
    {
        return nullptr;
    }
    return save_str(file_exp(fn));
}

bool open_data_source(DataSource *dp)
{
    bool success;

    if (dp->flags & DF_UNAVAILABLE)
    {
        return false;
    }
    set_data_source(dp);
    if (dp->flags & DF_OPEN)
    {
        return true;
    }
    if (dp->flags & DF_REMOTE)
    {
        if (nntp_connect(dp->news_id, true) <= 0)
        {
            dp->flags |= DF_UNAVAILABLE;
            return false;
        }
        g_nntp_allow_timeout = false;
        dp->nntp_link = g_nntp_link;
        if (dp->act_sf.refetch_secs)
        {
            switch (nntp_list("active", "control", 7))
            {
            case 1:
                if (std::strncmp(g_ser_line, "control ", 8) != 0)
                {
                    std::strcpy(g_buf, g_ser_line);
                    dp->act_sf.last_fetch = 0;
                    success = active_file_hash(dp);
                    break;
                }
                if (nntp_gets(g_buf, sizeof g_buf - 1) == NGSR_FULL_LINE //
                    && !nntp_at_list_end(g_buf))
                {
                    nntp_finish_list();
                    success = active_file_hash(dp);
                    break;
                }
                // FALL THROUGH

            case 0:
                dp->flags |= DF_USE_LIST_ACTIVE;
                if (dp->flags & DF_TMP_ACTIVE_FILE)
                {
                    dp->flags &= ~DF_TMP_ACTIVE_FILE;
                    std::free(dp->extra_name);
                    dp->extra_name = nullptr;
                    dp->act_sf.refetch_secs = 0;
                    success = source_file_open(&dp->act_sf,nullptr,
                                           nullptr,nullptr);
                }
                else
                {
                    success = active_file_hash(dp);
                }
                break;

            case -2:
                std::printf("Failed to open news server %s:\n%s\n", dp->news_id, g_ser_line);
                term_down(2);
                success = false;
                break;

            default:
                success = active_file_hash(dp);
                break;
            }
        }
        else
        {
            success = active_file_hash(dp);
        }
    }
    else
    {
        success = active_file_hash(dp);
    }
    if (success)
    {
        dp->flags |= DF_OPEN;
        if (dp->flags & DF_TRY_OVERVIEW)
        {
            ov_init();
        }
    }
    else
    {
        dp->flags |= DF_UNAVAILABLE;
    }
    if (dp->flags & DF_REMOTE)
    {
        g_nntp_allow_timeout = true;
    }
    return success;
}

void set_data_source(DataSource *dp)
{
    if (g_data_source)
    {
        g_data_source->nntp_link = g_nntp_link;
    }
    if (dp)
    {
        g_nntp_link = dp->nntp_link;
    }
    g_data_source = dp;
}

void check_data_sources()
{
    std::time_t now = std::time(nullptr);

    if (g_data_source_list)
    {
        for (DataSource *dp = data_source_first(); dp && !empty(dp->name); dp = data_source_next(dp))
        {
            if ((dp->flags & DF_OPEN) && dp->nntp_link.connection)
            {
                std::time_t limit = ((dp->flags & DF_ACTIVE) ? 30 * 60 : 10 * 60);
                if (now - dp->nntp_link.last_command > limit)
                {
                    DataSource* save_datasrc = g_data_source;
                    set_data_source(dp);
                    nntp_close(true);
                    dp->nntp_link = g_nntp_link;
                    set_data_source(save_datasrc);
                }
            }
        }
    }
}

void close_data_source(DataSource *dp)
{
    if (dp->flags & DF_REMOTE)
    {
        if (dp->flags & DF_TMP_ACTIVE_FILE)
        {
            remove(dp->extra_name);
        }
        else
        {
            source_file_end_append(&dp->act_sf, dp->extra_name);
        }
        if (dp->group_desc)
        {
            if (dp->flags & DF_TMP_GROUP_DESC)
            {
                remove(dp->group_desc);
            }
            else
            {
                source_file_end_append(&dp->desc_sf, dp->group_desc);
            }
        }
    }

    if (!(dp->flags & DF_OPEN))
    {
        return;
    }

    if (dp->flags & DF_REMOTE)
    {
        DataSource* save_datasrc = g_data_source;
        set_data_source(dp);
        nntp_close(true);
        dp->nntp_link = g_nntp_link;
        set_data_source(save_datasrc);
    }
    source_file_close(&dp->act_sf);
    source_file_close(&dp->desc_sf);
    dp->flags &= ~DF_OPEN;
    if (g_data_source == dp)
    {
        g_data_source = nullptr;
    }
}

bool active_file_hash(DataSource *dp)
{
    int ret;
    if (dp->flags & DF_REMOTE)
    {
        DataSource* save_datasrc = g_data_source;
        set_data_source(dp);
        g_spin_todo = dp->act_sf.recent_cnt;
        ret = source_file_open(&dp->act_sf, dp->extra_name, "active", dp->news_id);
        if (g_spin_count > 0)
        {
            dp->act_sf.recent_cnt = g_spin_count;
        }
        set_data_source(save_datasrc);
    }
    else
    {
        ret = source_file_open(&dp->act_sf, dp->news_id, nullptr, nullptr);
    }
    return ret != 0;
}

bool find_active_group(DataSource *dp, char *outbuf, const char *nam, int len, ArticleNum high)
{
    ActivePosition act_pos;
    std::FILE* fp = dp->act_sf.fp;
    char* lbp;
    int lbp_len;

    // Do a quick, hashed lookup

    outbuf[0] = '\0';
    HashDatum data = hash_fetch(dp->act_sf.hp, nam, len);
    if (data.dat_ptr)
    {
        ListNode* node = (ListNode*)data.dat_ptr;
        // dp->act_sf.lp->recent = node;
        act_pos = ActivePosition{node->low + data.dat_len};
        lbp = node->data + data.dat_len;
        lbp_len = std::strchr(lbp, '\n') - lbp + 1;
    }
    else
    {
        lbp = nullptr;
        lbp_len = 0;
    }
    if ((dp->flags & DF_USE_LIST_ACTIVE) //
        && (data_source_nntp_flags(dp) & NNTP_NEW_CMD_OK))
    {
        DataSource* save_datasrc = g_data_source;
        set_data_source(dp);
        switch (nntp_list("active", nam, len))
        {
        case 0:
            set_data_source(save_datasrc);
            return false;

        case 1:
            std::sprintf(outbuf, "%s\n", g_ser_line);
            nntp_finish_list();
            break;

        case -2:
            // TODO
            break;
        }
        set_data_source(save_datasrc);
        if (!lbp_len)
        {
            if (fp)
            {
                (void) source_file_append(&dp->act_sf, outbuf, len);
            }
            return true;
        }
# ifndef ANCIENT_NEWS
        // Safely update the low-water mark
        {
            char* f = std::strrchr(outbuf, ' ');
            char* t = lbp + lbp_len;
            while (*--t != ' ')
            {
            }
            while (t > lbp)
            {
                if (*--t == ' ')
                {
                    break;
                }
                if (f[-1] == ' ')
                {
                    *t = '0';
                }
                else
                {
                    *t = *--f;
                }
            }
        }
# endif
        high = ArticleNum{std::atol(outbuf+len+1)};
    }

    if (lbp_len)
    {
        if ((dp->flags & DF_REMOTE) && dp->act_sf.refetch_secs)
        {
            char* cp;
            if (high && high != ArticleNum{std::atol(cp = lbp + len + 1)})
            {
                cp = skip_digits(cp);
                while (*--cp != ' ')
                {
                    long num = value_of(high) % 10;
                    high /= ArticleNum{10};
                    *cp = '0' + (char)num;
                }
                std::fseek(fp, act_pos.value_of(), 0);
                std::fwrite(lbp, 1, lbp_len, fp);
            }
            goto use_cache;
        }

        // hopefully this forces a reread
        std::fseek(fp,2000000000L,1);

        // if line has changed length or is not there, we should
        // discard/close the active file, and re-open it.
        if (std::fseek(fp, act_pos.value_of(), 0) >= 0         //
            && std::fgets(outbuf, LINE_BUF_LEN, fp) != nullptr //
            && !std::strncmp(outbuf, nam, len) && outbuf[len] == ' ')
        {
            // Remember the latest info in our cache.
            (void) std::memcpy(lbp,outbuf,lbp_len);
            return true;
        }
use_cache:
        // Return our cached version
        (void) std::memcpy(outbuf,lbp,lbp_len);
        outbuf[lbp_len] = '\0';
        return true;
    }
    return false;       // no such group
}

const char *find_group_desc(DataSource *dp, const char *group_name)
{
    int grouplen;

    if (!dp->group_desc)
    {
        return "";
    }

    if (!dp->desc_sf.hp)
    {
        int ret;
        if ((dp->flags & DF_REMOTE) && dp->desc_sf.refetch_secs)
        {
            set_data_source(dp);
            if ((dp->flags & (DF_TMP_GROUP_DESC | DF_NO_XGTITLE)) == DF_TMP_GROUP_DESC //
                && g_net_speed < 5)
            {
                (void)source_file_open(&dp->desc_sf,nullptr, // TODO: check return?
                                   nullptr,nullptr);
                grouplen = std::strlen(group_name);
                goto try_xgtitle;
            }
            g_spin_todo = dp->desc_sf.recent_cnt;
            ret = source_file_open(&dp->desc_sf, dp->group_desc,
                               "newsgroups", dp->news_id);
            if (g_spin_count > 0)
            {
                dp->desc_sf.recent_cnt = g_spin_count;
            }
        }
        else
        {
            ret = source_file_open(&dp->desc_sf, dp->group_desc, nullptr, nullptr);
        }
        if (!ret)
        {
            if (dp->flags & DF_TMP_GROUP_DESC)
            {
                dp->flags &= ~DF_TMP_GROUP_DESC;
                remove(dp->group_desc);
            }
            std::free(dp->group_desc);
            dp->group_desc = nullptr;
            return "";
        }
        if (ret == 2 || !dp->desc_sf.refetch_secs)
        {
            dp->flags |= DF_NO_XGTITLE;
        }
    }

    grouplen = std::strlen(group_name);
    if (HashDatum data = hash_fetch(dp->desc_sf.hp, group_name, grouplen); data.dat_ptr)
    {
        ListNode*node = (ListNode*)data.dat_ptr;
        // dp->act_sf.lp->recent = node;
        return node->data + data.dat_len + grouplen + 1;
    }

try_xgtitle:
    if ((dp->flags & (DF_REMOTE | DF_NO_XGTITLE)) == DF_REMOTE)
    {
        set_data_source(dp);
        if (nntp_xgtitle(group_name) > 0)
        {
            nntp_gets(g_buf, sizeof g_buf - 1); // TODO: check return value?
            if (nntp_at_list_end(g_buf))
            {
                std::sprintf(g_buf, "%s \n", group_name);
            }
            else
            {
                nntp_finish_list();
                std::strcat(g_buf, "\n");
            }
            group_name = source_file_append(&dp->desc_sf, g_buf, grouplen);
            return group_name+grouplen+1;
        }
        dp->flags |= DF_NO_XGTITLE;
        if (dp->desc_sf.lp->high == -1)
        {
            source_file_close(&dp->desc_sf);
            if (dp->flags & DF_TMP_GROUP_DESC)
            {
                return find_group_desc(dp, group_name);
            }
            std::free(dp->group_desc);
            dp->group_desc = nullptr;
        }
    }
    return "";
}

// NOTE: This was factored from srcfile_open and srcfile_append and is
// basically same as dectrl() except the s++, *s != '\n' and return s.
// Because we need to keep track of s we can't really reuse dectrl()
// from cache.c; if we want to factor further we need a new function.
//
static char *adv_then_find_next_nl_and_dectrl(char *s)
{
    if (s == nullptr)
    {
        return s;
    }

    for (s++; *s && *s != '\n';)
    {
        int w = byte_length_at(s);
        if (at_grey_space(s))
        {
            for (int i = 0; i < w; i += 1)
            {
                s[i] = ' ';
            }
        }
        s += w;
    }
    return s;
}

int source_file_open(SourceFile *sfp, const char *filename, const char *fetch_cmd, const char *server)
{
    unsigned offset;
    char* s;
    HashDatum data;
    long node_low;
    int linelen;
    std::FILE* fp;
    std::time_t now = std::time(nullptr);
    bool use_buffered_nntp_gets = false;

    if (!filename)
    {
        fp = nullptr;
    }
    else if (server)
    {
        if (!sfp->refetch_secs)
        {
            server = nullptr;
            fp = std::fopen(filename, "r");
            g_spin_todo = 0;
        }
        else if (now - sfp->last_fetch > sfp->refetch_secs && (sfp->refetch_secs != 2 || !sfp->last_fetch))
        {
            fp = std::fopen(filename, "w+");
            if (fp)
            {
                std::printf("Getting %s file from %s.", fetch_cmd, server);
                std::fflush(stdout);
                // tell server we want the file
                if (!(g_nntp_link.flags & NNTP_NEW_CMD_OK))
                {
                    use_buffered_nntp_gets = true;
                }
                else if (nntp_list(fetch_cmd, "", 0) < 0)
                {
                    std::printf("\nCan't get %s file from server: \n%s\n",
                           fetch_cmd, g_ser_line);
                    term_down(2);
                    std::fclose(fp);
                    return 0;
                }
                sfp->last_fetch = now;
                if (g_net_speed > 8)
                {
                    g_spin_todo = 0;
                }
            }
        }
        else
        {
            server = nullptr;
            fp = std::fopen(filename, "r+");
            if (!fp)
            {
                sfp->refetch_secs = 0;
                fp = std::fopen(filename, "r");
            }
            g_spin_todo = 0;
        }
        if (sfp->refetch_secs & 3)
        {
            sfp->refetch_secs += 365L * 24 * 60 * 60;
        }
    }
    else
    {
        fp = std::fopen(filename, "r");
        g_spin_todo = 0;
    }

    if (filename && fp == nullptr)
    {
        std::printf(g_cant_open, filename);
        term_down(1);
        return 0;
    }
    set_spin(g_spin_todo > 0? SPIN_BAR_GRAPH : SPIN_FOREGROUND);

    source_file_close(sfp);

    // Create a list with one character per item using a large chunk size.
    sfp->lp = new_list(0, 0, 1, SRCFILE_CHUNK_SIZE, LF_NONE, nullptr);
    sfp->hp = hash_create(3001, source_file_cmp);
    sfp->fp = fp;

    if (!filename)
    {
        (void) list_get_item(sfp->lp, 0);
        sfp->lp->high = -1;
        set_spin(SPIN_OFF);
        return 1;
    }

    char *lbp = list_get_item(sfp->lp, 0);
    data.dat_ptr = (char*)sfp->lp->first;

    for (offset = 0, node_low = 0;; offset += linelen, lbp += linelen)
    {
        if (server)
        {
            if (use_buffered_nntp_gets)
            {
                use_buffered_nntp_gets = false;
            }
            else if (nntp_gets(g_buf, sizeof g_buf - 1) == NGSR_ERROR)
            {
                std::printf("\nError getting %s file.\n", fetch_cmd);
                term_down(2);
                source_file_close(sfp);
                set_spin(SPIN_OFF);
                return 0;
            }
            if (nntp_at_list_end(g_buf))
            {
                break;
            }
            std::strcat(g_buf,"\n");
            std::fputs(g_buf, fp);
            spin(200 * g_net_speed);
        }
        else if (!std::fgets(g_buf, sizeof g_buf, fp))
        {
            break;
        }

        s = skip_non_space(g_buf);
        if (!*s)
        {
            linelen = 0;
            continue;
        }
        int keylen = s - g_buf;
        if (*++s != '\n' && std::isspace(*s))
        {
            while (*++s != '\n' && std::isspace(*s))
            {
            }
            std::strcpy(g_buf+keylen+1, s);
            s = g_buf+keylen+1;
        }
        s = adv_then_find_next_nl_and_dectrl(s);
        linelen = s - g_buf + 1;
        if (*s != '\n')
        {
            if (linelen == sizeof g_buf)
            {
                linelen = 0;
                continue;
            }
            *s++ = '\n';
            *s = '\0';
        }
        if (offset + linelen > SRCFILE_CHUNK_SIZE)
        {
            ListNode* node = sfp->lp->recent;
            node_low += offset;
            node->high = node_low - 1;
            node->data_high = node->data + offset - 1;
            offset = 0;
            lbp = list_get_item(sfp->lp, node_low);
            data.dat_ptr = (char*)sfp->lp->recent;
        }
        data.dat_len = offset;
        (void) std::memcpy(lbp,g_buf,linelen);
        hash_store(sfp->hp, g_buf, keylen, data);
    }
    sfp->lp->high = node_low + offset - 1;
    set_spin(SPIN_OFF);

    if (server)
    {
        std::fflush(fp);
        if (std::ferror(fp))
        {
            std::printf("\nError writing the %s file %s.\n",fetch_cmd,filename);
            term_down(2);
            source_file_close(sfp);
            return 0;
        }
        newline();
    }
    std::fseek(fp,0L,0);

    return server? 2 : 1;
}

char *source_file_append(SourceFile *sfp, char *bp, int key_len)
{
    HashDatum data;

    long pos = sfp->lp->high + 1;
    char *lbp = list_get_item(sfp->lp, pos);
    ListNode *node = sfp->lp->recent;
    data.dat_len = pos - node->low;

    char *s = bp + key_len + 1;
    if (sfp->fp && sfp->refetch_secs && *s != '\n')
    {
        std::fseek(sfp->fp, 0, 2);
        std::fputs(bp, sfp->fp);
    }

    if (*s != '\n' && std::isspace(*s))
    {
        while (*++s != '\n' && std::isspace(*s))
        {
        }
        std::strcpy(bp+key_len+1, s);
        s = bp+key_len+1;
    }
    s = adv_then_find_next_nl_and_dectrl(s);
    const int linelen = s - bp + 1;
    if (*s != '\n')
    {
        *s++ = '\n';
        *s = '\0';
    }
    if (data.dat_len + linelen > SRCFILE_CHUNK_SIZE)
    {
        node->high = pos - 1;
        node->data_high = node->data + data.dat_len - 1;
        lbp = list_get_item(sfp->lp, pos);
        node = sfp->lp->recent;
        data.dat_len = 0;
    }
    data.dat_ptr = (char*)node;
    (void) std::memcpy(lbp,bp,linelen);
    hash_store(sfp->hp, bp, key_len, data);
    sfp->lp->high = pos + linelen - 1;

    return lbp;
}

void source_file_end_append(SourceFile *sfp, const char *filename)
{
    if (sfp->fp && sfp->refetch_secs)
    {
        std::fflush(sfp->fp);

        if (sfp->last_fetch)
        {
            struct utimbuf ut;
            std::time(&ut.actime);
            ut.modtime = sfp->last_fetch;
            (void) utime(filename, &ut);
        }
    }
}

void source_file_close(SourceFile *sfp)
{
    if (sfp->fp)
    {
        std::fclose(sfp->fp);
        sfp->fp = nullptr;
    }
    if (sfp->lp)
    {
        delete_list(sfp->lp);
        sfp->lp = nullptr;
    }
    if (sfp->hp)
    {
        hash_destroy(sfp->hp);
        sfp->hp = nullptr;
    }
}

static int source_file_cmp(const char *key, int key_len, HashDatum data)
{
    // We already know that the lengths are equal, just compare the strings
    return std::memcmp(key, ((ListNode*)data.dat_ptr)->data + data.dat_len, key_len);
}

// Edit Distance extension to trn
//
//      Mark Maimone (mwm@cmu.edu)
//      Carnegie Mellon Computer Science
//      9 May 1993
//
//      This code helps trn handle typos in newsgroup names much more
//   gracefully.  Instead of "... does not exist!!", it will pick the
//   nearest one, or offer you a choice if there are several options.
//

// find_close_match -- Finds the closest match for the string given in
// global g_newsgroup_name.  If found, the result will be the corrected string
// returned in that global.
//
// We compare the (presumably misspelled) newsgroup name with all legal
// newsgroups, using the Edit Distance metric.  The edit distance between
// two strings is the minimum number of simple operations required to
// convert one string to another (the implementation here supports INSERT,
// DELETE, CHANGE and SWAP).  This gives every legal newsgroup an integer
// rank.
//
// You might want to present all of the closest matches, and let the user
// choose among them.  But because I'm lazy I chose to only keep track of
// all with newsgroups with the//single* smallest error, in array s_newsgroup_ptrs[].
// A more flexible approach would keep around the 10 best matches, whether
// or not they had precisely the same edit distance, but oh well.
//

static char **s_newsgroup_ptrs{}; // List of potential matches
static int    s_newsgroup_num{};  // Length of list in s_newsgroup_ptrs[]
static int    s_best_match{};     // Value of best match

int find_close_match()
{
    int ret = 0;

    s_best_match = -1;
    s_newsgroup_ptrs = (char**)safe_malloc(MAX_NG * sizeof (char*));
    s_newsgroup_num = 0;

    // Iterate over all legal newsgroups
    for (DataSource *dp = data_source_first(); dp && !empty(dp->name); dp = data_source_next(dp))
    {
        if (dp->flags & DF_OPEN)
        {
            if (dp->act_sf.hp)
            {
                hash_walk(dp->act_sf.hp, check_distance, 0);
            }
            else
            {
                ret = -1;
            }
        }
    }

    if (ret < 0)
    {
        hash_walk(g_newsrc_hash, check_distance, 1);
        ret = 0;
    }

    // s_ngn is the number of possibilities.  If there's just one, go with it.

    switch (s_newsgroup_num)
    {
    case 0:
        break;
    case 1:
    {
        char* cp = std::strchr(s_newsgroup_ptrs[0], ' ');
        if (cp)
        {
            *cp = '\0';
        }
        if (g_verbose)
        {
            std::printf("(I assume you meant %s)\n", s_newsgroup_ptrs[0]);
        }
        else
        {
            std::printf("(Using %s)\n", s_newsgroup_ptrs[0]);
        }
        set_newsgroup_name(s_newsgroup_ptrs[0]);
        if (cp)
        {
            *cp = ' ';
        }
        ret = 1;
        break;
    }

    default:
        ret = get_near_miss();
        break;
    }
    std::free((char*)s_newsgroup_ptrs);
    return ret;
}

static int check_distance(int len, HashDatum *data, int newsrc_ptr)
{
    char* name;

    if (newsrc_ptr)
    {
        name = ((NewsgroupData *) data->dat_ptr)->rc_line;
    }
    else
    {
        name = ((ListNode *) data->dat_ptr)->data + data->dat_len;
    }

    // Efficiency: don't call edit_dist when the lengths are too different.
    const int ngname_len = static_cast<int>(g_newsgroup_name.length());
    if (len < ngname_len)
    {
        if (ngname_len - len > LENGTH_HACK)
        {
            return 0;
        }
    }
    else
    {
        if (len - ngname_len > LENGTH_HACK)
        {
            return 0;
        }
    }

    int value = edit_distn(g_newsgroup_name.c_str(), ngname_len, name, len);
    if (value > MIN_DIST)
    {
        return 0;
    }

    if (value < s_best_match)
    {
        s_newsgroup_num = 0;
    }
    if (s_best_match < 0 || value <= s_best_match)
    {
        for (int i = 0; i < s_newsgroup_num; i++)
        {
            if (!std::strcmp(name,s_newsgroup_ptrs[i]))
            {
                return 0;
            }
        }
        s_best_match = value;
        if (s_newsgroup_num < MAX_NG)
        {
            s_newsgroup_ptrs[s_newsgroup_num++] = name;
        }
    }
    return 0;
}

// Now we've got several potential matches, and have to choose between them
// somehow.  Again, results will be returned in global g_newsgroup_name.
//
static int get_near_miss()
{
    char promptbuf[256];
    char options[MAX_NG+10];
    char* op = options;

    if (g_verbose)
    {
        std::printf("However, here are some close matches:\n");
    }
    s_newsgroup_num = std::min(s_newsgroup_num, 9);         // Since we're using single digits....
    for (int i = 0; i < s_newsgroup_num; i++)
    {
        char* cp = std::strchr(s_newsgroup_ptrs[i], ' ');
        if (cp)
        {
            *cp = '\0';
        }
        std::printf("  %d.  %s\n", i+1, s_newsgroup_ptrs[i]);
        std::sprintf(op++, "%d", i+1);       // Expensive, but avoids ASCII deps
        if (cp)
        {
            *cp = ' ';
        }
    }
    *op++ = 'n';
    *op = '\0';

    if (g_verbose)
    {
        std::sprintf(promptbuf, "Which of these would you like?");
    }
    else
    {
        std::sprintf(promptbuf, "Which?");
    }
reask:
    in_char(promptbuf, MM_ADD_NEWSGROUP_PROMPT, options);
    print_cmd();
    std::putchar('\n');
    switch (*g_buf)
    {
    case 'n':
    case 'N':
    case 'q':
    case 'Q':
    case 'x':
    case 'X':
        return 0;

    case 'h':
    case 'H':
        if (g_verbose)
        {
            std::fputs("  You entered an illegal newsgroup name, and these are the nearest possible\n"
                    "  matches.  If you want one of these, then enter its number.  Otherwise\n"
                    "  just say 'n'.\n",
                    stdout);
        }
        else
        {
            std::fputs("Illegal newsgroup, enter a number or 'n'.\n", stdout);
        }
        goto reask;

    default:
        if (std::isdigit(*g_buf))
        {
            char* s = std::strchr(options, *g_buf);

            int i = s ? (s - options) : s_newsgroup_num;
            if (i >= 0 && i < s_newsgroup_num)
            {
                char* cp = std::strchr(s_newsgroup_ptrs[i], ' ');
                if (cp)
                {
                    *cp = '\0';
                }
                set_newsgroup_name(s_newsgroup_ptrs[i]);
                if (cp)
                {
                    *cp = ' ';
                }
                return 1;
            }
        }
        std::fputs(g_h_for_help, stdout);
        break;
    }

    settle_down();
    goto reask;
}
