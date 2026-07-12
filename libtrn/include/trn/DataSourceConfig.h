/* trn/DataSourceConfig.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_DATA_SOURCE_CONFIG_H
#define TRN_DATA_SOURCE_CONFIG_H

class IniSchema;
class IniSectionValues;

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

    const char *nntp_server() const;
    const char *active_file() const;
    const char *active_file_refetch() const;
    const char *spool_dir() const;
    const char *thread_dir() const;
    const char *overview_dir() const;
    const char *active_times() const;
    const char *group_desc() const;
    const char *group_desc_refetch() const;
    const char *auth_user() const;
    const char *auth_password() const;
    const char *auth_command() const;
    const char *xhdr_broken() const;
    const char *xrefs() const;
    const char *overview_format_file() const;
    const char *force_auth() const;

    void set_nntp_server(const char *value);
    void set_active_file(const char *value);
    void set_active_file_refetch(const char *value);
    void set_spool_dir(const char *value);
    void set_thread_dir(const char *value);
    void set_overview_dir(const char *value);
    void set_active_times(const char *value);
    void set_group_desc(const char *value);
    void set_group_desc_refetch(const char *value);
    void set_auth_user(const char *value);
    void set_auth_password(const char *value);
    void set_auth_command(const char *value);
    void set_xhdr_broken(const char *value);
    void set_xrefs(const char *value);
    void set_overview_format_file(const char *value);
    void set_force_auth(const char *value);

private:
    const char *m_nntp_server{};
    const char *m_active_file{};
    const char *m_active_file_refetch{};
    const char *m_spool_dir{};
    const char *m_thread_dir{};
    const char *m_overview_dir{};
    const char *m_active_times{};
    const char *m_group_desc{};
    const char *m_group_desc_refetch{};
    const char *m_auth_user{};
    const char *m_auth_password{};
    const char *m_auth_command{};
    const char *m_xhdr_broken{};
    const char *m_xrefs{};
    const char *m_overview_format_file{};
    const char *m_force_auth{};
};

inline const char *DataSourceConfig::nntp_server() const
{
    return m_nntp_server;
}

inline const char *DataSourceConfig::active_file() const
{
    return m_active_file;
}

inline const char *DataSourceConfig::active_file_refetch() const
{
    return m_active_file_refetch;
}

inline const char *DataSourceConfig::spool_dir() const
{
    return m_spool_dir;
}

inline const char *DataSourceConfig::thread_dir() const
{
    return m_thread_dir;
}

inline const char *DataSourceConfig::overview_dir() const
{
    return m_overview_dir;
}

inline const char *DataSourceConfig::active_times() const
{
    return m_active_times;
}

inline const char *DataSourceConfig::group_desc() const
{
    return m_group_desc;
}

inline const char *DataSourceConfig::group_desc_refetch() const
{
    return m_group_desc_refetch;
}

inline const char *DataSourceConfig::auth_user() const
{
    return m_auth_user;
}

inline const char *DataSourceConfig::auth_password() const
{
    return m_auth_password;
}

inline const char *DataSourceConfig::auth_command() const
{
    return m_auth_command;
}

inline const char *DataSourceConfig::xhdr_broken() const
{
    return m_xhdr_broken;
}

inline const char *DataSourceConfig::xrefs() const
{
    return m_xrefs;
}

inline const char *DataSourceConfig::overview_format_file() const
{
    return m_overview_format_file;
}

inline const char *DataSourceConfig::force_auth() const
{
    return m_force_auth;
}

inline void DataSourceConfig::set_nntp_server(const char *value)
{
    m_nntp_server = value;
}

inline void DataSourceConfig::set_active_file(const char *value)
{
    m_active_file = value;
}

inline void DataSourceConfig::set_active_file_refetch(const char *value)
{
    m_active_file_refetch = value;
}

inline void DataSourceConfig::set_spool_dir(const char *value)
{
    m_spool_dir = value;
}

inline void DataSourceConfig::set_thread_dir(const char *value)
{
    m_thread_dir = value;
}

inline void DataSourceConfig::set_overview_dir(const char *value)
{
    m_overview_dir = value;
}

inline void DataSourceConfig::set_active_times(const char *value)
{
    m_active_times = value;
}

inline void DataSourceConfig::set_group_desc(const char *value)
{
    m_group_desc = value;
}

inline void DataSourceConfig::set_group_desc_refetch(const char *value)
{
    m_group_desc_refetch = value;
}

inline void DataSourceConfig::set_auth_user(const char *value)
{
    m_auth_user = value;
}

inline void DataSourceConfig::set_auth_password(const char *value)
{
    m_auth_password = value;
}

inline void DataSourceConfig::set_auth_command(const char *value)
{
    m_auth_command = value;
}

inline void DataSourceConfig::set_xhdr_broken(const char *value)
{
    m_xhdr_broken = value;
}

inline void DataSourceConfig::set_xrefs(const char *value)
{
    m_xrefs = value;
}

inline void DataSourceConfig::set_overview_format_file(const char *value)
{
    m_overview_format_file = value;
}

inline void DataSourceConfig::set_force_auth(const char *value)
{
    m_force_auth = value;
}

#endif
