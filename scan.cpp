/* This file Copyright 1992 by Clifford A. Adams */
/* scan.c
 *
 * Scan initialization/cleanup and context control.
 */

/* CLEANUP: make a scontext file for the scan context stuff. */

#include "common.h"
#include "sorder.h"
#include "util.h"		/* allocation */
#include "scan.h"

/* the current values */
long *g_s_ent_sort{};      /* sorted list of entries in the context */
long g_s_ent_sort_max{};   /* maximum index of sorted array */
long g_s_ent_sorted_max{}; /* maximum index *that is sorted* */
long *g_s_ent_index{};     /* indexes into ent_sorted */
long g_s_ent_index_max{};  /* maximum entry number added */
int g_s_page_size{};       /* number of entries allocated for page */
/* (usually fixed, > max screen lines) */
PAGE_ENT *g_page_ents{}; /* array of entries on page */
/* -1 means not initialized for top and bottom entry */
long g_s_top_ent{}; /* top entry on page */
long g_s_bot_ent{}; /* bottom entry (note change) */
bool g_s_refill{};  /* does the page need refilling? */
/* refresh entries */
bool g_s_ref_all{}; /* refresh all on page */
bool g_s_ref_top{}; /* top status bar */
bool g_s_ref_bot{}; /* bottom status bar */
/* -1 for the next two entries means don't refresh */
short g_s_ref_status{}; /* line to start refreshing status from */
short g_s_ref_desc{};   /* line to start refreshing descript. from */
/* screen sizes */
short g_s_top_lines{};    /* lines for top status bar */
short g_s_bot_lines{};    /* lines for bottom status bar */
short g_s_status_cols{};  /* characters for status column */
short g_s_cursor_cols{};  /* characters for cursor column */
short g_s_itemnum_cols{}; /* characters for item number column */
short g_s_desc_cols{};    /* characters for description column */
/* pointer info */
short g_s_ptr_page_line{}; /* page_ent index */
long g_s_flags{};          /* misc. flags */
int g_s_num_contexts{};
SCONTEXT *g_s_contexts{}; /* array of context structures */
int g_s_cur_context{};    /* current context number */
int g_s_cur_type{};       /* current context type (for fast switching) */
/* options */
int g_s_itemnum{true}; /* show item numbers by default */
int g_s_mode_vi{};

void s_init_context(int cnum, int type)
{
    SCONTEXT*p;
    int i;

    /* g_s_num_contexts not incremented until last moment */
    if (cnum < 0 || cnum > g_s_num_contexts) {
	printf("s_init_context: illegal context number %d!\n",cnum) FLUSH;
	assert(false);
    }
    p = g_s_contexts + cnum;
    p->type = type;
    p->ent_sort = (long*)nullptr;
    p->ent_sort_max = -1;
    p->ent_sorted_max = -1;
    p->ent_index = (long*)nullptr;
    p->ent_index_max = -1;
    p->page_size = MAX_PAGE_SIZE;
    p->top_ent = -1;
    p->bot_ent = -1;
    p->refill = true;
    p->ref_all = true;
    p->ref_top = true;
    p->ref_bot = true;
    p->ref_status = -1;
    p->ref_desc = -1;
    /* next ones should be reset later */
    p->top_lines = 0;
    p->bot_lines = 0;
    p->status_cols = 0;
    p->cursor_cols = 0;
    p->desc_cols = 0;
    p->itemnum_cols = 0;
    p->ptr_page_line = 0;
    p->flags = 0;
    /* clear the page entries */
    for (i = 0; i < MAX_PAGE_SIZE; i++) {
	p->page_ents[i].entnum = 0;
	p->page_ents[i].lines = 0;
	p->page_ents[i].start_line = 0;
	p->page_ents[i].pageflags = (char)0;
    }
}

