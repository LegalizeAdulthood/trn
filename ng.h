/* ng.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NG_H
#define TRN_NG_H

struct ARTICLE;

extern ART_NUM  g_art;         /* current or prospective article # */
extern ART_NUM  g_recent_art;  /* previous article # for '-' command */
extern ART_NUM  g_curr_art;    /* current article # */
extern ARTICLE *g_recent_artp; /* article_ptr equivalents */
extern ARTICLE *g_curr_artp;
extern ARTICLE *g_artp;        /* the article ptr we use when g_art is 0 */
extern int      g_checkcount;  /* how many articles have we read in the current newsgroup since the last checkpoint? */
extern int      g_docheckwhen; /* how often to do checkpoint */
extern char    *g_subjline;    /* what format to use for '=' */
#ifdef MAILCALL
extern int g_mailcount; /* check for mail when 0 mod 5 */
#endif
extern char *g_mailcall;
extern bool  g_forcelast; /* ought we show "End of newsgroup"? */
extern bool  g_forcegrow; /* do we want to recalculate size of newsgroup, e.g. after posting? */
extern int   g_scanon;    /* -S */

enum do_newsgroup_result
{
    NG_ERROR = -1,
    NG_NORM = 0,
    NG_ASK = 1,
    NG_MINUS = 2,
    NG_SELPRIOR = 3,
    NG_SELNEXT = 4,
    NG_NOSERVER = 5,
    NG_NEXT = 6,
    NG_GO_ARTICLE = 7
};

void ng_init();
do_newsgroup_result do_newsgroup(char *start_command);
#ifdef MAILCALL
void setmail(bool force);
#endif
void setdfltcmd();
char ask_catchup();
bool output_subject(char *ptr, int flag);
char ask_memorize(char_int ch);

#endif
