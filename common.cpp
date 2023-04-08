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