/* allocate a new context number and initialize it */
/* returns context number */
//int type;			/* context type */
int s_new_context(int type)
{
    int i;

    /* check for deleted contexts */
    for (i = 0; i < g_s_num_contexts; i++)
	if (g_s_contexts[i].type == 0)	/* deleted context */
	    break;
    if (i < g_s_num_contexts) {	/* a deleted one was found */
	s_init_context(i,type);
	return i;
    }
    /* none deleted, so allocate a new one */
    i = g_s_num_contexts;
    i++;
    if (i == 1) {	/* none allocated before */
	g_s_contexts = (SCONTEXT*)safemalloc(sizeof (SCONTEXT));
    } else {
	g_s_contexts = (SCONTEXT*)saferealloc((char*)g_s_contexts,
					i * sizeof (SCONTEXT));
    }
    g_s_contexts[i-1].page_ents =
			(PAGE_ENT*)safemalloc(MAX_PAGE_SIZE*sizeof(PAGE_ENT));
    s_init_context(i-1,type);
    g_s_num_contexts++;			/* now safe to increment */
    return g_s_num_contexts-1;
}

/* saves the current context */
void s_save_context()
{
    SCONTEXT*p;

    p = g_s_contexts + g_s_cur_context;

    p->type = g_s_cur_type;
    p->page_ents = g_page_ents;

    p->ent_sort = g_s_ent_sort;
    p->ent_sort_max = g_s_ent_sort_max;
    p->ent_sorted_max = g_s_ent_sorted_max;
    p->ent_index = g_s_ent_index;
    p->ent_index_max = g_s_ent_index_max;

    p->page_size = g_s_page_size;
    p->top_ent = g_s_top_ent;
    p->bot_ent = g_s_bot_ent;
    p->refill = g_s_refill;
    p->ref_all = g_s_ref_all;
    p->ref_top = g_s_ref_top;
    p->ref_bot = g_s_ref_bot;
    p->ref_status = g_s_ref_status;
    p->ref_desc = g_s_ref_desc;

    p->top_lines = g_s_top_lines;
    p->bot_lines = g_s_bot_lines;
    p->status_cols = g_s_status_cols;
    p->cursor_cols = g_s_cursor_cols;
    p->desc_cols = g_s_desc_cols;
    p->itemnum_cols = g_s_itemnum_cols;
    p->ptr_page_line = g_s_ptr_page_line;
    p->flags = g_s_flags;
}


// int newcontext;			/* context number to activate */
void s_change_context(int newcontext)
{
    SCONTEXT*p;

    if (newcontext < 0 || newcontext >= g_s_num_contexts) {
	printf("s_change_context: bad context number %d!\n",newcontext) FLUSH;
	assert(false);
    }
    g_s_cur_context = newcontext;
    p = g_s_contexts + newcontext;
    g_s_cur_type = p->type;
    g_page_ents = p->page_ents;

    g_s_ent_sort = p->ent_sort;
    g_s_ent_sort_max = p->ent_sort_max;
    g_s_ent_sorted_max = p->ent_sorted_max;
    g_s_ent_index = p->ent_index;
    g_s_ent_index_max = p->ent_index_max;

    g_s_page_size = p->page_size;
    g_s_top_ent = p->top_ent;
    g_s_bot_ent = p->bot_ent;
    g_s_refill = p->refill;
    g_s_ref_all = p->ref_all;
    g_s_ref_top = p->ref_top;
    g_s_ref_bot = p->ref_bot;
    g_s_ref_status = p->ref_status;
    g_s_ref_desc = p->ref_desc;
    /* next ones should be reset later */
    g_s_top_lines = p->top_lines;
    g_s_bot_lines = p->bot_lines;
    g_s_status_cols = p->status_cols;
    g_s_cursor_cols = p->cursor_cols;
    g_s_desc_cols = p->desc_cols;
    g_s_itemnum_cols = p->itemnum_cols;
    g_s_ptr_page_line = p->ptr_page_line;
    g_s_flags = p->flags;
}

/* implement later? */
void s_clean_contexts()
{
}

//int cnum;		/* context number to delete */
void s_delete_context(int cnum)
{
    if (cnum < 0 || cnum >= g_s_num_contexts) {
	printf("s_delete_context: illegal context number %d!\n",cnum) FLUSH;
	assert(false);
    }
    s_order_clean();
    /* mark the context as empty */
    g_s_contexts[cnum].type = 0;
}
