/* opt.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char* ini_file;
EXT char* yesorno[2]
#ifdef DOINIT
 = {"no", "yes"};
#else
 ;
#endif

#define YESorNO(v) yesorno[(int)(v)]

/* Display Options */
#define OI_TERSE_OUTPUT			2
#define OI_PAGER_LINE_MARKING		(OI_TERSE_OUTPUT+1)
#define OI_ERASE_SCREEN			(OI_PAGER_LINE_MARKING+1)
#define OI_ERASE_EACH_LINE		(OI_ERASE_SCREEN+1)
#define OI_MUCK_UP_CLEAR		(OI_ERASE_EACH_LINE+1)
#define OI_BKGND_SPINNER		(OI_MUCK_UP_CLEAR+1)
#define OI_CHARSET			(OI_BKGND_SPINNER+1)
#define OI_FILTER_CONTROL_CHARACTERS	(OI_CHARSET+1)

/* Selector Options */
#define OI_USE_UNIV_SEL			(OI_FILTER_CONTROL_CHARACTERS+2)
#define OI_UNIV_SEL_ORDER		(OI_USE_UNIV_SEL+1)
#define OI_UNIV_FOLLOW			(OI_UNIV_SEL_ORDER+1)
#define OI_UNIV_SEL_CMDS		(OI_UNIV_FOLLOW+1)
#define OI_USE_NEWSRC_SEL		(OI_UNIV_SEL_CMDS+1)
#define OI_NEWSRC_SEL_CMDS		(OI_USE_NEWSRC_SEL+1)
#define OI_USE_ADD_SEL			(OI_NEWSRC_SEL_CMDS+1)
#define OI_ADD_SEL_CMDS			(OI_USE_ADD_SEL+1)
#define OI_USE_NEWSGROUP_SEL		(OI_ADD_SEL_CMDS+1)
#define OI_NEWSGROUP_SEL_ORDER		(OI_USE_NEWSGROUP_SEL+1)
#define OI_NEWSGROUP_SEL_CMDS		(OI_NEWSGROUP_SEL_ORDER+1)
#define OI_NEWSGROUP_SEL_STYLES		(OI_NEWSGROUP_SEL_CMDS+1)
#define OI_USE_NEWS_SEL			(OI_NEWSGROUP_SEL_STYLES+1)
#define OI_NEWS_SEL_MODE		(OI_USE_NEWS_SEL+1)
#define OI_NEWS_SEL_ORDER		(OI_NEWS_SEL_MODE+1)
#define OI_NEWS_SEL_CMDS		(OI_NEWS_SEL_ORDER+1)
#define OI_NEWS_SEL_STYLES		(OI_NEWS_SEL_CMDS+1)
#define OI_OPTION_SEL_CMDS		(OI_NEWS_SEL_STYLES+1)
#define OI_USE_SEL_NUM			(OI_OPTION_SEL_CMDS+1)
#define OI_SEL_NUM_GOTO			(OI_USE_SEL_NUM+1)

/* Newsreading Options */
#define OI_USE_THREADS			(OI_SEL_NUM_GOTO+2)
#define OI_SELECT_MY_POSTS		(OI_USE_THREADS+1)
#define OI_INITIAL_ARTICLE_LINES	(OI_SELECT_MY_POSTS+1)
#define OI_ARTICLE_TREE_LINES		(OI_INITIAL_ARTICLE_LINES+1)
#define OI_WORD_WRAP_MARGIN		(OI_ARTICLE_TREE_LINES+1)
#define OI_AUTO_GROW_GROUPS		(OI_WORD_WRAP_MARGIN+1)
#define OI_COMPRESS_SUBJECTS		(OI_AUTO_GROW_GROUPS+1)
#define OI_JOIN_SUBJECT_LINES		(OI_COMPRESS_SUBJECTS+1)
#define OI_GOTO_LINE_NUM		(OI_JOIN_SUBJECT_LINES+1)
#define OI_IGNORE_THRU_ON_SELECT	(OI_GOTO_LINE_NUM+1)
#define OI_READ_BREADTH_FIRST		(OI_IGNORE_THRU_ON_SELECT+1)
#define OI_BKGND_THREADING		(OI_READ_BREADTH_FIRST+1)
#define OI_SCANMODE_COUNT		(OI_BKGND_THREADING+1)
#define OI_HEADER_MAGIC			(OI_SCANMODE_COUNT+1)
#define OI_HEADER_HIDING		(OI_HEADER_MAGIC+1)

/* Posting Options */
#define OI_CITED_TEXT_STRING		(OI_HEADER_HIDING+2)

/* Save Options */
#define OI_SAVE_DIR			(OI_CITED_TEXT_STRING+2)
#define OI_AUTO_SAVE_NAME		(OI_SAVE_DIR+1)
#define OI_SAVEFILE_TYPE		(OI_AUTO_SAVE_NAME+1)

