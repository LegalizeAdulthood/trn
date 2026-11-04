/* trn/scan.h
 *
 * (Mostly scan context declarations.)
 */
// This file Copyright 1992,1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCAN_H
#define TRN_SCAN_H

#include <vector>

// return codes.  First two should be the same as article scan codes
enum
{
    S_QUIT = -1,
    S_ERR = -2,
    S_NOT_FOUND = -3 // command was not found in common scan subset
};

// different context types
enum ScanContextType
{
    S_NONE = 0,
    S_ART = 1,
    S_GROUP = 2,
    S_HELP = 3,
    S_VIRT = 4
};

struct PageEntry
{
    long  ent_num;    // entry (article/message/newsgroup) number
    short lines;      // how many screen lines to describe?
    short start_line; // screen line (0 = line under top status bar)
    char  page_flags; // not currently used.
};

struct ScanContext
{
    ScanContextType type; // context type

    // ordering information
    std::vector<long> ent_sort;       // sorted list of entries in the context
    long              ent_sort_max;   // maximum index of sorted array
    long              ent_sorted_max; // maximum index *that is sorted*
    std::vector<long> ent_index;      // indexes into ent_sorted
    long              ent_index_max;  // maximum entry number added

    std::vector<PageEntry> page_ents; // entries on page
    // -1 means not initialized for top and bottom entry
    long top_ent; // top entry on page
    long bot_ent; // bottom entry (note change)
    bool refill;  // does the page need refilling?
    // refresh entries
    bool ref_all; // refresh all on page
    bool ref_top; // top status bar
    bool ref_bot; // bottom status bar
    // -1 for the next two entries means don't refresh
    short ref_status; // line to start refreshing status from
    short ref_desc;   // line to start refreshing descript. from
    // screen sizes
    short top_lines;     // lines for top status bar
    short bot_lines;     // lines for bottom status bar
    short status_cols;   // characters for status column
    short cursor_cols;   // characters for cursor column
    short item_num_cols; // characters for item number column
    short desc_cols;     // characters for description column
    // pointer info
    short ptr_page_line; // page_ent index
    long  flags;
};

struct ArticleScanSession
{
    int  new_context(ScanContextType type);
    void save_context();
    void change_context(int newcontext);
    void delete_context(int cnum);
    void sort();
    void order_clean();
    void order_add(long ent);
    long prev(long ent);
    long next(long ent);
    long prev_elig(long a);
    long next_elig(long a);
    long first();
    long last();
    bool page_has_unavailable_entries() const;

    // TODO: private
    ScanContext       &current_context();
    std::vector<long> &ent_sort();
    std::vector<long> &ent_index();
    void               init_context(int cnum, ScanContextType type);
    void               save_order_bounds();
    void               sort_basic();

    std::vector<ScanContext> contexts;      // context structures
    int                      cur_context{}; // current context number
    ScanContextType          cur_type{};    // current context type (for fast switching)

    // ordering information
    long ent_sort_max{};   // maximum index of sorted array
    long ent_sorted_max{}; // maximum index *that is sorted*
    long ent_index_max{};  // maximum entry number added
    bool order_changed{};  // If true, resort next time order is considered

    std::vector<PageEntry> page_ents; // entries on page
    // -1 means not initialized for top and bottom entry
    long top_ent{}; // top entry on page
    long bot_ent{}; // bottom entry (note change)
    bool refill{};  // does the page need refilling?
    // refresh entries
    bool ref_all{}; // refresh all on page
    bool ref_top{}; // top status bar
    bool ref_bot{}; // bottom status bar
    // -1 for the next two entries means don't refresh
    short ref_status{}; // line to start refreshing status from
    short ref_desc{};   // line to start refreshing descript. from
    // screen sizes
    short top_lines{};     // lines for top status bar
    short bot_lines{};     // lines for bottom status bar
    short status_cols{};   // characters for status column
    short cursor_cols{};   // characters for cursor column
    short item_num_cols{}; // characters for item number column
    short desc_cols{};     // characters for description column
    // pointer info
    short ptr_page_line{}; // page_ent index
    long  flags{};         // misc. flags
};

extern ArticleScanSession g_art_scan;

// options
extern int g_s_item_num; // show item numbers by default
extern int g_s_mode_vi;

#endif
