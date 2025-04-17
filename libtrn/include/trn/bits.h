/* trn/bits.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#pragma once

#include "config/typedef.h"

struct Article;

extern int g_dm_count;

void bits_init();
void rc_to_bits();
bool set_first_art(const char *s);
void bits_to_rc();
void find_existing_articles();
void one_more(Article *ap);
void one_less(Article *ap);
void one_less_art_num(ArticleNum art_num);
void one_missing(Article *ap);
void unmark_as_read(Article *ap);
void set_read(Article *ap);
void delay_unmark(Article *ap);
void mark_as_read(Article *ap);
void mark_missing_articles();
void check_first(ArticleNum min);
void yank_back();
bool chase_xrefs(bool until_key);
