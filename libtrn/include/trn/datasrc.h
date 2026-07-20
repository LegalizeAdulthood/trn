/* trn/datasrc.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_DATASRC_H
#define TRN_DATASRC_H

#include <nntp/nntpclient.h>
#include <trn/enum-flags.h>
#include <trn/rt-ov.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum
{
    DEFAULT_REFETCH_SECS = 4L * 60 * 60 // 4 hours
};

struct HashTable;

struct SourceFile
{
    int              open(const std::filesystem::path &filename, std::string_view fetch_cmd, std::string_view server);
    std::string_view append(std::string_view line, int key_len);
    void             end_append(const std::filesystem::path &filename);
    void             close();

    std::FILE               *m_fp; // the file pointer to read the data
    HashTable               *m_hp; // the hash table for the data
    std::vector<std::string> m_lines;
    std::vector<long>        m_line_positions;
    long                     m_recent_cnt;   // # lines/bytes this file might be
    std::time_t              m_last_fetch;   // when the data was last fetched
    std::time_t              m_refetch_secs; // how long before we refetch this file
};

enum DataSourceFlags : std::uint16_t
{
    DF_NONE = 0,
    DF_TRY_OVERVIEW = 0x0001,
    DF_ADD_OK = 0x0004,
    DF_DEFAULT = 0x0008,
    DF_OPEN = 0x0010,
    DF_ACTIVE = 0x0020,
    DF_UNAVAILABLE = 0x0040,
    DF_REMOTE = 0x0080,
    DF_TMP_ACTIVE_FILE = 0x0100,
    DF_TMP_GROUP_DESC = 0x0200,
    DF_USE_LIST_ACTIVE = 0x0400,
    DF_XHDR_BROKEN = 0x0800,
    DF_NO_XGTITLE = 0x1000,
    DF_NO_LIST_GROUP = 0x2000,
    DF_NO_XREFS = 0x4000
};
DECLARE_FLAGS_ENUM(DataSourceFlags, std::uint16_t)

enum FieldFlags : std::uint8_t
{
    FF_NONE = 0,
    FF_HAS_FIELD = 0x01,
    FF_CHECK_FOR_FIELD = 0x02,
    FF_HAS_HDR = 0x04,
    FF_CHECK_FOR_HEADER = 0x08,
    FF_FILTER_SEND = 0x10
};
DECLARE_FLAGS_ENUM(FieldFlags, std::uint8_t)

struct DataSource
{
    bool             open();
    void             close();
    bool             active_file_hash();
    std::string      find_active_group(std::string_view name, ArticleNum high);
    std::string_view find_group_desc(std::string_view group_name);
    inline NNTPFlags nntp_flags() const;
    void             nntp_server_died();

    std::string      m_name;       // our user-friendly name
    std::string      m_news_id;    // the active file name or host name
    SourceFile       m_act_sf;     // the active file's hashed contents
    std::string      m_group_desc; // the newsgroup description file or tmp
    SourceFile       m_desc_sf;    // the group description's hashed contents
    std::string      m_extra_name; // local active.times or server's active file
    NNTPLink         m_nntp_link;
    std::string      m_spool_dir;
    std::string      m_over_dir;
    std::string      m_over_fmt;
    std::string      m_auth_user;
    std::string      m_auth_pass;
    long             m_last_new_group; // time of last newgroup check
    std::FILE       *m_ov_in;          // the overview's file handle
    std::time_t      m_ov_opened;      // time overview file was opened
    OverviewFieldNum m_field_num[OV_MAX_FIELDS];
    FieldFlags       m_field_flags[OV_MAX_FIELDS];
    DataSourceFlags  m_flags;
};

enum
{
    LENGTH_HACK = 5, // Don't bother comparing strings with lengths
                     // that differ by more than this.
    MAX_NG = 9,      // Maximum number of groups to offer.

    DATASRC_ALARM_SECS = (5 * 60)
};

extern std::vector<DataSource> g_data_sources;     // all data sources
extern DataSource             *g_data_source;      // the current data source
extern std::string             g_trn_access_text;  //
extern std::string             g_nntp_auth_file;   //
extern std::time_t             g_def_refetch_secs; // -z

void        data_source_init();
void        data_source_finalize();
DataSource *get_data_source(std::string_view name);
void        set_data_source(DataSource *dp);
void        check_data_sources();
int         find_close_match();

inline NNTPFlags DataSource::nntp_flags() const
{
    return this == g_data_source ? g_nntp_link.flags : m_nntp_link.flags;
}

inline DataSource *data_source_ptr(std::size_t n)
{
    return n < g_data_sources.size() ? &g_data_sources[n] : nullptr;
}

inline DataSource *data_source_first()
{
    return data_source_ptr(0);
}

inline DataSource *data_source_next(DataSource *p)
{
    const std::size_t n = static_cast<std::size_t>(p - g_data_sources.data()) + 1;
    return data_source_ptr(n);
}

#endif
