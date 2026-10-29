/* trn/ng.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NG_H
#define TRN_NG_H

#include <config/common.h>
#include <config/config2.h>
#include <config/typedef.h>

#include <functional>
#include <optional>
#include <string>

struct Article;

// A number can name a prospective, missing, or past-end article. A
// zero-number temporary article can still have a non-null borrowed pointer.
struct ArticleLocation
{
    void next(bool sel_flag, bool rereading);
    void prev(bool sel_flag, bool rereading);
    void first();
    bool next_with_subject();
    bool prev_with_subject();

    ArticleNum num{};
    Article   *art{};
};

inline bool operator==(const ArticleLocation &lhs, const ArticleLocation &rhs)
{
    return lhs.num == rhs.num && lhs.art == rhs.art;
}

inline bool operator!=(const ArticleLocation &lhs, const ArticleLocation &rhs)
{
    return !(lhs == rhs);
}

struct ReadingNavigation
{
    void commit_current();
    void restore_current()
    {
        target = current;
    }

    ArticleLocation target;  // current or prospective article
    ArticleLocation current; // article currently being read
    ArticleLocation recent;  // previous article for the '-' command
};

extern ReadingNavigation g_nav;
extern int        g_check_count; // how many articles have we read in the current newsgroup since the last checkpoint?
extern int        g_do_check_when; // how often to do checkpoint
extern std::optional<std::string> g_subj_line; // what format to use for '='
#ifdef MAIL_CALL
extern int g_mail_count;           // check for mail when 0 mod 5
#endif
extern std::string g_mail_call;
extern bool        g_force_last;      // ought we show "End of newsgroup"?
extern bool        g_force_grow;      // do we want to recalculate size of newsgroup, e.g. after posting?
extern int         g_scan_on;         // -S
extern bool        g_use_threads;     // -x
extern bool        g_unsafe_rc_saves; // -U
extern std::string g_default_cmd;     // 1st char is default command

enum DoNewsgroupResult
{
    NG_ERROR = -1,
    NG_NORM = 0,
    NG_ASK = 1,
    NG_MINUS = 2,
    NG_SEL_PRIOR = 3,
    NG_SEL_NEXT = 4,
    NG_NO_SERVER = 5,
    NG_NEXT = 6,
    NG_GO_ARTICLE = 7
};

extern DoNewsgroupResult g_newsgroup_exit_code;

enum ArticleSwitchResult
{
    AS_NORM = 0,
    AS_INP = 1,
    AS_ASK = 2,
    AS_CLEAN = 3,
    AS_SA = 4,
    AS_QUITNOW = 5,
    AS_SV = 6
};

void                ng_init();
DoNewsgroupResult   do_newsgroup(std::optional<std::string> start_command);
ArticleSwitchResult art_switch(std::string command, const std::function<void(ArticleNum)> &search_progress = {},
                               const std::function<char(char_int)> &select_articles = {});
#ifdef MAIL_CALL
void set_mail(bool force);
#endif
void set_default_cmd();
char ask_catchup();
bool output_subject(Article &article, int flag);
char ask_memorize(char_int ch);

#endif
