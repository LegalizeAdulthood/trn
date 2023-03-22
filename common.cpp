/* common.cpp
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#include "common.h"

/* GLOBAL THINGS */

/* file statistics area */
struct stat filestat;

/* various things of type char */

char msg[CBUFLEN];     /* general purpose message buffer */
char buf[LBUFLEN + 1]; /* general purpose line buffer */
char cmd_buf[CBUFLEN]; /* buffer for formatting system commands */
char *indstr{">"};     /* indent for old article embedded in followup */

char *cwd{nullptr};     /* current working directory */
char *dfltcmd{nullptr}; /* 1st char is default command */

/* switches */

int scanon{0}; /* -S */

bool use_threads{THREAD_INIT}; /* -x */
int max_tree_lines{6};

char UnivSelCmds[3]{"Z>"};
char NewsrcSelCmds[3]{"Z>"};
char AddSelCmds[3]{"Z>"};
char NewsgroupSelCmds[3]{"Z>"};
char NewsSelCmds[3]{"Z>"};
char OptionSelCmds[3]{"Z>"};

int UnivSelBtnCnt;
int NewsrcSelBtnCnt;
int AddSelBtnCnt;
int NewsgroupSelBtnCnt;
int NewsSelBtnCnt;
int OptionSelBtnCnt;
int ArtPagerBtnCnt;

char *UnivSelBtns{nullptr};
char *NewsrcSelBtns{nullptr};
char *AddSelBtns{nullptr};
char *NewsgroupSelBtns{nullptr};
char *NewsSelBtns{nullptr};
char *OptionSelBtns{nullptr};
char *ArtPagerBtns{nullptr};

bool dont_filter_control{false}; /* -j */
int join_subject_len{0};         /* -J */
bool kill_thru_kludge{true};     /* -k */
int keep_the_group_static{0};    /* -K */
bool mbox_always{false};         /* -M */
bool norm_always{false};         /* -N */
bool thread_always{false};       /* -a */
int auto_arrow_macros{2};        /* -A */
bool breadth_first{false};       /* -b */
bool bkgnd_spinner{false};       /* -B */
bool novice_delays{true};        /* +f */
int olden_days{false};           /* -o */
char auto_select_postings{0};    /* -p */
bool checkflag{false};           /* -c */
char *savedir{nullptr};          /* -d */
bool suppress_cn{false};         /* -s */
int countdown{5};                /* how many lines to list before invoking -s */
bool muck_up_clear{false};       /* -loco */
bool erase_screen{false};        /* -e */
bool can_home{false};
bool erase_each_line{false};   /* fancy -e */
int findlast{0};               /* -r */
bool allow_typeahead{false};   /* -T */
bool fuzzyGet{false};          /* -G */
bool verbose{true};            /* +t */
bool unbroken_subjects{false}; /* -u */
bool unsafe_rc_saves{false};   /* -U */
bool verify{false};            /* -v */
bool quickstart{false};        /* -q */

time_t defRefetchSecs{DEFAULT_REFETCH_SECS}; /* -z */

int word_wrap_offset{8}; /* right-hand column size (0 is off) */

int marking{NOMARKING}; /* -m */
int marking_areas{HALFPAGE_MARKING};

ART_LINE initlines{0}; /* -i */

#ifdef APPEND_UNSUB
bool append_unsub{1}; /* -I */
#else
bool append_unsub{0}; /* -I */
#endif

bool UseUnivSelector{false};
bool UseNewsrcSelector{false};
bool UseAddSelector{true};
bool UseNewsgroupSelector{true};
int UseNewsSelector{SELECT_INIT - 1};
bool UseMouse{false};
char MouseModes[32]{"acjlptwvK"};
bool use_colors{false};
bool UseTk{false};
bool UseTcl{false};
bool UseSelNum{false};
bool SelNumGoto{false};
/* miscellania */

bool in_ng{false}; /* true if in a newsgroup */
char mode{'i'};    /* current state of trn */
char gmode{'I'};   /* general mode of trn */

FILE *tmpfp{nullptr}; /* scratch fp used for .rnlock, .rnlast, etc. */

/* Factored strings */

char sh[]{SH};
char defeditor[]{DEFEDITOR};
char hforhelp[]{"Type h for help.\n"};
#ifdef STRICTCR
char badcr[]{"\nUnnecessary CR ignored.\n"};
#endif
char readerr[]{"rn read error"};
char unsubto[]{"Unsubscribed to newsgroup %s\n"};
char cantopen[]{"Can't open %s\n"};
char cantcreate[]{"Can't create %s\n"};
char cantrecreate[]{"Can't recreate %s -- restoring older version.\n"
                    "Perhaps you are near or over quota?\n"};

char nocd[]{"Can't chdir to directory %s\n"};
