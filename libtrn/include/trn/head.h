/* trn/head.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_HEAD_H
#define TRN_HEAD_H

#include <config/typedef.h>
#include <trn/enum-flags.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct Article;

// types of header lines
// (These must stay in alphabetic order at least in the first letter.
// Within each letter it helps to arrange in increasing likelihood.)
//
enum HeaderLineType
{
    PAST_HEADER = 0,        // body
    SHOWN_LINE,             // unrecognized but shown
    HIDDEN_LINE,            // unrecognized but hidden
    CUSTOM_LINE,            // to isolate a custom line
    SOME_LINE,              // default for unrecognized
    HEAD_FIRST = SOME_LINE, // first header line
    AUTHOR_LINE,            // Author
    BYTES_LINE,             // Bytes
    CONT_NAME_LINE,         // Content-Name
    CONT_DISP_LINE,         // Content-Disposition
    CONT_LEN_LINE,          // Content-Length
    CONT_XFER_LINE,         // Content-Transfer-Encoding
    CONT_TYPE_LINE,         // Content-Type
    DIST_LINE,              // distribution
    DATE_LINE,              // date
    EXPIR_LINE,             // expires
    FOLLOW_LINE,            // followup-to
    FROM_LINE,              // from
    IN_REPLY_LINE,          // in-reply-to
    KEYW_LINE,              // keywords
    LINES_LINE,             // lines
    MIME_VER_LINE,          // mime-version
    MSG_ID_LINE,            // message-id
    NEWSGROUPS_LINE,        // newsgroups
    PATH_LINE,              // path
    RVER_LINE,              // relay-version
    REPLY_LINE,             // reply-to
    REFS_LINE,              // references
    SUMMARY_LINE,           // summary
    SUBJ_LINE,              // subject
    XREF_LINE,              // xref
    HEAD_LAST,              // total # of headers
};

enum HeaderTypeFlags : std::uint8_t
{
    HT_NONE = 0x0,
    HT_HIDE = 0x01,      // hide this line
    HT_MAGIC = 0x02,     // do any special processing on this line
    HT_CACHED = 0x04,    // this information is cached article data
    HT_DEF_HIDE = 0x08,  // hidden by default
    HT_DEF_MAGIC = 0x10, // magic by default
    HT_MAGIC_OK = 0x20,  // magic even possible for line
};
DECLARE_FLAGS_ENUM(HeaderTypeFlags, std::uint8_t);

struct HeaderType
{
    std::string     name;    // header line identifier
    char            length;  // the header's string length
    HeaderTypeFlags flags;   // the header's flags
};

struct HeaderPosition
{
    ArticlePosition min_pos{}; // pointer to beginning of line in article
    ArticlePosition max_pos{}; // pointer to end of line in article
};

struct ParsedHeaderContext
{
    ArticleNum                            parsed_art{}; // the article number we've parsed
    HeaderLineType                        in_header{};  // are we decoding the header?
    std::string                           head_buf;
    Article                              *parsed_artp{}; // the article ptr we've parsed
    bool                                  first_one{};   // is this the 1st occurrence of this header line?
    bool                                  reading_nntp_header{};
    std::array<HeaderPosition, HEAD_LAST> positions{};

    void start_header(ArticleNum artnum);
    // Restart a parsed header's presentation without discarding its offsets.
    void restart_header(ArticleNum artnum);
    bool parse_line(std::string_view art_buf, int new_hide, int old_hide);

    // TODO: private
    void        end_line();
    bool        line_span(HeaderLineType which_line, char *&line, int &size);
    std::string current_line_text(HeaderLineType which_line);
};

extern ParsedHeaderContext g_parsed_header;

struct UserHeaderType
{
    std::string name;   // user-defined headers
    char        length; // the header's string length
    char        flags;  // the header's flags
};

struct HeaderRules
{
    void init();
    void clear();
    HeaderLineType set_line_type(std::string_view header_name);
    void           set(std::string_view s, HeaderTypeFlags flag, bool set_flag);

    // TODO: private
    void set_list(HeaderTypeFlags flag, HeaderTypeFlags def_flag, std::string_view prefix_list);
    std::string hidden_list();

    HeaderType                  type[HEAD_LAST]{};
    HeaderLineType              type_idx[26]{};
    std::vector<UserHeaderType> user_type;
    std::array<int, 26>         user_type_index{};
    int                         user_type_count{};
    int                         user_type_max{};
};

extern HeaderRules g_header_rules;

enum
{
    PREFETCH_SIZE = 5
};

void head_init();
void head_final();

HeaderLineType get_header_num(std::string_view header_name);
void           end_header();
bool           parse_header(ArticleNum art_num);
std::string    fetch_lines(ArticleNum art_num, HeaderLineType which_line);
void           prefetch_lines(ArticleNum art_num, HeaderLineType which_line);
std::string    prefetch_lines_copy(ArticleNum art_num, HeaderLineType which_line);
inline void    prefetch_subj(ArticleNum art_num)
{
    prefetch_lines(art_num, SUBJ_LINE);
}
inline std::string fetch_subj_copy(ArticleNum art_num)
{
    return prefetch_lines_copy(art_num, SUBJ_LINE);
}
inline void prefetch_xref(ArticleNum art_num)
{
    prefetch_lines(art_num, XREF_LINE);
}

#endif
