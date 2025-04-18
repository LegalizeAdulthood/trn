/* trn/datasrc.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_DATASRC_H
#define TRN_DATASRC_H

#include "nntp/nntpclient.h"
#include "trn/list.h"
#include "trn/enum-flags.h"
#include "trn/rt-ov.h"

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

enum
{
    DEFAULT_REFETCH_SECS = 4L * 60 * 60 // 4 hours
};

struct HashTable;
struct List;

struct SourceFile
{
    std::FILE  *fp;           // the file pointer to read the data
    HashTable  *hp;           // the hash table for the data
    List       *lp;           // the list used to store the data
    long        recent_cnt;   // # lines/bytes this file might be
    std::time_t last_fetch;    // when the data was last fetched
    std::time_t refetch_secs; // how long before we refetch this file
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
    char            *name;       // our user-friendly name
    char            *news_id;    // the active file name or host name
    SourceFile       act_sf;     // the active file's hashed contents
    char            *group_desc; // the newsgroup description file or tmp
    SourceFile       desc_sf;    // the group description's hashed contents
    char            *extra_name; // local active.times or server's active file
    NNTPLink         nntp_link;
    char            *spool_dir;
    char            *over_dir;
    char            *over_fmt;
    char            *auth_user;
    char            *auth_pass;
    long             last_new_group; // time of last newgroup check
    std::FILE       *ov_in;          // the overview's file handle
    std::time_t      ov_opened;      // time overview file was opened
    OverviewFieldNum field_num[OV_MAX_FIELDS];
    FieldFlags       field_flags[OV_MAX_FIELDS];
    DataSourceFlags  flags;
};

enum
{
    LENGTH_HACK = 5, // Don't bother comparing strings with lengths
                     // that differ by more than this.
    MAX_NG = 9,      // Maximum number of groups to offer.

    DATASRC_ALARM_SECS = (5 * 60)
};

extern List       *g_data_source_list; // a list of all data sources
extern DataSource *g_data_source;      // the current data source
extern int         g_data_source_cnt;  //
extern char       *g_trn_access_mem;   //
extern std::string g_nntp_auth_file;   //
extern std::time_t g_def_refetch_secs; // -z

void        data_source_init();
void        data_source_finalize();
char       *read_data_sources(const char *filename);
DataSource *get_data_source(const char *name);
bool        open_data_source(DataSource *dp);
void        set_data_source(DataSource *dp);
void        check_data_sources();
void        close_data_source(DataSource *dp);
bool        active_file_hash(DataSource *dp);
bool        find_active_group(DataSource *dp, char *outbuf, const char *nam, int len, ArticleNum high);
const char *find_group_desc(DataSource *dp, const char *group_name);
int         source_file_open(SourceFile *sfp, const char *filename, const char *fetch_cmd, const char *server);
char       *source_file_append(SourceFile *sfp, char *bp, int key_len);
void        source_file_end_append(SourceFile *sfp, const char *filename);
void        source_file_close(SourceFile *sfp);
int         find_close_match();

inline NNTPFlags data_source_nntp_flags(const DataSource *dp)
{
    return dp == g_data_source ? g_nntp_link.flags : dp->nntp_link.flags;
}

inline DataSource *data_source_ptr(int n)
{
    return (DataSource *) list_get_item(g_data_source_list, n);
}

inline DataSource *data_source_first()
{
    return data_source_ptr(0);
}

inline DataSource *data_source_next(DataSource *p)
{
    return (DataSource *) next_list_item(g_data_source_list, (char *) p);
}

#endif
