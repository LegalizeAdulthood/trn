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
    NF_DELSEL = 0x04,
    NF_INCLUDED = 0x10,
    NF_UNTHREADED = 0x40,
    NF_VISIT = 0x80
};
DECLARE_FLAGS_ENUM(NewsgroupFlags, std::uint8_t);

struct NewsgroupData
{
    NewsgroupData    *prev;
    NewsgroupData    *next;
    Newsrc    *rc;                 /* which rc is this line from? */
    char      *rcline;             /* pointer to group's .newsrc line */
    ART_NUM    abs1st;             /* 1st real article in newsgroup */
    ART_NUM    ngmax;              /* high message num for the group */
    ART_UNREAD toread;             /* number of articles to be read in newsgroup */
                                   /* < 0 is invalid or unsubscribed newsgroup */
    NewsgroupNum          num;           /* a possible sort order for this group */
    int             numoffset;     /* offset from rcline to numbers on line */
    char            subscribechar; /* holds the character : or ! while spot is \0 */
    NewsgroupFlags flags;         /* flags for each group */
};

extern List       *g_ngdata_list;      /* a list of NGDATA */
extern int         g_ngdata_cnt;       //
extern NewsgroupNum      g_newsgroup_cnt;    /* all newsgroups in our current newsrc(s) */
extern NewsgroupNum      g_newsgroup_toread; //
extern ART_UNREAD  g_ng_min_toread;    /* == TR_ONE or TR_NONE */
extern NewsgroupData     *g_first_ng;         //
extern NewsgroupData     *g_last_ng;          //
extern NewsgroupData     *g_ngptr;            /* current newsgroup data ptr */
extern NewsgroupData     *g_current_ng;       /* stable current newsgroup so we can ditz with g_ngptr */
extern NewsgroupData     *g_recent_ng;        /* the prior newsgroup we visited */
extern NewsgroupData     *g_starthere;        /* set to the first newsgroup with unread news on startup */
extern NewsgroupData     *g_sel_page_np;      //
extern NewsgroupData     *g_sel_next_np;      //
extern ART_NUM     g_absfirst;         /* 1st real article in current newsgroup */
extern ART_NUM     g_firstart;         /* minimum unread article number in newsgroup */
extern ART_NUM     g_lastart;          /* maximum article number in newsgroup */
extern ART_UNREAD  g_missing_count;    /* for reports on missing articles */
extern std::string g_moderated;        //
extern bool        g_redirected;       //
extern std::string g_redirected_to;    //
extern bool        g_threaded_group;   //
extern NewsgroupData     *g_ng_go_ngptr;      //
extern ART_NUM     g_ng_go_artnum;     //
extern bool        g_novice_delays;    /* +f */
extern bool        g_in_ng;            /* true if in a newsgroup */

void ngdata_init();
void set_ng(NewsgroupData *np);
int access_ng();
void chdir_newsdir();
void grow_ng(ART_NUM newlast);
void sort_newsgroups();
void ng_skip();
ART_NUM getngsize(NewsgroupData *gp);

#endif
