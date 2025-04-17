/* common.cpp
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#include "config/common.h"

/* GLOBAL THINGS */

/* various things of type char */

char g_msg[CBUFLEN];     /* general purpose message buffer */
char g_buf[LBUFLEN + 1]; /* general purpose line buffer */
char g_cmd_buf[CBUFLEN]; /* buffer for formatting system commands */

/* Factored strings */
const char *g_h_for_help{"Type h for help.\n"};
#ifdef STRICTCR
char g_bad_cr[]{"\nUnnecessary CR ignored.\n"};
#endif
const char *g_unsub_to{"Unsubscribed to newsgroup %s\n"};
const char *g_cant_open{"Can't open %s\n"};
const char *g_cant_create{"Can't create %s\n"};

const char *g_no_cd{"Can't chdir to directory %s\n"};
