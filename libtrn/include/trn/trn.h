/* trn/trn.h
 */
// This software is copyrighted as detailed in the LICENSE file.
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
    ING_NO_SERVER,
    ING_DISPLAY,
    ING_MESSAGE
};

extern std::string g_newsgroup_name;         // name of current newsgroup
extern std::string g_newsgroup_dir;          // same thing in directory name form
extern std::string g_patch_level;            //
extern int         g_find_last;              // -r
extern bool        g_verbose;                // +t
extern bool        g_use_univ_selector;      //
extern bool        g_use_newsrc_selector;    //
extern bool        g_use_newsgroup_selector; //
extern int         g_use_news_selector;      //

void                 trn_init();
void                 do_multirc();
InputNewsgroupResult input_newsgroup();
void                 check_active_refetch(bool force);
void                 trn_version();
void                 set_newsgroup_name(const char *what);
int                  trn_main(int argc, char *argv[]);

#endif
