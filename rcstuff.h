/* rcstuff.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef RCSTUFF_H
#define RCSTUFF_H

#define TR_ONE ((ART_UNREAD) 1)
#define TR_NONE ((ART_UNREAD) 0)
#define TR_UNSUB ((ART_UNREAD) -1)
			/* keep this one as -1, some tests use >= TR_UNSUB */
#define TR_IGNORE ((ART_UNREAD) -2)
#define TR_BOGUS ((ART_UNREAD) -3)
#define TR_JUNK ((ART_UNREAD) -4)

enum
{
    NF_SEL = 0x01,
    NF_DEL = 0x02,
    NF_DELSEL = 0x04,
    NF_INCLUDED = 0x10,
    NF_UNTHREADED = 0x40,
    NF_VISIT = 0x80
};

#define ADDNEW_SUB ':'
#define ADDNEW_UNSUB '!'

enum
{
    GNG_RELOC = 0x0001,
    GNG_FUZZY = 0x0002
};

struct NEWSRC
{
    NEWSRC*	next;
    DATASRC*	datasrc;
    char*	name;		/* the name of the associated newsrc */
    char*	oldname;	/* the backup of the newsrc */
    char*	newname;	/* our working newsrc file */
    char*	infoname;	/* the time/size info file */
    char*	lockname;	/* the lock file we created */
    int		flags;
};

enum
{
    RF_ADD_NEWGROUPS = 0x0001,
    RF_ADD_GROUPS = 0x0002,
    RF_OPEN = 0x0100,
    RF_ACTIVE = 0x0200,
    RF_RCCHANGED = 0x0400
};

struct MULTIRC {
    NEWSRC* first;
    int num;
    int flags;
};

enum
{
    MF_SEL = 0x0001,
    MF_INCLUDED = 0x0010
};

#define multirc_ptr(n)  ((MULTIRC*)listnum2listitem(g_multirc_list,(long)(n)))
#define multirc_low()   ((MULTIRC*)listnum2listitem(g_multirc_list,existing_listnum(g_multirc_list,0L,1)))
#define multirc_high()  ((MULTIRC*)listnum2listitem(g_multirc_list,existing_listnum(g_multirc_list,g_multirc_list->high,-1)))
#define multirc_next(p) ((MULTIRC*)next_listitem(g_multirc_list,(char*)(p)))
#define multirc_prev(p) ((MULTIRC*)prev_listitem(g_multirc_list,(char*)(p)))

extern HASHTABLE *g_newsrc_hash;
extern MULTIRC *g_sel_page_mp;
extern MULTIRC *g_sel_next_mp;
extern LIST *g_multirc_list; /* a list of all MULTIRCs */
extern MULTIRC *g_multirc;   /* the current MULTIRC */
extern bool g_paranoid;      /* did we detect some inconsistency in .newsrc? */
extern int g_addnewbydefault;

bool rcstuff_init();
NEWSRC *new_newsrc(char *name, char *newsrc, char *add_ok);
bool use_multirc(MULTIRC *mp);
void unuse_multirc(MULTIRC *mptr);
bool use_next_multirc(MULTIRC *mptr);
bool use_prev_multirc(MULTIRC *mptr);
char *multirc_name(MULTIRC *mp);
void abandon_ng(NGDATA *np);
bool get_ng(const char *what, int flags);
bool relocate_newsgroup(NGDATA *move_np, NG_NUM newnum);
void list_newsgroups();
NGDATA *find_ng(const char *ngnam);
void cleanup_newsrc(NEWSRC *rp);
void sethash(NGDATA *np);
void checkpoint_newsrcs();
bool write_newsrcs(MULTIRC *mptr);
void get_old_newsrcs(MULTIRC *mptr);

#endif
