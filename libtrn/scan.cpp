/* scan.cpp
 *
 * Scan initialization/cleanup and context control.
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/scan.h>

#include <config/common.h>
#include <trn/size_cast.h>
#include <trn/sorder.h>

#include <fmt/format.h>

#include <cstdio>

// TODO: make a scontext file for the scan context stuff.

ArticleBrowseSession g_browse{};

// options
int g_s_item_num{true}; // show item numbers by default
int g_s_mode_vi{};

static void s_clean_contexts();

void ArticleBrowseSession::init_context(int cnum, ScanContextType type)
{
    if (cnum < 0 || cnum >= size_cast<int>(contexts))
    {
        fmt::print("s_init_context: illegal context number {}!\n", cnum);
        TRN_ASSERT(false);
    }
    ScanContext *p = &contexts[cnum];
    p->type = type;
    p->ent_sort.clear();
    p->ent_sort_max = -1;
    p->ent_sorted_max = -1;
    p->ent_index.clear();
    p->ent_index_max = -1;
    p->page_ents.clear();
    p->top_ent = -1;
    p->bot_ent = -1;
    p->refill = true;
    p->ref_all = true;
    p->ref_top = true;
    p->ref_bot = true;
    p->ref_status = -1;
    p->ref_desc = -1;
    // next ones should be reset later
    p->top_lines = 0;
    p->bot_lines = 0;
    p->status_cols = 0;
    p->cursor_cols = 0;
    p->desc_cols = 0;
    p->item_num_cols = 0;
    p->ptr_page_line = 0;
    p->flags = 0;
}

// allocate a new context number and initialize it
// returns context number
//int type;                     // context type
int s_new_context(ScanContextType type)
{
    int i;

    // check for deleted contexts
    for (i = 0; i < size_cast<int>(g_browse.contexts); i++)
    {
        if (g_browse.contexts[i].type == S_NONE) // deleted context
        {
            break;
        }
    }
    if (i < size_cast<int>(g_browse.contexts)) // a deleted one was found
    {
        g_browse.init_context(i, type);
        return i;
    }
    // none deleted, so allocate a new one
    g_browse.contexts.emplace_back();
    i = size_cast<int>(g_browse.contexts) - 1;
    g_browse.init_context(i, type);
    return i;
}

// saves the current context
void s_save_context()
{
    ScanContext *p = &g_browse.contexts[g_browse.cur_context];

    p->type = g_browse.cur_type;
    p->page_ents = g_browse.page_ents;

    p->ent_sort_max = g_browse.ent_sort_max;
    p->ent_sorted_max = g_browse.ent_sorted_max;
    p->ent_index_max = g_browse.ent_index_max;

    p->top_ent = g_browse.top_ent;
    p->bot_ent = g_browse.bot_ent;
    p->refill = g_browse.refill;
    p->ref_all = g_browse.ref_all;
    p->ref_top = g_browse.ref_top;
    p->ref_bot = g_browse.ref_bot;
    p->ref_status = g_browse.ref_status;
    p->ref_desc = g_browse.ref_desc;

    p->top_lines = g_browse.top_lines;
    p->bot_lines = g_browse.bot_lines;
    p->status_cols = g_browse.status_cols;
    p->cursor_cols = g_browse.cursor_cols;
    p->desc_cols = g_browse.desc_cols;
    p->item_num_cols = g_browse.item_num_cols;
    p->ptr_page_line = g_browse.ptr_page_line;
    p->flags = g_browse.flags;
}

// int newcontext;                      // context number to activate
void s_change_context(int newcontext)
{
    if (newcontext < 0 || newcontext >= size_cast<int>(g_browse.contexts))
    {
        fmt::print("s_change_context: bad context number {}!\n", newcontext);
        TRN_ASSERT(false);
    }
    g_browse.cur_context = newcontext;
    ScanContext *p = &g_browse.contexts[newcontext];
    g_browse.cur_type = p->type;
    g_browse.page_ents = p->page_ents;

    g_browse.ent_sort_max = p->ent_sort_max;
    g_browse.ent_sorted_max = p->ent_sorted_max;
    g_browse.ent_index_max = p->ent_index_max;

    g_browse.top_ent = p->top_ent;
    g_browse.bot_ent = p->bot_ent;
    g_browse.refill = p->refill;
    g_browse.ref_all = p->ref_all;
    g_browse.ref_top = p->ref_top;
    g_browse.ref_bot = p->ref_bot;
    g_browse.ref_status = p->ref_status;
    g_browse.ref_desc = p->ref_desc;
    // next ones should be reset later
    g_browse.top_lines = p->top_lines;
    g_browse.bot_lines = p->bot_lines;
    g_browse.status_cols = p->status_cols;
    g_browse.cursor_cols = p->cursor_cols;
    g_browse.desc_cols = p->desc_cols;
    g_browse.item_num_cols = p->item_num_cols;
    g_browse.ptr_page_line = p->ptr_page_line;
    g_browse.flags = p->flags;
}

// implement later?
static void s_clean_contexts()
{
}

// int cnum;             // context number to delete
void s_delete_context(int cnum)
{
    if (cnum < 0 || cnum >= size_cast<int>(g_browse.contexts))
    {
        fmt::print("s_delete_context: illegal context number {}!\n", cnum);
        TRN_ASSERT(false);
    }
    if (cnum == g_browse.cur_context)
    {
        s_order_clean();
    }
    else
    {
        g_browse.contexts[cnum].ent_sort.clear();
        g_browse.contexts[cnum].ent_sort_max = -1;
        g_browse.contexts[cnum].ent_sorted_max = -1;
        g_browse.contexts[cnum].ent_index.clear();
        g_browse.contexts[cnum].ent_index_max = -1;
        g_browse.contexts[cnum].page_ents.clear();
    }
    // mark the context as empty
    g_browse.contexts[cnum].type = S_NONE;
}
