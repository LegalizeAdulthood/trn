/* ngdata.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef NGDATA_H
#define NGDATA_H

struct NGDATA
{
    NGDATA* prev;
    NGDATA* next;
    NEWSRC* rc;		/* which rc is this line from? */
    char* rcline;	/* pointer to group's .newsrc line */
    ART_NUM abs1st;	/* 1st real article in newsgroup */
    ART_NUM ngmax;	/* high message num for the group */
    ART_UNREAD toread;	/* number of articles to be read in newsgroup */
			/* < 0 is invalid or unsubscribed newsgroup */
    NG_NUM num;		/* a possible sort order for this group */
    int numoffset;	/* offset from rcline to numbers on line */
    char subscribechar;	/* holds the character : or ! while spot is \0 */
    char flags;  	/* flags for each group */
};

extern LIST *g_ngdata_list; /* a list of NGDATA */
extern int g_ngdata_cnt;
extern NG_NUM g_newsgroup_cnt; /* all newsgroups in our current newsrc(s) */
extern NG_NUM g_newsgroup_toread;
extern ART_UNREAD g_ng_min_toread; /* == TR_ONE or TR_NONE */
extern NGDATA *g_first_ng;
extern NGDATA *g_last_ng;
extern NGDATA *g_ngptr;      /* current newsgroup data ptr */
extern NGDATA *g_current_ng; /* stable current newsgroup so we can ditz with g_ngptr */
extern NGDATA *g_recent_ng;  /* the prior newsgroup we visited */
extern NGDATA *g_starthere;  /* set to the first newsgroup with unread news on startup */
extern NGDATA *g_sel_page_np;
extern NGDATA *g_sel_next_np;
extern ART_NUM g_absfirst;         /* 1st real article in current newsgroup */
extern ART_NUM g_firstart;         /* minimum unread article number in newsgroup */
extern ART_NUM g_lastart;          /* maximum article number in newsgroup */
extern ART_UNREAD g_missing_count; /* for reports on missing articles */
extern char *g_moderated;
extern char *g_redirected;
extern bool g_threaded_group;
extern NGDATA *g_ng_go_ngptr;
extern ART_NUM g_ng_go_artnum;
extern char *g_ng_go_msgid;

#define ngdata_ptr(ngnum) ((NGDATA*)listnum2listitem(g_ngdata_list,(long)(ngnum)))

void ngdata_init();
void set_ng(NGDATA *np);
int access_ng();
void chdir_newsdir();
void grow_ng(ART_NUM newlast);
void sort_newsgroups();
void ng_skip();
ART_NUM getngsize(NGDATA *gp);

#endif
