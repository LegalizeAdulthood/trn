/* trn/backpage.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_BACKPAGE_H
#define TRN_BACKPAGE_H

#include <config/typedef.h>

// things for doing the 'back page' command

void back_page_init();
void            back_page_final();
ArticlePosition virtual_read(ArticleLine index);
void virtual_write(ArticleLine index, ArticlePosition value);

#endif
