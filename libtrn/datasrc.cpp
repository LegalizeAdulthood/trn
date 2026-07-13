/* datasrc.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/datasrc.h>

#include <config/common.h>
#include <config/fdio.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <trn/DataSourceConfig.h>
#include <trn/edit_dist.h>
#include <trn/hash.h>
#include <trn/IniDocument.h>
#include <trn/IniSectionValues.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/rcstuff.h>
#include <trn/rt-ov.h>
#include <trn/rt-util.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

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
#include <string_view>
#include <vector>

std::vector<DataSource> g_data_sources;                           // all data sources
DataSource             *g_data_source{};                          // the current data source
char                   *g_trn_access_mem{};                       //
std::string             g_nntp_auth_file;                         //
time_t                  g_def_refetch_secs{DEFAULT_REFETCH_SECS}; // -z

static std::optional<std::string> dir_or_none(DataSource *dp, const char *dir, DataSourceFlags flag);
static std::optional<std::string> file_or_none(const char *fn);
static const char                *opt_c_str(const std::optional<std::string> &text);
static HashDatum                  source_file_hash_datum(SourceFile *source_file, std::size_t index);
static SourceFile                *source_file_from_hash(HashDatum data);
static std::string               *source_file_line(HashDatum data);
static long                       source_file_position(HashDatum data);
static int                        source_file_cmp(std::string_view key, HashDatum data);
static int                        check_distance(int len, HashDatum *data, int newsrc_ptr);
static int                        get_near_miss();
static DataSource                *new_data_source(const char *name, const DataSourceConfig &config);
static char                      *read_data_sources(const char *filename);

/// @brief Initializes the data sources for the application.
///
/// This function sets up the global data source list, reads configuration
/// values, and creates default data sources based on the environment and
/// configuration files. It ensures that the data sources are prepared for
/// use by other parts of the application.
///
/// Global variables initialized:
/// - `g_data_sources`: The global list of data sources.
/// - `g_nntp_auth_file`: The NNTP authentication file path.
/// - `g_trn_access_mem`: Memory for TRN access configuration.
///
void data_source_init()
{
    char *actname = nullptr;

    g_data_sources.clear();
    g_data_sources.reserve(20);

    g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);

    char       *machine = get_val("NNTPSERVER");
    std::string expanded_machine;
    if (machine && std::strcmp(machine, "local") != 0)
    {
        DataSourceConfig config;
        char            *auth_pass = nullptr;
        config.set_nntp_server(machine);
        config.set_auth_user(read_auth_file(g_nntp_auth_file.c_str(), &auth_pass));
        config.set_auth_password(auth_pass);
        config.set_force_auth(get_val("NNTP_FORCE_AUTH"));
        new_data_source("default", config);
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
        expanded_machine = file_exp(SERVER_NAME);
        if (!expanded_machine.empty())
        {
            machine = expanded_machine.data();
            if (FILE_REF(machine))
            {
                machine = nntp_server_name(machine);
            }
        }
        if (machine && !std::strcmp(machine, "local"))
        {
            machine = nullptr;
            actname = ACTIVE;
        }
        DataSourceConfig config;
        config.set_nntp_server(machine);
        config.set_active_file(actname);
        config.set_spool_dir(NEWS_SPOOL);
        config.set_overview_dir(OVERVIEW_DIR);
        config.set_overview_format_file(OVERVIEW_FMT);
        config.set_active_times(ACTIVE_TIMES);
        config.set_group_desc(GROUP_DESC);
        if (machine)
        {
            char *auth_pass = nullptr;
            config.set_auth_user(read_auth_file(g_nntp_auth_file.c_str(), &auth_pass));
            config.set_auth_password(auth_pass);
            config.set_force_auth(get_val("NNTP_FORCE_AUTH"));
        }
        new_data_source("default", config);
    }
}

void data_source_finalize()
{
    if (!g_data_sources.empty())
    {
        for (DataSource &data_source : g_data_sources)
        {
            data_source.close();
        }

        g_data_sources.clear();
    }
    g_data_source = nullptr;
    g_nntp_auth_file.clear();
}

/// @brief Reads data sources from the specified file.
///
/// This function reads and parses data sources from an INI-style file. It
/// processes each section and conditionally creates new data sources based
/// on the parsed values.
///
/// @param filename The name of the file to read data sources from.
/// @return A pointer to the allocated file buffer, or nullptr if the file
///         could not be opened or read.
///
static char *read_data_sources(const char *filename)
{
    IniSectionValues values;
    IniDocument      document = IniDocument::read_file(file_exp(filename).c_str(), filename);

    if (document.data() == nullptr)
    {
        return nullptr;
    }

    IniDocument::Section section;
    while (document.next_section(section))
    {
        if (section.has_condition() && !check_ini_cond(section.condition))
        {
            continue;
        }
        if (string_case_equal(section.name, "group ", 6))
        {
            continue;
        }
        if (parse_ini_section(section.body, DataSourceConfig::schema(), values) == nullptr)
        {
            break;
        }
        new_data_source(section.name, DataSourceConfig::from(values));
    }
    return document.release_buffer();
}

DataSource *get_data_source(std::string_view name)
{
    for (DataSource *dp = data_source_first(); dp; dp = data_source_next(dp))
    {
        if (dp->m_name == name)
        {
            return dp;
        }
    }
    return nullptr;
}

static DataSource *new_data_source(const char *name, const DataSourceConfig &config)
{
    if (config.nntp_server() == nullptr && config.active_file() == nullptr)
    {
        return nullptr;
    }

    DataSource *dp = &g_data_sources.emplace_back();

    if (config.nntp_server())
    {
        dp->m_flags |= DF_REMOTE;
    }

    dp->m_name = name;
    if (!std::strcmp(name, "default"))
    {
        dp->m_flags |= DF_DEFAULT;
    }

    const char *v = config.nntp_server();
    if (v != nullptr)
    {
        dp->m_news_id = v;
        char *cp = std::strchr(dp->m_news_id.data(), ';');
        if (cp != nullptr)
        {
            *cp = '\0';
            dp->m_nntp_link.port_number = std::atoi(cp + 1);
        }

        v = config.active_file_refetch();
        if (v != nullptr && *v)
        {
            dp->m_act_sf.m_refetch_secs = text_to_secs(v, g_def_refetch_secs);
        }
        else if (!config.active_file())
        {
            dp->m_act_sf.m_refetch_secs = g_def_refetch_secs;
        }
    }
    else
    {
        dp->m_news_id = file_exp(config.active_file());
    }

    if (std::optional<std::string> spool_dir = file_or_none(config.spool_dir()))
    {
        dp->m_spool_dir = *spool_dir;
    }
    else
    {
        dp->m_spool_dir = g_tmp_dir;
    }

    dp->m_over_dir = dir_or_none(dp, config.overview_dir(), DF_TRY_OVERVIEW);
    dp->m_over_fmt = file_or_none(config.overview_format_file());
    dp->m_group_desc = dir_or_none(dp, config.group_desc(), DF_NONE);
    dp->m_extra_name = dir_or_none(dp, config.active_times(), DF_ADD_OK);
    if (dp->m_flags & DF_REMOTE)
    {
        // FYI, we know extra_name to be nullptr in this case.
        if (config.active_file())
        {
            dp->m_extra_name = file_exp(config.active_file());
            stat_t extra_stat{};
            if (stat(dp->m_extra_name->c_str(), &extra_stat) >= 0)
            {
                dp->m_act_sf.m_last_fetch = extra_stat.st_mtime;
            }
        }
        else
        {
            dp->m_extra_name = temp_filename();
            dp->m_flags |= DF_TMP_ACTIVE_FILE;
            if (!dp->m_act_sf.m_refetch_secs)
            {
                dp->m_act_sf.m_refetch_secs = 1;
            }
        }

        v = config.group_desc_refetch();
        if (v != nullptr && *v)
        {
            dp->m_desc_sf.m_refetch_secs = text_to_secs(v, g_def_refetch_secs);
        }
        else if (!dp->m_group_desc)
        {
            dp->m_desc_sf.m_refetch_secs = g_def_refetch_secs;
        }
        if (dp->m_group_desc)
        {
            stat_t desc_stat{};
            if (stat(dp->m_group_desc->c_str(), &desc_stat) >= 0)
            {
                dp->m_desc_sf.m_last_fetch = desc_stat.st_mtime;
            }
        }
        else
        {
            dp->m_group_desc = temp_filename();
            dp->m_flags |= DF_TMP_GROUP_DESC;
            if (!dp->m_desc_sf.m_refetch_secs)
            {
                dp->m_desc_sf.m_refetch_secs = 1;
            }
        }
    }
    v = config.force_auth();
    if (v != nullptr && (*v == 'y' || *v == 'Y'))
    {
        dp->m_nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
    }
    v = config.auth_user();
    if (v != nullptr)
    {
        dp->m_auth_user = v;
    }
    v = config.auth_password();
    if (v != nullptr)
    {
        dp->m_auth_pass = v;
    }
    v = config.xhdr_broken();
    if (v != nullptr && (*v == 'y' || *v == 'Y'))
    {
        dp->m_flags |= DF_XHDR_BROKEN;
    }
    v = config.xrefs();
    if (v != nullptr && (*v == 'n' || *v == 'N'))
    {
        dp->m_flags |= DF_NO_XREFS;
    }

    return dp;
}

static std::optional<std::string> dir_or_none(DataSource *dp, const char *dir, DataSourceFlags flag)
{
    if (!dir || !*dir || !std::strcmp(dir, "remote"))
    {
        dp->m_flags |= flag;
        if (dp->m_flags & DF_REMOTE)
        {
            return std::nullopt;
        }
        if (flag == DF_ADD_OK)
        {
            return dp->m_news_id + ".times";
        }
        if (flag == DF_NONE)
        {
            const char *cp = std::strrchr(dp->m_news_id.c_str(), '/');
            if (!cp)
            {
                return std::nullopt;
            }
            const std::size_t len = static_cast<std::size_t>(cp - dp->m_news_id.c_str() + 1);
            return dp->m_news_id.substr(0, len) + "newsgroups";
        }
        return dp->m_spool_dir;
    }

    if (!std::strcmp(dir, "none"))
    {
        return std::nullopt;
    }

    dp->m_flags |= flag;
    const std::string expanded_dir = file_exp(dir);
    if (expanded_dir == dp->m_spool_dir)
    {
        return dp->m_spool_dir;
    }
    return expanded_dir;
}

static std::optional<std::string> file_or_none(const char *fn)
{
    if (!fn || !*fn || !std::strcmp(fn, "none") || !std::strcmp(fn, "remote"))
    {
        return std::nullopt;
    }
    return file_exp(fn);
}

static const char *opt_c_str(const std::optional<std::string> &text)
{
    return text ? text->c_str() : nullptr;
}

bool DataSource::open()
{
    bool success;

    if (m_flags & DF_UNAVAILABLE)
    {
        return false;
    }
    set_data_source(this);
    if (m_flags & DF_OPEN)
    {
        return true;
    }
    if (m_flags & DF_REMOTE)
    {
        if (nntp_connect(m_news_id.c_str(), true) <= 0)
        {
            m_flags |= DF_UNAVAILABLE;
            return false;
        }
        g_nntp_allow_timeout = false;
        m_nntp_link = g_nntp_link;
        if (m_act_sf.m_refetch_secs)
        {
            switch (nntp_list("active", "control"))
            {
            case 1:
                if (std::strncmp(g_ser_line, "control ", 8) != 0)
                {
                    std::strcpy(g_buf, g_ser_line);
                    m_act_sf.m_last_fetch = 0;
                    success = active_file_hash();
                    break;
                }
                if (nntp_gets(g_buf, sizeof g_buf - 1) == NGSR_FULL_LINE //
                    && !nntp_at_list_end(g_buf))
                {
                    nntp_finish_list();
                    success = active_file_hash();
                    break;
                }
                // FALL THROUGH

            case 0:
                m_flags |= DF_USE_LIST_ACTIVE;
                if (m_flags & DF_TMP_ACTIVE_FILE)
                {
                    m_flags &= ~DF_TMP_ACTIVE_FILE;
                    m_extra_name.reset();
                    m_act_sf.m_refetch_secs = 0;
                    success = m_act_sf.open(nullptr, "", nullptr);
                }
                else
                {
                    success = active_file_hash();
                }
                break;

            case -2:
                std::printf("Failed to open news server %s:\n%s\n", m_news_id.c_str(), g_ser_line);
                term_down(2);
                success = false;
                break;

            default:
                success = active_file_hash();
                break;
            }
        }
        else
        {
            success = active_file_hash();
        }
    }
    else
    {
        success = active_file_hash();
    }
    if (success)
    {
        m_flags |= DF_OPEN;
        if (m_flags & DF_TRY_OVERVIEW)
        {
            ov_init();
        }
    }
    else
    {
        m_flags |= DF_UNAVAILABLE;
    }
    if (m_flags & DF_REMOTE)
    {
        g_nntp_allow_timeout = true;
    }
    return success;
}

void set_data_source(DataSource *dp)
{
    if (g_data_source)
    {
        g_data_source->m_nntp_link = g_nntp_link;
    }
    if (dp)
    {
        g_nntp_link = dp->m_nntp_link;
    }
    g_data_source = dp;
}

void check_data_sources()
{
    std::time_t now = std::time(nullptr);

    if (!g_data_sources.empty())
    {
        for (DataSource *dp = data_source_first(); dp; dp = data_source_next(dp))
        {
            if ((dp->m_flags & DF_OPEN) && dp->m_nntp_link.connection)
            {
                std::time_t limit = ((dp->m_flags & DF_ACTIVE) ? 30 * 60 : 10 * 60);
                if (now - dp->m_nntp_link.last_command > limit)
                {
                    DataSource *save_datasrc = g_data_source;
                    set_data_source(dp);
                    nntp_close(true);
                    dp->m_nntp_link = g_nntp_link;
                    set_data_source(save_datasrc);
                }
            }
        }
    }
}

void DataSource ::close()
{
    if (m_flags & DF_REMOTE)
    {
        if (m_flags & DF_TMP_ACTIVE_FILE)
        {
            remove(m_extra_name->c_str());
        }
        else
        {
            m_act_sf.end_append(opt_c_str(m_extra_name));
        }
        if (m_group_desc)
        {
            if (m_flags & DF_TMP_GROUP_DESC)
            {
                remove(m_group_desc->c_str());
            }
            else
            {
                m_desc_sf.end_append(m_group_desc->c_str());
            }
        }
    }

    if (!(m_flags & DF_OPEN))
    {
        return;
    }

    if (m_flags & DF_REMOTE)
    {
        DataSource *save_datasrc = g_data_source;
        set_data_source(this);
        nntp_close(true);
        m_nntp_link = g_nntp_link;
        set_data_source(save_datasrc);
    }
    m_act_sf.close();
    m_desc_sf.close();
    m_flags &= ~DF_OPEN;
    if (g_data_source == this)
    {
        g_data_source = nullptr;
    }
}

bool DataSource::active_file_hash()
{
    int ret;
    if (m_flags & DF_REMOTE)
    {
        DataSource *save_datasrc = g_data_source;
        set_data_source(this);
        g_spin_todo = m_act_sf.m_recent_cnt;
        ret = m_act_sf.open(opt_c_str(m_extra_name), "active", m_news_id.c_str());
        if (g_spin_count > 0)
        {
            m_act_sf.m_recent_cnt = g_spin_count;
        }
        set_data_source(save_datasrc);
    }
    else
    {
        ret = m_act_sf.open(m_news_id.c_str(), "", nullptr);
    }
    return ret != 0;
}

bool DataSource::find_active_group(char *outbuf, std::string_view name, ArticleNum high)
{
    ActivePosition act_pos;
    std::FILE* fp = m_act_sf.m_fp;
    char* lbp;
    int lbp_len;
    const char* name_data = name.empty() ? "" : name.data();
    const int name_len = static_cast<int>(name.size());

    // Do a quick, hashed lookup

    outbuf[0] = '\0';
    HashDatum data = hash_fetch(m_act_sf.m_hp, name);
    if (data.dat_ptr)
    {
        std::string *line = source_file_line(data);
        act_pos = ActivePosition{source_file_position(data)};
        lbp = line->data();
        lbp_len = static_cast<int>(line->size());
    }
    else
    {
        lbp = nullptr;
        lbp_len = 0;
    }
    if (m_flags & DF_USE_LIST_ACTIVE //
        && nntp_flags() & NNTP_NEW_CMD_OK)
    {
        DataSource* save_datasrc = g_data_source;
        set_data_source(this);
        switch (nntp_list("active", name))
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
                (void) m_act_sf.append(outbuf, name_len);
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
        high = ArticleNum{std::atol(outbuf + name_len + 1)};
    }

    if (lbp_len)
    {
        if ((m_flags & DF_REMOTE) && m_act_sf.m_refetch_secs)
        {
            char* cp;
            if (high && high != ArticleNum{std::atol(cp = lbp + name_len + 1)})
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
            && !std::strncmp(outbuf, name_data, name_len) && outbuf[name_len] == ' ')
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

const char *DataSource::find_group_desc(std::string_view group_name)
{
    const int grouplen = static_cast<int>(group_name.size());

    if (!m_group_desc)
    {
        return "";
    }

    if (!m_desc_sf.m_hp)
    {
        int ret;
        if ((m_flags & DF_REMOTE) && m_desc_sf.m_refetch_secs)
        {
            set_data_source(this);
            if ((m_flags & (DF_TMP_GROUP_DESC | DF_NO_XGTITLE)) == DF_TMP_GROUP_DESC //
                && g_net_speed < 5)
            {
                (void) m_desc_sf.open(nullptr, "", nullptr);
                goto try_xgtitle;
            }
            g_spin_todo = m_desc_sf.m_recent_cnt;
            ret = m_desc_sf.open(m_group_desc->c_str(), "newsgroups", m_news_id.c_str());
            if (g_spin_count > 0)
            {
                m_desc_sf.m_recent_cnt = g_spin_count;
            }
        }
        else
        {
            ret = m_desc_sf.open(m_group_desc->c_str(), "", nullptr);
        }
        if (!ret)
        {
            if (m_flags & DF_TMP_GROUP_DESC)
            {
                m_flags &= ~DF_TMP_GROUP_DESC;
                remove(m_group_desc->c_str());
            }
            m_group_desc.reset();
            return "";
        }
        if (ret == 2 || !m_desc_sf.m_refetch_secs)
        {
            m_flags |= DF_NO_XGTITLE;
        }
    }

    if (HashDatum data = hash_fetch(m_desc_sf.m_hp, group_name); data.dat_ptr)
    {
        return source_file_line(data)->c_str() + grouplen + 1;
    }

try_xgtitle:
    if ((m_flags & (DF_REMOTE | DF_NO_XGTITLE)) == DF_REMOTE)
    {
        set_data_source(this);
        if (nntp_xgtitle(group_name) > 0)
        {
            nntp_gets(g_buf, sizeof g_buf - 1); // TODO: check return value?
            if (nntp_at_list_end(g_buf))
            {
                const std::string group_name_string{group_name};
                std::snprintf(g_buf, sizeof g_buf, "%s \n", group_name_string.c_str());
            }
            else
            {
                nntp_finish_list();
                std::strcat(g_buf, "\n");
            }
            const std::string_view stored_group = m_desc_sf.append(g_buf, grouplen);
            return stored_group.data() + grouplen + 1;
        }
        m_flags |= DF_NO_XGTITLE;
        if (m_desc_sf.m_lines.empty())
        {
            m_desc_sf.close();
            if (m_flags & DF_TMP_GROUP_DESC)
            {
                return find_group_desc(group_name);
            }
            m_group_desc.reset();
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

int SourceFile::open(const char *filename, std::string_view fetch_cmd, const char *server)
{
    char             *s;
    long              pos = 0;
    int               linelen;
    std::FILE        *fp;
    std::time_t       now = std::time(nullptr);
    bool              use_buffered_nntp_gets = false;
    const std::string fetch_command{fetch_cmd};

    if (!filename)
    {
        fp = nullptr;
    }
    else if (server)
    {
        if (!m_refetch_secs)
        {
            server = nullptr;
            fp = std::fopen(filename, "r");
            g_spin_todo = 0;
        }
        else if (now - m_last_fetch > m_refetch_secs && (m_refetch_secs != 2 || !m_last_fetch))
        {
            fp = std::fopen(filename, "w+");
            if (fp)
            {
                std::printf("Getting %s file from %s.", fetch_command.c_str(), server);
                std::fflush(stdout);
                // tell server we want the file
                if (!(g_nntp_link.flags & NNTP_NEW_CMD_OK))
                {
                    use_buffered_nntp_gets = true;
                }
                else if (nntp_list(fetch_cmd, "") < 0)
                {
                    std::printf("\nCan't get %s file from server: \n%s\n",
                           fetch_command.c_str(), g_ser_line);
                    term_down(2);
                    std::fclose(fp);
                    return 0;
                }
                m_last_fetch = now;
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
                m_refetch_secs = 0;
                fp = std::fopen(filename, "r");
            }
            g_spin_todo = 0;
        }
        if (m_refetch_secs & 3)
        {
            m_refetch_secs += 365L * 24 * 60 * 60;
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

    close();

    m_hp = hash_create(3001, source_file_cmp);
    m_fp = fp;

    if (!filename)
    {
        set_spin(SPIN_OFF);
        return 1;
    }

    for (;; pos += linelen)
    {
        if (server)
        {
            if (use_buffered_nntp_gets)
            {
                use_buffered_nntp_gets = false;
            }
            else if (nntp_gets(g_buf, sizeof g_buf - 1) == NGSR_ERROR)
            {
                std::printf("\nError getting %s file.\n", fetch_command.c_str());
                term_down(2);
                close();
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
        const std::size_t index = m_lines.size();
        m_line_positions.push_back(pos);
        m_lines.emplace_back(g_buf, static_cast<std::size_t>(linelen));
        hash_store(m_hp, std::string_view{g_buf, static_cast<std::size_t>(keylen)},
                   source_file_hash_datum(this, index));
    }
    set_spin(SPIN_OFF);

    if (server)
    {
        std::fflush(fp);
        if (std::ferror(fp))
        {
            std::printf("\nError writing the %s file %s.\n",fetch_command.c_str(),filename);
            term_down(2);
            close();
            return 0;
        }
        newline();
    }
    std::fseek(fp,0L,0);

    return server? 2 : 1;
}

std::string_view SourceFile::append(std::string_view line, int key_len)
{
    const long pos = m_lines.empty() ? 0 : m_line_positions.back() + static_cast<long>(m_lines.back().size());

    std::string stored_line{line};
    char       *line_start = stored_line.data();
    char       *s = line_start + key_len + 1;
    if (m_fp && m_refetch_secs && *s != '\n')
    {
        std::fseek(m_fp, 0, 2);
        std::fwrite(line.data(), 1, line.size(), m_fp);
    }

    if (*s != '\n' && std::isspace(*s))
    {
        while (*++s != '\n' && std::isspace(*s))
        {
        }
        const std::size_t content_pos = static_cast<std::size_t>(key_len + 1);
        stored_line.erase(content_pos, static_cast<std::size_t>(s - (line_start + content_pos)));
        line_start = stored_line.data();
        s = line_start + content_pos;
    }
    s = adv_then_find_next_nl_and_dectrl(s);
    const std::size_t linelen = static_cast<std::size_t>(s - line_start + 1);
    if (*s != '\n')
    {
        stored_line.resize(linelen - 1);
        stored_line.push_back('\n');
    }
    else
    {
        stored_line.resize(linelen);
    }
    const std::size_t index = m_lines.size();
    m_line_positions.push_back(pos);
    m_lines.push_back(std::move(stored_line));
    const std::string &stored = m_lines.back();
    hash_store(m_hp, std::string_view{stored.data(), static_cast<std::size_t>(key_len)},
               source_file_hash_datum(this, index));

    return stored;
}

void SourceFile::end_append(const char *filename)
{
    if (m_fp && m_refetch_secs)
    {
        std::fflush(m_fp);

        if (m_last_fetch)
        {
            struct utimbuf ut;
            std::time(&ut.actime);
            ut.modtime = m_last_fetch;
            (void) utime(filename, &ut);
        }
    }
}

void SourceFile::close()
{
    if (m_fp)
    {
        std::fclose(m_fp);
        m_fp = nullptr;
    }
    if (m_hp)
    {
        hash_destroy(m_hp);
        m_hp = nullptr;
    }
    m_lines.clear();
    m_line_positions.clear();
}

static HashDatum source_file_hash_datum(SourceFile *source_file, std::size_t index)
{
    return {reinterpret_cast<char *>(source_file), static_cast<unsigned>(index)};
}

static SourceFile *source_file_from_hash(HashDatum data)
{
    return reinterpret_cast<SourceFile *>(data.dat_ptr);
}

static std::string *source_file_line(HashDatum data)
{
    SourceFile *source_file = source_file_from_hash(data);
    return &source_file->m_lines[data.dat_len];
}

static long source_file_position(HashDatum data)
{
    SourceFile *source_file = source_file_from_hash(data);
    return source_file->m_line_positions[data.dat_len];
}

static int source_file_cmp(std::string_view key, HashDatum data)
{
    const std::string *line = source_file_line(data);
    if (line->size() < key.size())
    {
        return 1;
    }
    const std::string_view line_key{line->data(), key.size()};

    return key.compare(line_key);
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
    for (DataSource *dp = data_source_first(); dp; dp = data_source_next(dp))
    {
        if (dp->m_flags & DF_OPEN)
        {
            if (dp->m_act_sf.m_hp)
            {
                hash_walk(dp->m_act_sf.m_hp, check_distance, 0);
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
        name = ((NewsgroupData *) data->dat_ptr)->m_rc_line;
    }
    else
    {
        name = source_file_line(*data)->data();
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

    const std::string_view newsgroup_name{
            g_newsgroup_name.data(), g_newsgroup_name.size()};
    const std::string_view candidate_name{
            name != nullptr ? name : "",
            name != nullptr && len > 0 ? static_cast<std::size_t>(len) : 0};
    int value = edit_distn(newsgroup_name, candidate_name);
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
    std::string options;

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
        fmt::print("  {}.  {}\n", i + 1, s_newsgroup_ptrs[i]);
        options += std::to_string(i + 1);
        if (cp)
        {
            *cp = ' ';
        }
    }
    options += 'n';

    const std::string prompt{g_verbose ? "Which of these would you like?" : "Which?"};
reask:
    in_char(prompt.c_str(), MM_ADD_NEWSGROUP_PROMPT, options.c_str());
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
            const std::size_t pos = options.find(*g_buf);

            int i = pos != std::string::npos ? static_cast<int>(pos) : s_newsgroup_num;
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
