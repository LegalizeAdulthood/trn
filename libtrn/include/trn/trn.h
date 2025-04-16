/* trn/trn.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_TRN_H
#define TRN_TRN_H

#include <string>

enum InputNewsgroupResult
{
    ING_NORM = 0,
    ING_ASK,
    ING_INPUT,
    ING_ERASE,
    ING_QUIT,
    ING_ERROR,
    ING_SPECIAL,
    ING_BREAK,
    ING_RESTART,
    ING_NOSERVER,
    ING_DISPLAY,
    ING_MESSAGE
};

extern std::string g_ngname;                 /* name of current newsgroup */
extern std::string g_ngdir;                  /* same thing in directory name form */
extern std::string g_patchlevel;             //
extern int         g_findlast;               /* -r */
extern bool        g_verbose;                /* +t */
extern bool        g_use_univ_selector;      //
extern bool        g_use_newsrc_selector;    //
extern bool        g_use_newsgroup_selector; //
extern int         g_use_news_selector;      //

void trn_init();
void do_multirc();
InputNewsgroupResult input_newsgroup();
void check_active_refetch(bool force);
void trn_version();
void set_ngname(const char *what);
int trn_main(int argc, char *argv[]);

#endif
