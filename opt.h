/* opt.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef OPT_H
#define OPT_H

#define YESorNO(v) g_yesorno[(int)(v)]

/* Display Options */
enum display_option
{
    OI_TERSE_OUTPUT = 2,
    OI_PAGER_LINE_MARKING,
    OI_ERASE_SCREEN,
    OI_ERASE_EACH_LINE,
    OI_MUCK_UP_CLEAR,
    OI_BKGND_SPINNER,
    OI_CHARSET,
    OI_FILTER_CONTROL_CHARACTERS,

    /* Selector Options */
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

    /* Newsreading Options */
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
    OI_BKGND_THREADING,
    OI_SCANMODE_COUNT,
    OI_HEADER_MAGIC,
    OI_HEADER_HIDING,

    /* Posting Options */
    OI_CITED_TEXT_STRING = OI_HEADER_HIDING + 2,

    /* Save Options */
    OI_SAVE_DIR = OI_CITED_TEXT_STRING + 2,
    OI_AUTO_SAVE_NAME,
    OI_SAVEFILE_TYPE,

    /* Mouse Options */
    OI_USE_MOUSE = OI_SAVEFILE_TYPE + 2,
    OI_MOUSE_MODES,
    OI_UNIV_SEL_BTNS,
    OI_NEWSRC_SEL_BTNS,
    OI_ADD_SEL_BTNS,
    OI_NEWSGROUP_SEL_BTNS,
    OI_NEWS_SEL_BTNS,
    OI_OPTION_SEL_BTNS,
    OI_ART_PAGER_BTNS,

    /* MIME Options */
    OI_MULTIPART_SEPARATOR = OI_ART_PAGER_BTNS + 2,
    OI_AUTO_VIEW_INLINE,

    /* Misc Options */
    OI_NEWGROUP_CHECK = OI_AUTO_VIEW_INLINE + 2,
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

    /* CAA: There are no current plans for scan modes other than article.
     *      The general-scan options might as well be displayed in the same
     *      section as the article-scan options.
     */
    OI_SCANA_FOLLOW = OI_TRN_LAST + 2,
    OI_SCANA_FOLD,
    OI_SCANA_UNZOOMFOLD,
    OI_SCANA_MARKSTAY,
    OI_SCAN_VI,
    OI_SCAN_ITEMNUM,
    OI_SCANA_DISPANUM,
    OI_SCANA_DISPAUTHOR,
    OI_SCANA_DISPSCORE,
    OI_SCANA_DISPSUBCNT,
    OI_SCANA_DISPSUBJ,
    OI_SCANA_DISPSUMMARY,
    OI_SCANA_DISPKEYW,
    OI_SCAN_LAST = OI_SCANA_DISPKEYW,

    OI_SC_VERBOSE = OI_SCAN_LAST + 2,
    OI_SCORE_LAST = OI_SC_VERBOSE,
};

#define OF_SEL		0x0001
#define OF_INCLUDED	0x0010

extern COMPEX g_optcompex;
extern char* g_ini_file;
extern char* g_yesorno[2];
extern INI_WORDS g_options_ini[];
extern char **g_option_def_vals;
extern char **g_option_saved_vals;
extern char *g_option_flags;
extern int g_sel_page_op;
extern int g_sel_next_op;

void opt_init(int argc, char *argv[], char **tcbufptr);
void opt_file(char *filename, char **tcbufptr, bool bleat);
void set_options(char **vals);
void set_option(int num, char *s);
void save_options(const char *filename);
char *option_value(int num);
void set_header(char *s, int flag, bool setit);
char *quote_string(char *val);
void cwd_check();

#endif
