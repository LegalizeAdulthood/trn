/* This file Copyright 1992,1993 by Clifford A. Adams */
/* scan.h
 *
 * (Mostly scan context declarations.)
 */
#ifndef TRN_SCAN_H
#define TRN_SCAN_H

/* return codes.  First two should be the same as article scan codes */
enum
{
    S_QUIT = -1,
    S_ERR = -2,
    S_NOTFOUND = -3 /* command was not found in common scan subset */
};

/* number of entries allocated for a page */
enum
{
    MAX_PAGE_SIZE = 256
};

/* different context types */
enum scontext_type
{
    S_NONE = 0,
    S_ART = 1,
    S_GROUP = 2,
    S_HELP = 3,
    S_VIRT = 4
};

struct PAGE_ENT
{
    long entnum;	/* entry (article/message/newsgroup) number */
    short lines;	/* how many screen lines to describe? */
    short start_line;	/* screen line (0 = line under top status bar) */
    char pageflags;	/* not currently used. */
};

struct SCONTEXT
{
    scontext_type type; /* context type */

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
extern long *g_s_ent_sort;      /* sorted list of entries in the context */
extern long g_s_ent_sort_max;   /* maximum index of sorted array */
extern long g_s_ent_sorted_max; /* maximum index *that is sorted* */
extern long *g_s_ent_index;     /* indexes into ent_sorted */
extern long g_s_ent_index_max;  /* maximum entry number added */
extern int g_s_page_size;       /* number of entries allocated for page */
/* (usually fixed, > max screen lines) */
extern PAGE_ENT *g_page_ents; /* array of entries on page */
/* -1 means not initialized for top and bottom entry */
extern long g_s_top_ent; /* top entry on page */
extern long g_s_bot_ent; /* bottom entry (note change) */
extern bool g_s_refill;  /* does the page need refilling? */
/* refresh entries */
extern bool g_s_ref_all; /* refresh all on page */
extern bool g_s_ref_top; /* top status bar */
extern bool g_s_ref_bot; /* bottom status bar */
/* -1 for the next two entries means don't refresh */
extern short g_s_ref_status; /* line to start refreshing status from */
extern short g_s_ref_desc;   /* line to start refreshing descript. from */
/* screen sizes */
extern short g_s_top_lines;    /* lines for top status bar */
extern short g_s_bot_lines;    /* lines for bottom status bar */
extern short g_s_status_cols;  /* characters for status column */
extern short g_s_cursor_cols;  /* characters for cursor column */
extern short g_s_itemnum_cols; /* characters for item number column */
extern short g_s_desc_cols;    /* characters for description column */
/* pointer info */
extern short g_s_ptr_page_line; /* page_ent index */
extern long g_s_flags;          /* misc. flags */
extern int g_s_num_contexts;
extern SCONTEXT *g_s_contexts; /* array of context structures */
extern int g_s_cur_context;       /* current context number */
extern scontext_type g_s_cur_type;          /* current context type (for fast switching) */
/* options */
extern int g_s_itemnum; /* show item numbers by default */
extern int g_s_mode_vi;

void s_init_context(int cnum, scontext_type type);
int s_new_context(scontext_type type);
void s_save_context();
void s_change_context(int newcontext);
void s_clean_contexts();
void s_delete_context(int cnum);

#endif
