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

ArticleScanSession g_art_scan{};

// options
int g_s_item_num{true}; // show item numbers by default
int g_s_mode_vi{};

static void s_clean_contexts();

void ArticleScanSession::init_context(int cnum, ScanContextType type)
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
int ArticleScanSession::new_context(ScanContextType type)
{
    int i;

    // check for deleted contexts
    for (i = 0; i < size_cast<int>(contexts); i++)
    {
        if (contexts[i].type == S_NONE) // deleted context
        {
            break;
        }
    }
    if (i < size_cast<int>(contexts)) // a deleted one was found
    {
        init_context(i, type);
        return i;
    }
    // none deleted, so allocate a new one
    contexts.emplace_back();
    i = size_cast<int>(contexts) - 1;
    init_context(i, type);
    return i;
}

// saves the current context
void ArticleScanSession::save_context()
{
    ScanContext *p = &contexts[cur_context];

    p->type = cur_type;
    p->page_ents = page_ents;

    p->ent_sort_max = ent_sort_max;
    p->ent_sorted_max = ent_sorted_max;
    p->ent_index_max = ent_index_max;

    p->top_ent = top_ent;
    p->bot_ent = bot_ent;
    p->refill = refill;
    p->ref_all = ref_all;
    p->ref_top = ref_top;
    p->ref_bot = ref_bot;
    p->ref_status = ref_status;
    p->ref_desc = ref_desc;

    p->top_lines = top_lines;
    p->bot_lines = bot_lines;
    p->status_cols = status_cols;
    p->cursor_cols = cursor_cols;
    p->desc_cols = desc_cols;
    p->item_num_cols = item_num_cols;
    p->ptr_page_line = ptr_page_line;
    p->flags = flags;
}

// int newcontext;                      // context number to activate
void ArticleScanSession::change_context(int newcontext)
{
    if (newcontext < 0 || newcontext >= size_cast<int>(contexts))
    {
        fmt::print("s_change_context: bad context number {}!\n", newcontext);
        TRN_ASSERT(false);
    }
    cur_context = newcontext;
    ScanContext *p = &contexts[newcontext];
    cur_type = p->type;
    page_ents = p->page_ents;

    ent_sort_max = p->ent_sort_max;
    ent_sorted_max = p->ent_sorted_max;
    ent_index_max = p->ent_index_max;

    top_ent = p->top_ent;
    bot_ent = p->bot_ent;
    refill = p->refill;
    ref_all = p->ref_all;
    ref_top = p->ref_top;
    ref_bot = p->ref_bot;
    ref_status = p->ref_status;
    ref_desc = p->ref_desc;
    // next ones should be reset later
    top_lines = p->top_lines;
    bot_lines = p->bot_lines;
    status_cols = p->status_cols;
    cursor_cols = p->cursor_cols;
    desc_cols = p->desc_cols;
    item_num_cols = p->item_num_cols;
    ptr_page_line = p->ptr_page_line;
    flags = p->flags;
}

// implement later?
static void s_clean_contexts()
{
}

// int cnum;             // context number to delete
void s_delete_context(int cnum)
{
    if (cnum < 0 || cnum >= size_cast<int>(g_art_scan.contexts))
    {
        fmt::print("s_delete_context: illegal context number {}!\n", cnum);
        TRN_ASSERT(false);
    }
    if (cnum == g_art_scan.cur_context)
    {
        s_order_clean();
    }
    else
    {
        g_art_scan.contexts[cnum].ent_sort.clear();
        g_art_scan.contexts[cnum].ent_sort_max = -1;
        g_art_scan.contexts[cnum].ent_sorted_max = -1;
        g_art_scan.contexts[cnum].ent_index.clear();
        g_art_scan.contexts[cnum].ent_index_max = -1;
        g_art_scan.contexts[cnum].page_ents.clear();
    }
    // mark the context as empty
    g_art_scan.contexts[cnum].type = S_NONE;
}
