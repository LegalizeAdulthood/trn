/* trn/opt.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_OPT_H
#define TRN_OPT_H

#include "trn/enum-flags.h"
#include "trn/head.h"
#include "trn/search.h"
#include "trn/util.h"

#include <string>

// Display Options
enum OptionIndex
{
    OI_NONE = -1,

    OI_TERSE_OUTPUT = 2,
    OI_PAGER_LINE_MARKING,
    OI_ERASE_SCREEN,
    OI_ERASE_EACH_LINE,
    OI_MUCK_UP_CLEAR,
    OI_BACKGROUND_SPINNER,
    OI_CHARSET,
    OI_FILTER_CONTROL_CHARACTERS,

    // Selector Options
    OI_USE_UNIV_SEL = OI_FILTER_CONTROL_CHARACTERS + 2,
    OI_UNIV_SEL_ORDER,
    OI_UNIV_FOLLOW,
    OI_UNIV_SEL_CMDS,
    OI_USE_NEWSRC_SEL,
    OI_NEWSRC_SEL_CMDS,
    OI_USE_ADD_SEL,
    OI_ADD_SEL_CMDS,
    OI_USE_NEWSGROUP_SEL,
    OI_NEWSGROUP_SEL_ORDER,
    OI_NEWSGROUP_SEL_CMDS,
    OI_NEWSGROUP_SEL_STYLES,
    OI_USE_NEWS_SEL,
    OI_NEWS_SEL_MODE,
    OI_NEWS_SEL_ORDER,
    OI_NEWS_SEL_CMDS,
    OI_NEWS_SEL_STYLES,
    OI_OPTION_SEL_CMDS,
    OI_USE_SEL_NUM,
    OI_SEL_NUM_GOTO,

    // News reading Options
    OI_USE_THREADS = OI_SEL_NUM_GOTO + 2,
    OI_SELECT_MY_POSTS,
    OI_INITIAL_ARTICLE_LINES,
    OI_ARTICLE_TREE_LINES,
    OI_WORD_WRAP_MARGIN,
    OI_AUTO_GROW_GROUPS,
    OI_COMPRESS_SUBJECTS,
    OI_JOIN_SUBJECT_LINES,
    OI_GOTO_LINE_NUM,
    OI_IGNORE_THRU_ON_SELECT,
    OI_READ_BREADTH_FIRST,
    OI_BACKGROUND_THREADING,
    OI_SCAN_MODE_COUNT,
    OI_HEADER_MAGIC,
    OI_HEADER_HIDING,

    // Posting Options
    OI_CITED_TEXT_STRING = OI_HEADER_HIDING + 2,

    // Save Options
    OI_SAVE_DIR = OI_CITED_TEXT_STRING + 2,
    OI_AUTO_SAVE_NAME,
    OI_SAVE_FILE_TYPE,

    // Mouse Options
    OI_USE_MOUSE = OI_SAVE_FILE_TYPE + 2,
    OI_MOUSE_MODES,
    OI_UNIV_SEL_BTNS,
    OI_NEWSRC_SEL_BTNS,
    OI_ADD_SEL_BTNS,
    OI_NEWSGROUP_SEL_BTNS,
    OI_NEWS_SEL_BTNS,
    OI_OPTION_SEL_BTNS,
    OI_ART_PAGER_BTNS,

    // MIME Options
    OI_MULTIPART_SEPARATOR = OI_ART_PAGER_BTNS + 2,
    OI_AUTO_VIEW_INLINE,

    // Misc Options
    OI_NEW_GROUP_CHECK = OI_AUTO_VIEW_INLINE + 2,
    OI_RESTRICTION_INCLUDES_EMPTIES,
    OI_APPEND_UNSUBSCRIBED_GROUPS,
    OI_INITIAL_GROUP_LIST,
    OI_RESTART_AT_LAST_GROUP,
    OI_EAT_TYPEAHEAD,
    OI_VERIFY_INPUT,
    OI_FUZZY_NEWSGROUP_NAMES,
    OI_AUTO_ARROW_MACROS,
    OI_CHECKPOINT_NEWSRC_FREQUENCY,
    OI_DEFAULT_REFETCH_TIME,
    OI_NOVICE_DELAYS,
    OI_OLD_MTHREADS_DATABASE,

    OI_TRN_LAST = OI_OLD_MTHREADS_DATABASE,

    // There are no current plans for scan modes other than article.
    // The general-scan options might as well be displayed in the same
    // section as the article-scan options.
    //
    OI_SCAN_ART_FOLLOW = OI_TRN_LAST + 2,
    OI_SCAN_ART_FOLD,
    OI_SCAN_ART_UNZOOM_FOLD,
    OI_SCAN_ART_MARK_STAY,
    OI_SCAN_VI,
    OI_SCAN_ITEM_NUM,
    OI_SCAN_ART_DISP_ART_NUM,
    OI_SCAN_ART_DISP_AUTHOR,
    OI_SCAN_ART_DISP_SCORE,
    OI_SCAN_ART_DISP_SUB_COUNT,
    OI_SCAN_ART_DISP_SUBJ,
    OI_SCAN_ART_DISP_SUMMARY,
    OI_SCAN_ART_DISP_KEYW,
    OI_SCAN_LAST = OI_SCAN_ART_DISP_KEYW,

    OI_SC_VERBOSE = OI_SCAN_LAST + 2,
    OI_SCORE_LAST = OI_SC_VERBOSE,
};

enum OptionFlags : char
{
    OF_NONE = 0x00,
    OF_SEL = 0x01,
    OF_INCLUDED = 0x10
};
DECLARE_FLAGS_ENUM(OptionFlags, char);

extern CompiledRegex g_opt_compex;
extern std::string   g_ini_file;
extern IniWords      g_options_ini[];
extern char        **g_option_def_vals;
extern char        **g_option_saved_vals;
extern OptionFlags  *g_option_flags;
extern int           g_sel_page_op;

void        opt_init(int argc, char *argv[], char **tcbufptr);
void        opt_final();
void        opt_file(const char *filename, char **tcbufptr, bool bleat);
void        set_options(char **vals);
void        set_option(OptionIndex num, const char *s);
void        save_options(const char *filename);
const char *option_value(OptionIndex num);
void        set_header(const char *s, HeaderTypeFlags flag, bool setit);
const char *quote_string(const char *val);
void        cwd_check();

inline const char *yes_or_no(bool v)
{
    return v ? "yes" : "no";
}

#endif
