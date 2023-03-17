/* ngdata.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

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

EXT LIST* ngdata_list INIT(nullptr); /* a list of NGDATA */
EXT int ngdata_cnt INIT(0);
EXT NG_NUM newsgroup_cnt INIT(0); /* all newsgroups in our current newsrc(s) */
EXT NG_NUM newsgroup_toread INIT(0);
EXT ART_UNREAD ng_min_toread INIT(1); /* == TR_ONE or TR_NONE */

EXT NGDATA* first_ng INIT(nullptr);
EXT NGDATA* last_ng INIT(nullptr);
EXT NGDATA* ngptr INIT(nullptr);	/* current newsgroup data ptr */

EXT NGDATA* current_ng INIT(nullptr);/* stable current newsgroup so we can ditz with ngptr */
EXT NGDATA* recent_ng INIT(nullptr); /* the prior newsgroup we visited */
EXT NGDATA* starthere INIT(nullptr); /* set to the first newsgroup with unread news on startup */

#define ngdata_ptr(ngnum) ((NGDATA*)listnum2listitem(ngdata_list,(long)(ngnum)))
/*#define ngdata_num(ngptr) listitem2listnum(ngdata_list,(char*)ngptr)*/

EXT NGDATA* sel_page_np;
EXT NGDATA* sel_next_np;

EXT ART_NUM absfirst INIT(0);	/* 1st real article in current newsgroup */
EXT ART_NUM firstart INIT(0);	/* minimum unread article number in newsgroup */
EXT ART_NUM lastart INIT(0);	/* maximum article number in newsgroup */
EXT ART_UNREAD missing_count;	/* for reports on missing articles */

EXT char* moderated;
EXT char* redirected;
EXT bool ThreadedGroup;

/* CAA goto-newsgroup extensions */
EXT NGDATA* ng_go_ngptr INIT(nullptr);
EXT ART_NUM ng_go_artnum INIT(0);
EXT char* ng_go_msgid INIT(nullptr);

void ngdata_init();
void set_ng(NGDATA *np);
int access_ng();
void chdir_newsdir();
void grow_ng(ART_NUM);
void sort_newsgroups();
void ng_skip();
ART_NUM getngsize(NGDATA *gp);
