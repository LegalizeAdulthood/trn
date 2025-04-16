/* trn/bits.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#pragma once

#include "config/typedef.h"

struct Article;

extern int g_dmcount;

void bits_init();
void rc_to_bits();
bool set_firstart(const char *s);
void bits_to_rc();
void find_existing_articles();
void onemore(Article *ap);
void oneless(Article *ap);
void oneless_artnum(ART_NUM artnum);
void onemissing(Article *ap);
void unmark_as_read(Article *ap);
void set_read(Article *ap);
void delay_unmark(Article *ap);
void mark_as_read(Article *ap);
void mark_missing_articles();
void check_first(ART_NUM min);
void yankback();
bool chase_xrefs(bool until_key);
