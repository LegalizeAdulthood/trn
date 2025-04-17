/* trn/ngdata.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NGDATA_H
#define TRN_NGDATA_H

#include <config/typedef.h>

#include "trn/enum-flags.h"

#include <cstdint>
#include <string>

struct List;
struct Newsrc;

enum NewsgroupFlags : std::uint8_t
{
    NF_NONE = 0x00,
    NF_SEL = 0x01,
    NF_DEL = 0x02,
    NF_DEL_SEL = 0x04,
    NF_INCLUDED = 0x10,
    NF_UNTHREADED = 0x40,
    NF_VISIT = 0x80
};
DECLARE_FLAGS_ENUM(NewsgroupFlags, std::uint8_t);

struct NewsgroupData
{
    NewsgroupData *prev;
    NewsgroupData *next;
    Newsrc        *rc;             /* which rc is this line from? */
    char          *rc_line;        /* pointer to group's .newsrc line */
    ArticleNum     abs_first;      /* 1st real article in newsgroup */
    ArticleNum     ng_max;         /* high message num for the group */
    ArticleUnread  to_read;        /* number of articles to be read in newsgroup */
                                   /* < 0 is invalid or unsubscribed newsgroup */
    NewsgroupNum   num;            /* a possible sort order for this group */
    int            num_offset;     /* offset from rcline to numbers on line */
    char           subscribe_char; /* holds the character : or ! while spot is \0 */
    NewsgroupFlags flags;          /* flags for each group */
};

extern List          *g_newsgroup_data_list;   /* a list of NGDATA */
extern int            g_newsgroup_data_count;  //
extern NewsgroupNum   g_newsgroup_count;       /* all newsgroups in our current newsrc(s) */
extern NewsgroupNum   g_newsgroup_to_read;     //
extern ArticleUnread  g_newsgroup_min_to_read; /* == TR_ONE or TR_NONE */
extern NewsgroupData *g_first_newsgroup;       //
extern NewsgroupData *g_last_newsgroup;        //
extern NewsgroupData *g_newsgroup_ptr;         /* current newsgroup data ptr */
extern NewsgroupData *g_current_newsgroup;     /* stable current newsgroup so we can ditz with g_ngptr */
extern NewsgroupData *g_recent_newsgroup;      /* the prior newsgroup we visited */
extern NewsgroupData *g_start_here;            /* set to the first newsgroup with unread news on startup */
extern NewsgroupData *g_sel_page_np;           //
extern NewsgroupData *g_sel_next_np;           //
extern ArticleNum     g_abs_first;             /* 1st real article in current newsgroup */
extern ArticleNum     g_first_art;             /* minimum unread article number in newsgroup */
extern ArticleNum     g_last_art;              /* maximum article number in newsgroup */
extern ArticleUnread  g_missing_count;         /* for reports on missing articles */
extern std::string    g_moderated;             //
extern bool           g_redirected;            //
extern std::string    g_redirected_to;         //
extern bool           g_threaded_group;        //
extern NewsgroupData *g_ng_go_newsgroup_ptr;   //
extern ArticleNum     g_ng_go_art_num;         //
extern bool           g_novice_delays;         /* +f */
extern bool           g_in_ng;                 /* true if in a newsgroup */

void newsgroup_data_init();
void set_newsgroup(NewsgroupData *np);
int access_newsgroup();
void chdir_news_dir();
void grow_newsgroup(ArticleNum new_last);
void sort_newsgroups();
void newsgroup_skip();
ArticleNum get_newsgroup_size(NewsgroupData *gp);

#endif
