/* trn/ng.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NG_H
#define TRN_NG_H

#include <config/config2.h>
#include <config/typedef.h>

#include <string>

struct Article;

extern ArticleNum g_art;         /* current or prospective article # */
extern ArticleNum g_recent_art;  /* previous article # for '-' command */
extern ArticleNum g_curr_art;    /* current article # */
extern Article   *g_recent_artp; /* article_ptr equivalents */
extern Article   *g_curr_artp;   //
extern Article   *g_artp;        /* the article ptr we use when g_art is 0 */
extern int        g_check_count; /* how many articles have we read in the current newsgroup since the last checkpoint? */
extern int        g_do_check_when; /* how often to do checkpoint */
extern char      *g_subj_line;     /* what format to use for '=' */
#ifdef MAILCALL
extern int g_mail_count;           /* check for mail when 0 mod 5 */
#endif
extern char       *g_mail_call;
extern bool        g_force_last;      /* ought we show "End of newsgroup"? */
extern bool        g_force_grow;      /* do we want to recalculate size of newsgroup, e.g. after posting? */
extern int         g_scan_on;         /* -S */
extern bool        g_use_threads;     /* -x */
extern bool        g_unsafe_rc_saves; /* -U */
extern std::string g_default_cmd;     /* 1st char is default command */

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

void ng_init();
DoNewsgroupResult do_newsgroup(char *start_command);
#ifdef MAILCALL
void set_mail(bool force);
#endif
void set_default_cmd();
char ask_catchup();
bool output_subject(char *ptr, int flag);
char ask_memorize(char_int ch);

#endif
