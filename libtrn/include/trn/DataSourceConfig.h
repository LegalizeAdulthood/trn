/* trn/DataSourceConfig.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_DATA_SOURCE_CONFIG_H
#define TRN_DATA_SOURCE_CONFIG_H

class IniSchema;
class IniSectionValues;

#include <optional>
#include <string>
#include <string_view>

enum class DataSourceConfigField
{
    NntpServer = 1,
    ActiveFile,
    ActiveFileRefetch,
    SpoolDir,
    ThreadDir,
    OverviewDir,
    ActiveTimes,
    GroupDesc,
    GroupDescRefetch,
    AuthUser,
    AuthPassword,
    AuthCommand,
    XhdrBroken,
    Xrefs,
    OverviewFormatFile,
    ForceAuth
};

class DataSourceConfig
{
public:
    static const IniSchema &schema();
    static DataSourceConfig from(const IniSectionValues &values);

    std::optional<std::string_view> nntp_server() const;
    std::optional<std::string_view> active_file() const;
    std::optional<std::string_view> active_file_refetch() const;
    std::optional<std::string_view> spool_dir() const;
    std::optional<std::string_view> thread_dir() const;
    std::optional<std::string_view> overview_dir() const;
    std::optional<std::string_view> active_times() const;
    std::optional<std::string_view> group_desc() const;
    std::optional<std::string_view> group_desc_refetch() const;
    std::optional<std::string_view> auth_user() const;
    std::optional<std::string_view> auth_password() const;
    std::optional<std::string_view> auth_command() const;
    std::optional<std::string_view> xhdr_broken() const;
    std::optional<std::string_view> xrefs() const;
    std::optional<std::string_view> overview_format_file() const;
    std::optional<std::string_view> force_auth() const;

    void set_nntp_server(std::string_view value);
    void set_active_file(std::string_view value);
    void set_active_file_refetch(std::string_view value);
    void set_spool_dir(std::string_view value);
    void set_thread_dir(std::string_view value);
    void set_overview_dir(std::string_view value);
    void set_active_times(std::string_view value);
    void set_group_desc(std::string_view value);
    void set_group_desc_refetch(std::string_view value);
    void set_auth_user(std::string_view value);
    void set_auth_password(std::string_view value);
    void set_auth_command(std::string_view value);
    void set_xhdr_broken(std::string_view value);
    void set_xrefs(std::string_view value);
    void set_overview_format_file(std::string_view value);
    void set_force_auth(std::string_view value);

private:
    static std::optional<std::string_view> view(const std::optional<std::string> &value);
    static void                            set_value(std::optional<std::string> &target, std::string_view value);

    std::optional<std::string> m_nntp_server;
    std::optional<std::string> m_active_file;
    std::optional<std::string> m_active_file_refetch;
    std::optional<std::string> m_spool_dir;
    std::optional<std::string> m_thread_dir;
    std::optional<std::string> m_overview_dir;
    std::optional<std::string> m_active_times;
    std::optional<std::string> m_group_desc;
    std::optional<std::string> m_group_desc_refetch;
    std::optional<std::string> m_auth_user;
    std::optional<std::string> m_auth_password;
    std::optional<std::string> m_auth_command;
    std::optional<std::string> m_xhdr_broken;
    std::optional<std::string> m_xrefs;
    std::optional<std::string> m_overview_format_file;
    std::optional<std::string> m_force_auth;
};

inline std::optional<std::string_view> DataSourceConfig::view(const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return std::string_view{*value};
}

inline std::optional<std::string_view> DataSourceConfig::nntp_server() const
{
    return view(m_nntp_server);
}

inline std::optional<std::string_view> DataSourceConfig::active_file() const
{
    return view(m_active_file);
}

inline std::optional<std::string_view> DataSourceConfig::active_file_refetch() const
{
    return view(m_active_file_refetch);
}

inline std::optional<std::string_view> DataSourceConfig::spool_dir() const
{
    return view(m_spool_dir);
}

inline std::optional<std::string_view> DataSourceConfig::thread_dir() const
{
    return view(m_thread_dir);
}

inline std::optional<std::string_view> DataSourceConfig::overview_dir() const
{
    return view(m_overview_dir);
}

inline std::optional<std::string_view> DataSourceConfig::active_times() const
{
    return view(m_active_times);
}

inline std::optional<std::string_view> DataSourceConfig::group_desc() const
{
    return view(m_group_desc);
}

inline std::optional<std::string_view> DataSourceConfig::group_desc_refetch() const
{
    return view(m_group_desc_refetch);
}

inline std::optional<std::string_view> DataSourceConfig::auth_user() const
{
    return view(m_auth_user);
}

inline std::optional<std::string_view> DataSourceConfig::auth_password() const
{
    return view(m_auth_password);
}

inline std::optional<std::string_view> DataSourceConfig::auth_command() const
{
    return view(m_auth_command);
}

inline std::optional<std::string_view> DataSourceConfig::xhdr_broken() const
{
    return view(m_xhdr_broken);
}

inline std::optional<std::string_view> DataSourceConfig::xrefs() const
{
    return view(m_xrefs);
}

inline std::optional<std::string_view> DataSourceConfig::overview_format_file() const
{
    return view(m_overview_format_file);
}

inline std::optional<std::string_view> DataSourceConfig::force_auth() const
{
    return view(m_force_auth);
}

inline void DataSourceConfig::set_nntp_server(std::string_view value)
{
    set_value(m_nntp_server, value);
}

inline void DataSourceConfig::set_active_file(std::string_view value)
{
    set_value(m_active_file, value);
}

inline void DataSourceConfig::set_active_file_refetch(std::string_view value)
{
    set_value(m_active_file_refetch, value);
}

inline void DataSourceConfig::set_spool_dir(std::string_view value)
{
    set_value(m_spool_dir, value);
}

inline void DataSourceConfig::set_thread_dir(std::string_view value)
{
    set_value(m_thread_dir, value);
}

inline void DataSourceConfig::set_overview_dir(std::string_view value)
{
    set_value(m_overview_dir, value);
}

inline void DataSourceConfig::set_active_times(std::string_view value)
{
    set_value(m_active_times, value);
}

inline void DataSourceConfig::set_group_desc(std::string_view value)
{
    set_value(m_group_desc, value);
}

inline void DataSourceConfig::set_group_desc_refetch(std::string_view value)
{
    set_value(m_group_desc_refetch, value);
}

inline void DataSourceConfig::set_auth_user(std::string_view value)
{
    set_value(m_auth_user, value);
}

inline void DataSourceConfig::set_auth_password(std::string_view value)
{
    set_value(m_auth_password, value);
}

inline void DataSourceConfig::set_auth_command(std::string_view value)
{
    set_value(m_auth_command, value);
}

inline void DataSourceConfig::set_xhdr_broken(std::string_view value)
{
    set_value(m_xhdr_broken, value);
}

inline void DataSourceConfig::set_xrefs(std::string_view value)
{
    set_value(m_xrefs, value);
}

inline void DataSourceConfig::set_overview_format_file(std::string_view value)
{
    set_value(m_overview_format_file, value);
}

inline void DataSourceConfig::set_force_auth(std::string_view value)
{
    set_value(m_force_auth, value);
}

#endif
