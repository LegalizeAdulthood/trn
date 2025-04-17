/* trn/artstate.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ARTSTATE_H
#define TRN_ARTSTATE_H

#include <config/typedef.h>

#include "trn/search.h"

#include <string>

extern bool          g_reread;          /* consider current art temporarily unread? */
extern bool          g_do_fseek;        /* should we back up in article file? */
extern bool          g_old_subject;     /* not 1st art in subject thread */
extern ArticleLine   g_top_line;        /* top line of current screen */
extern bool          g_do_hiding;       /* hide header lines with -h? */
extern bool          g_is_mime;         /* process mime in an article? */
extern bool          g_multimedia_mime; /* images/audio to see/hear? */
extern bool          g_rotate;          /* has rotation been requested? */
extern std::string   g_prompt;          /* pointer to current prompt */
extern char         *g_first_line;      /* special first line? */
extern const char   *g_hide_line;       /* custom line hiding? */
extern const char   *g_page_stop;       /* custom page terminator? */
extern CompiledRegex g_hide_compex;
extern CompiledRegex g_page_compex;

#endif