/* Mouse Options */
#define OI_USE_MOUSE			(OI_SAVEFILE_TYPE+2)
#define OI_MOUSE_MODES			(OI_USE_MOUSE+1)
#define OI_UNIV_SEL_BTNS		(OI_MOUSE_MODES+1)
#define OI_NEWSRC_SEL_BTNS		(OI_UNIV_SEL_BTNS+1)
#define OI_ADD_SEL_BTNS			(OI_NEWSRC_SEL_BTNS+1)
#define OI_NEWSGROUP_SEL_BTNS		(OI_ADD_SEL_BTNS+1)
#define OI_NEWS_SEL_BTNS		(OI_NEWSGROUP_SEL_BTNS+1)
#define OI_OPTION_SEL_BTNS		(OI_NEWS_SEL_BTNS+1)
#define OI_ART_PAGER_BTNS		(OI_OPTION_SEL_BTNS+1)

/* MIME Options */
#define OI_MULTIPART_SEPARATOR		(OI_ART_PAGER_BTNS+2)
#define OI_AUTO_VIEW_INLINE		(OI_MULTIPART_SEPARATOR+1)

/* Misc Options */
#define OI_NEWGROUP_CHECK		(OI_AUTO_VIEW_INLINE+2)
#define OI_RESTRICTION_INCLUDES_EMPTIES	(OI_NEWGROUP_CHECK+1)
#define OI_APPEND_UNSUBSCRIBED_GROUPS	(OI_RESTRICTION_INCLUDES_EMPTIES+1)
#define OI_INITIAL_GROUP_LIST		(OI_APPEND_UNSUBSCRIBED_GROUPS+1)
#define OI_RESTART_AT_LAST_GROUP	(OI_INITIAL_GROUP_LIST+1)
#define OI_EAT_TYPEAHEAD		(OI_RESTART_AT_LAST_GROUP+1)
#define OI_VERIFY_INPUT			(OI_EAT_TYPEAHEAD+1)
#define OI_FUZZY_NEWSGROUP_NAMES	(OI_VERIFY_INPUT+1)
#define OI_AUTO_ARROW_MACROS		(OI_FUZZY_NEWSGROUP_NAMES+1)
#define OI_CHECKPOINT_NEWSRC_FREQUENCY	(OI_AUTO_ARROW_MACROS+1)
#define OI_DEFAULT_REFETCH_TIME		(OI_CHECKPOINT_NEWSRC_FREQUENCY+1)
#define OI_NOVICE_DELAYS		(OI_DEFAULT_REFETCH_TIME+1)
#define OI_OLD_MTHREADS_DATABASE	(OI_NOVICE_DELAYS+1)

#define OI_TRN_LAST			(OI_OLD_MTHREADS_DATABASE)

#ifdef SCAN_ART
/* CAA: There are no current plans for scan modes other than SCAN_ART.
 *      The general-scan options might as well be displayed in the same
 *      section as the article-scan options.
 */
# define OI_SCANA_FOLLOW		(OI_TRN_LAST+2)
# define OI_SCANA_FOLD			(OI_SCANA_FOLLOW+1)
# define OI_SCANA_UNZOOMFOLD		(OI_SCANA_FOLD+1)
# define OI_SCANA_MARKSTAY		(OI_SCANA_UNZOOMFOLD+1)
# define OI_SCAN_VI			(OI_SCANA_MARKSTAY+1)
# define OI_SCAN_ITEMNUM		(OI_SCAN_VI+1)
# define OI_SCANA_DISPANUM		(OI_SCAN_ITEMNUM+1)
# define OI_SCANA_DISPAUTHOR		(OI_SCANA_DISPANUM+1)
# define OI_SCANA_DISPSCORE		(OI_SCANA_DISPAUTHOR+1)
# define OI_SCANA_DISPSUBCNT		(OI_SCANA_DISPSCORE+1)
# define OI_SCANA_DISPSUBJ		(OI_SCANA_DISPSUBCNT+1)
# define OI_SCANA_DISPSUMMARY		(OI_SCANA_DISPSUBJ+1)
# define OI_SCANA_DISPKEYW		(OI_SCANA_DISPSUMMARY+1)
# define OI_SCAN_LAST			(OI_SCANA_DISPKEYW)
#else /* !SCAN_ART */
# define OI_SCAN_LAST			(OI_TRN_LAST)
#endif /* SCAN_ART */

#ifdef SCORE
# define OI_SC_VERBOSE			(OI_SCAN_LAST+2)
# define OI_SCORE_LAST			(OI_SC_VERBOSE)
#else
# define OI_SCORE_LAST			(OI_SCAN_LAST)
#endif

extern INI_WORDS options_ini[];
EXT char** option_def_vals;
EXT char** option_saved_vals;
EXT char* option_flags;

#define OF_SEL		0x0001
#define OF_INCLUDED	0x0010

EXT int sel_page_op;
EXT int sel_next_op;

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

void opt_init _((int,char**,char**));
void opt_file(char *filename, char **tcbufptr, bool bleat);
void set_options _((char**));
void set_option _((int,char*));
void save_options _((char*));
char* option_value _((int));
void set_header(char *s, int flag, bool setit);
char* quote_string _((char*));
void cwd_check _((void));
