/* This file Copyright 1992,1993 by Clifford A. Adams */
/* scan.h
 *
 * (Mostly scan context declarations.)
 */

/* return codes.  First two should be the same as article scan codes */
#define S_QUIT (-1)
#define S_ERR (-2)
/* command was not found in common scan subset */
#define S_NOTFOUND (-3)

/* number of entries allocated for a page */
#define MAX_PAGE_SIZE 256

/* different context types */
#define S_ART 1
#define S_GROUP 2
#define S_HELP 3
#define S_VIRT 4

struct PAGE_ENT
{
    long entnum;	/* entry (article/message/newsgroup) number */
    short lines;	/* how many screen lines to describe? */
    short start_line;	/* screen line (0 = line under top status bar) */
    char pageflags;	/* not currently used. */
};

struct SCONTEXT
{
    int type;			/* context type */

    /* ordering information */
    long* ent_sort;		/* sorted list of entries in the context */
    long ent_sort_max;		/* maximum index of sorted array */
    long ent_sorted_max;	/* maximum index *that is sorted* */
    long* ent_index;		/* indexes into ent_sorted */
    long ent_index_max;		/* maximum entry number added */

    int page_size;		/* number of entries allocated for page */
				/* (usually fixed, > max screen lines) */
    PAGE_ENT* page_ents;	/* array of entries on page */
    /* -1 means not initialized for top and bottom entry */
    long top_ent;		/* top entry on page */
    long bot_ent;		/* bottom entry (note change) */
    bool refill;		/* does the page need refilling? */
    /* refresh entries */
    bool ref_all;		/* refresh all on page */
    bool ref_top;		/* top status bar */
    bool ref_bot;		/* bottom status bar */
    /* -1 for the next two entries means don't refresh */
    short ref_status;		/* line to start refreshing status from */
    short ref_desc;		/* line to start refreshing descript. from */
    /* screen sizes */
    short top_lines;		/* lines for top status bar */
    short bot_lines;		/* lines for bottom status bar */
    short status_cols;		/* characters for status column */
    short cursor_cols;		/* characters for cursor column */
    short itemnum_cols;		/* characters for item number column */
    short desc_cols;		/* characters for description column */
    /* pointer info */
    short ptr_page_line;	/* page_ent index */
    long flags;
};

/* the current values */

EXT long* s_ent_sort INIT(nullptr);	/* sorted list of entries in the context */
EXT long s_ent_sort_max INIT(0);	/* maximum index of sorted array */
EXT long s_ent_sorted_max INIT(0);	/* maximum index *that is sorted* */
EXT long* s_ent_index INIT(nullptr);	/* indexes into ent_sorted */
EXT long s_ent_index_max INIT(0);	/* maximum entry number added */

EXT int s_page_size INIT(0);		/* number of entries allocated for page */
				/* (usually fixed, > max screen lines) */
EXT PAGE_ENT* page_ents INIT(nullptr);	/* array of entries on page */
/* -1 means not initialized for top and bottom entry */
EXT long s_top_ent INIT(0);		/* top entry on page */
EXT long s_bot_ent INIT(0);		/* bottom entry (note change) */
EXT bool s_refill INIT(false);		/* does the page need refilling? */
/* refresh entries */
EXT bool s_ref_all INIT(0);		/* refresh all on page */
EXT bool s_ref_top INIT(0);		/* top status bar */
EXT bool s_ref_bot INIT(0);		/* bottom status bar */
/* -1 for the next two entries means don't refresh */
EXT short s_ref_status INIT(0);		/* line to start refreshing status from */
EXT short s_ref_desc INIT(0);		/* line to start refreshing descript. from */
/* screen sizes */
EXT short s_top_lines INIT(0);		/* lines for top status bar */
EXT short s_bot_lines INIT(0);		/* lines for bottom status bar */
EXT short s_status_cols INIT(0);	/* characters for status column */
EXT short s_cursor_cols INIT(0);	/* characters for cursor column */
EXT short s_itemnum_cols INIT(0);	/* characters for item number column */
EXT short s_desc_cols INIT(0);		/* characters for description column */
/* pointer info */
EXT short s_ptr_page_line INIT(0);	/* page_ent index */
EXT long  s_flags INIT(0);		/* misc. flags */

EXT int s_num_contexts INIT(0);
/* array of context structures */
EXT SCONTEXT* s_contexts INIT(nullptr);

/* current context number */
EXT int s_cur_context INIT(0);
/* current context type (for fast switching) */
EXT int s_cur_type INIT(0);

/* options */
/* show item numbers by default */
EXT int s_itemnum INIT(true);
EXT int s_mode_vi INIT(0);

void s_init_context(int, int);
int s_new_context(int);
void s_save_context();
void s_change_context(int);
void s_clean_contexts();
void s_delete_context(int);
