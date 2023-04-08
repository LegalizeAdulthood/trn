/* common.cpp
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#include "common.h"

/* GLOBAL THINGS */

/* various things of type char */

char        g_msg[CBUFLEN];     /* general purpose message buffer */
char        g_buf[LBUFLEN + 1]; /* general purpose line buffer */
char        g_cmd_buf[CBUFLEN]; /* buffer for formatting system commands */
std::string g_indstr{">"};      /* indent for old article embedded in followup */

std::string g_privdir; /* private news directory */
std::string g_dfltcmd; /* 1st char is default command */

/* switches */

int g_word_wrap_offset{8}; /* right-hand column size (0 is off) */

int g_marking{NOMARKING}; /* -m */
int g_marking_areas{HALFPAGE_MARKING};

ART_LINE g_initlines{}; /* -i */

#ifdef APPEND_UNSUB
bool g_append_unsub{true}; /* -I */
#else
bool g_append_unsub{}; /* -I */
#endif

bool g_use_univ_selector{};
bool g_use_newsrc_selector{};
bool g_use_add_selector{true};
bool g_use_newsgroup_selector{true};
int g_use_news_selector{SELECT_INIT - 1};
bool g_use_mouse{};
// Array of minor_mode characters.
char g_mouse_modes[32]{"acjlptwvK"};
bool g_use_colors{};
bool g_use_tk{};
bool g_use_tcl{};
bool g_use_sel_num{};
bool g_sel_num_goto{};
/* miscellania */

bool         g_in_ng{};               /* true if in a newsgroup */
minor_mode   g_mode{MM_INITIALIZING};         /* current state of trn */
general_mode g_general_mode{GM_INIT}; /* general mode of trn */

FILE *g_tmpfp{}; /* scratch fp used for .rnlock, .rnlast, etc. */

/* Factored strings */

const char *g_hforhelp{"Type h for help.\n"};
#ifdef STRICTCR
char g_badcr[]{"\nUnnecessary CR ignored.\n"};
#endif
const char *g_readerr{"rn read error"};
const char *g_unsubto{"Unsubscribed to newsgroup %s\n"};
const char *g_cantopen{"Can't open %s\n"};
const char *g_cantcreate{"Can't create %s\n"};
const char *g_cantrecreate{"Can't recreate %s -- restoring older version.\n"
                           "Perhaps you are near or over quota?\n"};

const char *g_nocd{"Can't chdir to directory %s\n"};
