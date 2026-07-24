/* datasrc.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/datasrc.h>

#include <file_contents.h>

#include <config/common.h>
#include <config/env.h>
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
#include <charconv>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

std::vector<DataSource> g_data_sources;                           // all data sources
DataSource             *g_data_source{};                          // the current data source
std::string             g_trn_access_text;                        //
std::string             g_nntp_auth_file;                         //
time_t                  g_def_refetch_secs{DEFAULT_REFETCH_SECS}; // -z

static std::string                dir_or_empty(DataSource *dp, std::string_view dir, DataSourceFlags flag);
static std::string                file_or_empty(std::string_view fn);
static HashDatum                  source_file_hash_datum(SourceFile *source_file, std::size_t index);
static SourceFile                *source_file_from_hash(HashDatum data);
static std::string               *source_file_line(HashDatum data);
static long                       source_file_position(HashDatum data);
static int                        source_file_cmp(std::string_view key, HashDatum data);
static void                       check_distance(std::string_view candidate_name,
                                                 std::vector<std::string> &newsgroup_matches, int &best_match);
static int                        get_near_miss(const std::vector<std::string> &newsgroup_matches);
static DataSource                *new_data_source(std::string_view name, const DataSourceConfig &config);
static std::string                read_data_sources(std::string_view filename);

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
/// - `g_trn_access_text`: TRN access configuration text.
///
void data_source_init()
{
    std::string_view active_file;

    g_data_sources.clear();
    g_data_sources.reserve(20);

    g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);

    std::string       server_name = get_env_var("NNTPSERVER");
    const std::string force_auth = get_env_var("NNTP_FORCE_AUTH");
    if (!server_name.empty() && server_name != "local")
    {
        DataSourceConfig      config;
        const AuthCredentials credentials = read_auth_file(g_nntp_auth_file);
        config.set_nntp_server(server_name);
        config.set_auth_user(credentials.user);
        config.set_auth_password(credentials.password);
        config.set_force_auth(force_auth);
        new_data_source("default", config);
    }

    g_trn_access_text = read_data_sources(TRNACCESS);
    std::string default_access_text = read_data_sources(DEFACCESS);
    if (g_trn_access_text.empty())
    {
        g_trn_access_text = std::move(default_access_text);
    }

    if (server_name.empty())
    {
        server_name = file_exp(SERVER_NAME);
        if (!server_name.empty())
        {
            if (file_ref(server_name))
            {
                server_name = nntp_server_name(server_name);
            }
        }
        if (server_name == "local")
        {
            server_name.clear();
            active_file = ACTIVE;
        }
        DataSourceConfig config;
        if (!server_name.empty())
        {
            config.set_nntp_server(server_name);
        }
        if (!active_file.empty())
        {
            config.set_active_file(active_file);
        }
        config.set_spool_dir(NEWS_SPOOL);
        config.set_overview_dir(OVERVIEW_DIR);
        config.set_overview_format_file(OVERVIEW_FMT);
        config.set_active_times(ACTIVE_TIMES);
        config.set_group_desc(GROUP_DESC);
        if (!server_name.empty())
        {
            const AuthCredentials credentials = read_auth_file(g_nntp_auth_file);
            config.set_auth_user(credentials.user);
            config.set_auth_password(credentials.password);
            config.set_force_auth(force_auth);
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
    g_trn_access_text.clear();
    g_nntp_auth_file.clear();
}

/// @brief Reads data sources from the specified file.
///
/// This function reads and parses data sources from an INI-style file. It
/// processes each section and conditionally creates new data sources based
/// on the parsed values.
///
/// @param filename The name of the file to read data sources from.
/// @return The file contents, or an empty string if the file could not be
///         opened or read.
///
static std::string read_data_sources(std::string_view filename)
{
    IniSectionValues values;
    std::string      contents = file_contents(file_exp(filename));

    if (contents.empty())
    {
        return {};
    }

    IniDocument document{contents, filename};
    for (const IniSection section : document)
    {
        if (section.has_condition())
        {
            if (!check_ini_cond(section.condition()))
            {
                continue;
            }
        }

        const std::string_view section_name = section.name();
        if (string_case_equal(section_name.substr(0, 6), "group "))
        {
            continue;
        }
        parse_ini_section(section, DataSourceConfig::schema(), values);
        new_data_source(section_name, DataSourceConfig::from(values));
    }
    return contents;
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

static DataSource *new_data_source(std::string_view name, const DataSourceConfig &config)
{
    const std::optional<std::string_view> nntp_server = config.nntp_server();
    const std::optional<std::string_view> active_file = config.active_file();

    if (!nntp_server && !active_file)
    {
        return nullptr;
    }

    DataSource *dp = &g_data_sources.emplace_back();

    if (nntp_server)
    {
        dp->m_flags |= DF_REMOTE;
    }

    dp->m_name.assign(name);
    if (dp->m_name == "default")
    {
        dp->m_flags |= DF_DEFAULT;
    }

    if (nntp_server)
    {
        dp->m_news_id = *nntp_server;
        const std::string::size_type port_separator = dp->m_news_id.find(';');
        if (port_separator != std::string::npos)
        {
            const std::string_view port_text{dp->m_news_id.data() + port_separator + 1,
                                             dp->m_news_id.size() - port_separator - 1};
            (void) std::from_chars(port_text.data(), port_text.data() + port_text.size(), dp->m_nntp_link.port_number);
            dp->m_news_id.resize(port_separator);
        }

        const std::optional<std::string_view> active_file_refetch = config.active_file_refetch();
        if (active_file_refetch && !active_file_refetch->empty())
        {
            dp->m_act_sf.m_refetch_secs = text_to_secs(*active_file_refetch, g_def_refetch_secs);
        }
        else if (!active_file)
        {
            dp->m_act_sf.m_refetch_secs = g_def_refetch_secs;
        }
    }
    else
    {
        dp->m_news_id = file_exp(*active_file);
    }

    if (std::string spool_dir = file_or_empty(config.spool_dir().value_or(std::string_view{})); !spool_dir.empty())
    {
        dp->m_spool_dir = spool_dir;
    }
    else
    {
        dp->m_spool_dir = g_tmp_dir;
    }

    dp->m_over_dir = dir_or_empty(dp, config.overview_dir().value_or(std::string_view{}), DF_TRY_OVERVIEW);
    dp->m_over_fmt = file_or_empty(config.overview_format_file().value_or(std::string_view{}));
    dp->m_group_desc = dir_or_empty(dp, config.group_desc().value_or(std::string_view{}), DF_NONE);
    dp->m_extra_name = dir_or_empty(dp, config.active_times().value_or(std::string_view{}), DF_ADD_OK);
    if (dp->m_flags & DF_REMOTE)
    {
        // FYI, we know extra_name is empty in this case.
        if (active_file)
        {
            dp->m_extra_name = file_exp(*active_file);
            stat_t extra_stat{};
            if (stat(dp->m_extra_name.c_str(), &extra_stat) >= 0)
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

        const std::optional<std::string_view> group_desc_refetch = config.group_desc_refetch();
        if (group_desc_refetch && !group_desc_refetch->empty())
        {
            dp->m_desc_sf.m_refetch_secs = text_to_secs(*group_desc_refetch, g_def_refetch_secs);
        }
        else if (dp->m_group_desc.empty())
        {
            dp->m_desc_sf.m_refetch_secs = g_def_refetch_secs;
        }
        if (!dp->m_group_desc.empty())
        {
            stat_t desc_stat{};
            if (stat(dp->m_group_desc.c_str(), &desc_stat) >= 0)
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
    if (const std::optional<std::string_view> force_auth = config.force_auth();
        force_auth && !force_auth->empty() && (force_auth->front() == 'y' || force_auth->front() == 'Y'))
    {
        dp->m_nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
    }
    if (const std::optional<std::string_view> auth_user = config.auth_user(); auth_user && !auth_user->empty())
    {
        dp->m_auth_user = *auth_user;
    }
    if (const std::optional<std::string_view> auth_password = config.auth_password();
        auth_password && !auth_password->empty())
    {
        dp->m_auth_pass = *auth_password;
    }
    if (const std::optional<std::string_view> xhdr_broken = config.xhdr_broken();
        xhdr_broken && !xhdr_broken->empty() && (xhdr_broken->front() == 'y' || xhdr_broken->front() == 'Y'))
    {
        dp->m_flags |= DF_XHDR_BROKEN;
    }
    if (const std::optional<std::string_view> xrefs = config.xrefs();
        xrefs && !xrefs->empty() && (xrefs->front() == 'n' || xrefs->front() == 'N'))
    {
        dp->m_flags |= DF_NO_XREFS;
    }

    return dp;
}

static std::string dir_or_empty(DataSource *dp, std::string_view dir, DataSourceFlags flag)
{
    if (dir.empty() || dir == "remote")
    {
        dp->m_flags |= flag;
        if (dp->m_flags & DF_REMOTE)
        {
            return {};
        }
        if (flag == DF_ADD_OK)
        {
            return dp->m_news_id + ".times";
        }
        if (flag == DF_NONE)
        {
            const std::size_t slash = dp->m_news_id.rfind('/');
            if (slash == std::string::npos)
            {
                return {};
            }
            return dp->m_news_id.substr(0, slash + 1) + "newsgroups";
        }
        return dp->m_spool_dir;
    }

    if (dir == "none")
    {
        return {};
    }

    dp->m_flags |= flag;
    const std::string expanded_dir = file_exp(dir);
    if (expanded_dir == dp->m_spool_dir)
    {
        return dp->m_spool_dir;
    }
    return expanded_dir;
}

static std::string file_or_empty(std::string_view file)
{
    if (file.empty() || file == "none" || file == "remote")
    {
        return {};
    }
    return file_exp(file);
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
            constexpr std::string_view control_group_prefix{"control "};
            std::string                active_line;
            active_line.reserve(NNTP_STRLEN);
            switch (nntp_list("active", "control", active_line))
            {
            case 1:
            {
                if (std::string_view{active_line}.substr(0, control_group_prefix.size()) != control_group_prefix)
                {
                    m_act_sf.m_last_fetch = 0;
                    success = active_file_hash(active_line);
                    break;
                }
                std::string next_active_line;
                next_active_line.reserve(LINE_BUF_LEN);
                if (nntp_gets(next_active_line, LINE_BUF_LEN) == NGSR_FULL_LINE //
                    && !nntp_at_list_end(next_active_line))
                {
                    nntp_finish_list();
                    success = active_file_hash();
                    break;
                }
                // FALL THROUGH
            }

            case 0:
                m_flags |= DF_USE_LIST_ACTIVE;
                if (m_flags & DF_TMP_ACTIVE_FILE)
                {
                    m_flags &= ~DF_TMP_ACTIVE_FILE;
                    m_extra_name.clear();
                    m_act_sf.m_refetch_secs = 0;
                    success = m_act_sf.open({}, "", "");
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
            std::error_code error;
            fs::remove(m_extra_name, error);
        }
        else
        {
            m_act_sf.end_append(m_extra_name);
        }
        if (!m_group_desc.empty())
        {
            if (m_flags & DF_TMP_GROUP_DESC)
            {
                std::error_code error;
                fs::remove(m_group_desc, error);
            }
            else
            {
                m_desc_sf.end_append(m_group_desc);
            }
        }
    }

    if (!(m_flags & DF_OPEN))
    {
        return;
    }

    if (m_flags & DF_REMOTE)
    {
        DataSource* save_datasrc = g_data_source;
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

bool DataSource::active_file_hash(std::string_view first_line)
{
    int ret;
    if (m_flags & DF_REMOTE)
    {
        DataSource *save_datasrc = g_data_source;
        set_data_source(this);
        g_spin_todo = m_act_sf.m_recent_cnt;
        ret = m_act_sf.open(m_extra_name, "active", m_news_id, first_line);
        if (g_spin_count > 0)
        {
            m_act_sf.m_recent_cnt = g_spin_count;
        }
        set_data_source(save_datasrc);
    }
    else
    {
        ret = m_act_sf.open(m_news_id, "", "");
    }
    return ret != 0;
}

std::string DataSource::find_active_group(std::string_view name, ArticleNum high)
{
    ActivePosition    act_pos{};
    std::FILE        *fp = m_act_sf.m_fp;
    std::string      *cached_line{};
    const std::size_t name_len = name.size();

    // Do a quick, hashed lookup

    HashDatum data = hash_fetch(m_act_sf.m_hp, name);
    if (data.dat_ptr)
    {
        cached_line = source_file_line(data);
        act_pos = ActivePosition{source_file_position(data)};
    }
    std::string active_line;
    if (m_flags & DF_USE_LIST_ACTIVE //
        && nntp_flags() & NNTP_NEW_CMD_OK)
    {
        DataSource *save_datasrc = g_data_source;
        set_data_source(this);
        switch (nntp_list("active", name, active_line))
        {
        case 0:
            set_data_source(save_datasrc);
            return {};

        case 1:
            active_line += '\n';
            nntp_finish_list();
            break;

        case -2:
            // TODO
            break;
        }
        set_data_source(save_datasrc);
        if (cached_line == nullptr)
        {
            if (fp && !active_line.empty())
            {
                (void) m_act_sf.append(active_line, static_cast<int>(name_len));
            }
            return active_line;
        }

        if (!active_line.empty())
        {
# ifndef ANCIENT_NEWS
            // Safely update the low-water mark
            const std::size_t active_flag_separator = active_line.rfind(' ');
            const std::size_t cached_flag_separator = cached_line->rfind(' ');
            if (active_flag_separator != std::string::npos && active_flag_separator > 0 &&
                cached_flag_separator != std::string::npos && cached_flag_separator > 0)
            {
                const std::size_t active_low_separator = active_line.rfind(' ', active_flag_separator - 1);
                const std::size_t cached_low_separator = cached_line->rfind(' ', cached_flag_separator - 1);
                if (active_low_separator != std::string::npos && cached_low_separator != std::string::npos)
                {
                    const std::string_view active_low{active_line.data() + active_low_separator + 1,
                                                      active_flag_separator - active_low_separator - 1};
                    const std::size_t      field_width = cached_flag_separator - cached_low_separator - 1;
                    const std::string_view low_digits =
                        active_low.substr(active_low.size() > field_width ? active_low.size() - field_width : 0);
                    cached_line->replace(cached_low_separator + 1, field_width,
                                         fmt::format("{:0>{}}", low_digits, field_width));
                }
            }
# endif
            const std::string_view active_fields =
                std::string_view{active_line}.substr(std::min(name_len + 1, active_line.size()));
            long active_high{};
            std::from_chars(active_fields.data(), active_fields.data() + active_fields.size(), active_high);
            high = ArticleNum{active_high};
        }
    }

    if (cached_line != nullptr)
    {
        if ((m_flags & DF_REMOTE) && m_act_sf.m_refetch_secs)
        {
            const std::string_view cached_fields =
                std::string_view{*cached_line}.substr(std::min(name_len + 1, cached_line->size()));
            long cached_high{};
            std::from_chars(cached_fields.data(), cached_fields.data() + cached_fields.size(), cached_high);
            if (high && high != ArticleNum{cached_high})
            {
                const std::size_t high_start = name_len + 1;
                const std::size_t high_end = cached_line->find(' ', high_start);
                if (high_end != std::string::npos)
                {
                    const std::size_t      field_width = high_end - high_start;
                    const std::string      high_text = std::to_string(value_of(high));
                    const std::string_view high_digits = std::string_view{high_text}.substr(
                        high_text.size() > field_width ? high_text.size() - field_width : 0);
                    cached_line->replace(high_start, field_width, fmt::format("{:0>{}}", high_digits, field_width));
                }
                std::fseek(fp, act_pos.value_of(), 0);
                fmt::print(fp, "{}", *cached_line);
            }
            return *cached_line;
        }

        // hopefully this forces a reread
        std::fseek(fp,2000000000L,1);

        // if line has changed length or is not there, we should
        // discard/close the active file, and re-open it.
        if (std::fseek(fp, act_pos.value_of(), 0) >= 0)
        {
            std::string file_line;
            file_line.reserve(LINE_BUF_LEN);
            for (int ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp))
            {
                file_line += static_cast<char>(ch);
                if (ch == '\n')
                {
                    break;
                }
            }
            if (file_line.size() > name_len && std::string_view{file_line}.substr(0, name_len) == name &&
                file_line[name_len] == ' ')
            {
                // Remember the latest info in our cache.
                *cached_line = file_line;
                return file_line;
            }
        }
        return *cached_line;
    }
    return {}; // no such group
}

std::string_view DataSource::find_group_desc(std::string_view group_name)
{
    const int grouplen = static_cast<int>(group_name.size());

    if (m_group_desc.empty())
    {
        return {};
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
                (void) m_desc_sf.open({}, "", "");
                goto try_xgtitle;
            }
            g_spin_todo = m_desc_sf.m_recent_cnt;
            ret = m_desc_sf.open(m_group_desc, "newsgroups", m_news_id);
            if (g_spin_count > 0)
            {
                m_desc_sf.m_recent_cnt = g_spin_count;
            }
        }
        else
        {
            ret = m_desc_sf.open(m_group_desc, "", "");
        }
        if (!ret)
        {
            if (m_flags & DF_TMP_GROUP_DESC)
            {
                m_flags &= ~DF_TMP_GROUP_DESC;
                std::error_code error;
                fs::remove(m_group_desc, error);
            }
            m_group_desc.clear();
            return {};
        }
        if (ret == 2 || !m_desc_sf.m_refetch_secs)
        {
            m_flags |= DF_NO_XGTITLE;
        }
    }

    if (HashDatum data = hash_fetch(m_desc_sf.m_hp, group_name); data.dat_ptr)
    {
        return std::string_view{*source_file_line(data)}.substr(grouplen + 1);
    }

try_xgtitle:
    if ((m_flags & (DF_REMOTE | DF_NO_XGTITLE)) == DF_REMOTE)
    {
        set_data_source(this);
        if (nntp_xgtitle(group_name) > 0)
        {
            std::string description_line;
            description_line.reserve(NNTP_STRLEN);
            if (nntp_gets(description_line, NNTP_STRLEN - 1) == NGSR_ERROR)
            {
                description_line = g_ser_line;
            }
            if (nntp_at_list_end(description_line))
            {
                return m_desc_sf.append(fmt::format("{} \n", group_name), grouplen).substr(grouplen + 1);
            }
            description_line += '\n';
            nntp_finish_list();
            return m_desc_sf.append(description_line, grouplen).substr(grouplen + 1);
        }
        m_flags |= DF_NO_XGTITLE;
        if (m_desc_sf.m_lines.empty())
        {
            m_desc_sf.close();
            if (m_flags & DF_TMP_GROUP_DESC)
            {
                return find_group_desc(group_name);
            }
            m_group_desc.clear();
        }
    }
    return {};
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

int SourceFile::open(const fs::path &filename, std::string_view fetch_cmd, std::string_view server,
                     std::string_view first_line)
{
    long              pos = 0;
    std::FILE        *fp;
    std::time_t       now = std::time(nullptr);
    bool              use_first_line = false;
    const bool        has_filename = !filename.empty();
    bool              use_server = !server.empty();

    if (!has_filename)
    {
        fp = nullptr;
    }
    else if (use_server)
    {
        if (!m_refetch_secs)
        {
            use_server = false;
            fp = std::fopen(filename.string().c_str(), "r");
            g_spin_todo = 0;
        }
        else if (now - m_last_fetch > m_refetch_secs && (m_refetch_secs != 2 || !m_last_fetch))
        {
            fp = std::fopen(filename.string().c_str(), "w+");
            if (fp)
            {
                fmt::print("Getting {} file from {}.", fetch_cmd, server);
                std::fflush(stdout);
                // tell server we want the file
                if (!(g_nntp_link.flags & NNTP_NEW_CMD_OK))
                {
                    use_first_line = !first_line.empty();
                }
                else if (nntp_list(fetch_cmd, "") < 0)
                {
                    fmt::print("\nCan't get {} file from server: \n{}\n", fetch_cmd, g_ser_line);
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
            use_server = false;
            fp = std::fopen(filename.string().c_str(), "r+");
            if (!fp)
            {
                m_refetch_secs = 0;
                fp = std::fopen(filename.string().c_str(), "r");
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
        fp = std::fopen(filename.string().c_str(), "r");
        g_spin_todo = 0;
    }

    if (has_filename && fp == nullptr)
    {
        fmt::print("Can't open {}\n", filename.string());
        term_down(1);
        return 0;
    }
    set_spin(g_spin_todo > 0? SPIN_BAR_GRAPH : SPIN_FOREGROUND);

    close();

    m_hp = hash_create(3001, source_file_cmp);
    m_fp = fp;

    if (!has_filename)
    {
        set_spin(SPIN_OFF);
        return 1;
    }

    std::string line;
    line.reserve(LINE_BUF_LEN);
    std::string remote_line{first_line};
    remote_line.reserve(LINE_BUF_LEN);
    for (;;)
    {
        line.clear();
        if (use_server)
        {
            if (use_first_line)
            {
                use_first_line = false;
            }
            else if (nntp_gets(remote_line, LINE_BUF_LEN) == NGSR_ERROR)
            {
                fmt::print("\nError getting {} file.\n", fetch_cmd);
                term_down(2);
                close();
                set_spin(SPIN_OFF);
                return 0;
            }
            if (nntp_at_list_end(remote_line))
            {
                break;
            }
            line = remote_line;
            line += '\n';
            fmt::print(fp, "{}", line);
            spin(200 * g_net_speed);
        }
        else
        {
            for (int ch = std::fgetc(fp); ch != EOF; ch = std::fgetc(fp))
            {
                line += static_cast<char>(ch);
                if (ch == '\n')
                {
                    break;
                }
            }
            if (line.empty())
            {
                break;
            }
        }

        const std::string::iterator key_end = std::find_if(line.begin(), line.end(), [](char ch)
                                                           { return std::isspace(static_cast<unsigned char>(ch)); });
        if (key_end == line.end())
        {
            continue;
        }
        const std::size_t key_len = static_cast<std::size_t>(key_end - line.begin());
        const std::size_t value_offset = key_len + 1;
        if (value_offset < line.size() && line[value_offset] != '\n' &&
            std::isspace(static_cast<unsigned char>(line[value_offset])))
        {
            const std::string::iterator value_start =
                std::find_if(line.begin() + value_offset, line.end(),
                             [](char ch) { return ch == '\n' || !std::isspace(static_cast<unsigned char>(ch)); });
            line.erase(value_offset, static_cast<std::size_t>(value_start - line.begin()) - value_offset);
        }
        std::size_t line_end = line.find('\n', value_offset);
        if (line_end == std::string::npos)
        {
            line_end = line.size();
        }
        for (std::size_t offset = std::min(value_offset + 1, line_end); offset < line_end;)
        {
            const std::size_t width = static_cast<std::size_t>(byte_length_at(line.data() + offset));
            if (at_grey_space(line.data() + offset))
            {
                std::fill_n(line.begin() + offset, width, ' ');
            }
            offset += width;
        }
        const bool has_newline = line_end < line.size();
        line.resize(line_end + (has_newline ? 1 : 0));
        if (!has_newline)
        {
            line += '\n';
        }
        const std::size_t index = m_lines.size();
        m_line_positions.push_back(pos);
        m_lines.push_back(line);
        hash_store(m_hp, std::string_view{m_lines.back()}.substr(0, key_len), source_file_hash_datum(this, index));
        pos += static_cast<long>(line.size());
    }
    set_spin(SPIN_OFF);

    if (use_server)
    {
        std::fflush(fp);
        if (std::ferror(fp))
        {
            fmt::print("\nError writing the {} file {}.\n", fetch_cmd, filename.string());
            term_down(2);
            close();
            return 0;
        }
        newline();
    }
    std::fseek(fp,0L,0);

    return use_server ? 2 : 1;
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

void SourceFile::end_append(const fs::path &filename)
{
    if (m_fp && m_refetch_secs)
    {
        std::fflush(m_fp);

        if (m_last_fetch && !filename.empty())
        {
            struct utimbuf ut;
            std::time(&ut.actime);
            ut.modtime = m_last_fetch;
            (void) utime(filename.string().c_str(), &ut);
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
// all newsgroups with the single smallest error.
// A more flexible approach would keep around the 10 best matches, whether
// or not they had precisely the same edit distance, but oh well.
//

int find_close_match()
{
    int                      ret = 0;
    int                      best_match = -1;
    std::vector<std::string> newsgroup_matches;
    newsgroup_matches.reserve(MAX_NG);

    // Iterate over all legal newsgroups
    for (DataSource *dp = data_source_first(); dp; dp = data_source_next(dp))
    {
        if (dp->m_flags & DF_OPEN)
        {
            if (dp->m_act_sf.m_hp)
            {
                for (const std::string &line : dp->m_act_sf.m_lines)
                {
                    const std::string::const_iterator key_end = std::find_if(
                        line.begin(), line.end(), [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
                    check_distance(std::string_view{line}.substr(0, static_cast<std::size_t>(key_end - line.begin())),
                                   newsgroup_matches, best_match);
                }
            }
            else
            {
                ret = -1;
            }
        }
    }

    if (ret < 0)
    {
        for (const NewsgroupData *newsgroup : g_newsgroup_order)
        {
            if (newsgroup->m_num_offset != 0 && newsgroup->m_to_read != TR_IGNORE)
            {
                check_distance(newsgroup->rc_name(), newsgroup_matches, best_match);
            }
        }
        ret = 0;
    }

    // If there's just one possibility, go with it.

    switch (newsgroup_matches.size())
    {
    case 0:
        break;
    case 1:
    {
        const std::string &match = newsgroup_matches.front();
        if (g_verbose)
        {
            fmt::print("(I assume you meant {})\n", match);
        }
        else
        {
            fmt::print("(Using {})\n", match);
        }
        set_newsgroup_name(match);
        ret = 1;
        break;
    }

    default:
        ret = get_near_miss(newsgroup_matches);
        break;
    }
    return ret;
}

static void check_distance(std::string_view candidate_name, std::vector<std::string> &newsgroup_matches,
                           int &best_match)
{
    // Efficiency: don't call edit_dist when the lengths are too different.
    const int ngname_len = static_cast<int>(g_newsgroup_name.length());
    const int candidate_len = static_cast<int>(candidate_name.size());
    if (candidate_len < ngname_len)
    {
        if (ngname_len - candidate_len > LENGTH_HACK)
        {
            return;
        }
    }
    else
    {
        if (candidate_len - ngname_len > LENGTH_HACK)
        {
            return;
        }
    }

    const std::string_view newsgroup_name{g_newsgroup_name};
    int                    value = edit_distn(newsgroup_name, candidate_name);
    if (value > MIN_DIST)
    {
        return;
    }

    if (value < best_match)
    {
        newsgroup_matches.clear();
    }
    if (best_match < 0 || value <= best_match)
    {
        if (std::find_if(newsgroup_matches.begin(), newsgroup_matches.end(), [candidate_name](const std::string &match)
                         { return std::string_view{match} == candidate_name; }) != newsgroup_matches.end())
        {
            return;
        }
        best_match = value;
        if (newsgroup_matches.size() < static_cast<std::size_t>(MAX_NG))
        {
            newsgroup_matches.emplace_back(candidate_name);
        }
    }
}

// Now we've got several potential matches, and have to choose between them
// somehow.  Again, results will be returned in global g_newsgroup_name.
//
static int get_near_miss(const std::vector<std::string> &newsgroup_matches)
{
    std::string options;

    if (g_verbose)
    {
        fmt::print("However, here are some close matches:\n");
    }
    for (std::size_t i = 0; i < newsgroup_matches.size(); i++)
    {
        fmt::print("  {}.  {}\n", i + 1, newsgroup_matches[i]);
        options += std::to_string(i + 1);
    }
    options += 'n';

    const std::string prompt{g_verbose ? "Which of these would you like?" : "Which?"};
reask:
    in_char(prompt, MM_ADD_NEWSGROUP_PROMPT, options);
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
            if (pos < newsgroup_matches.size())
            {
                set_newsgroup_name(newsgroup_matches[pos]);
                return 1;
            }
        }
        std::fputs("Type h for help.\n", stdout);
        break;
    }

    settle_down();
    goto reask;
}
