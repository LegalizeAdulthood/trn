/* trn/backpage.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_BACKPAGE_H
#define TRN_BACKPAGE_H

#include <config/common.h>
#include <config/typedef.h>

// things for doing the 'back page' command

struct BackPageCache
{
    void final();

    int             vary_fd{-1};         // virtual array file for storing  file offsets
    ArticlePosition vary_buf[VARY_SIZE]; // current window onto virtual array
    long            old_offset{-1};      // offset to block currently in window
#ifdef DEBUG
    int max_index{-1};
#endif
};

extern BackPageCache g_back_page;

void back_page_init();
ArticlePosition virtual_read(ArticleLine index);
void virtual_write(ArticleLine index, ArticlePosition value);

#endif
