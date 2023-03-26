/* artstate.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef ARTSTATE_H
#define ARTSTATE_H

#include "search.h"

extern bool g_reread;          /* consider current art temporarily unread? */
extern bool g_do_fseek;        /* should we back up in article file? */
extern bool g_oldsubject;      /* not 1st art in subject thread */
extern ART_LINE g_topline;     /* top line of current screen */
extern bool g_do_hiding;       /* hide header lines with -h? */
extern bool g_is_mime;         /* process mime in an article? */
extern bool g_multimedia_mime; /* images/audio to see/hear? */
extern bool g_rotate;          /* has rotation been requested? */
extern char *g_prompt;         /* pointer to current prompt */
extern char *g_firstline;      /* special first line? */
extern const char *g_hideline; /* custom line hiding? */
extern const char *g_pagestop; /* custom page terminator? */
extern COMPEX g_hide_compex;
extern COMPEX g_page_compex;

#endif
