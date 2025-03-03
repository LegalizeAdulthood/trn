/* bits.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#pragma once

#include "typedef.h"

struct ARTICLE;

extern int g_dmcount;

void bits_init();
void rc_to_bits();
bool set_firstart(const char *s);
void bits_to_rc();
void find_existing_articles();
void onemore(ARTICLE *ap);
void oneless(ARTICLE *ap);
void oneless_artnum(ART_NUM artnum);
void onemissing(ARTICLE *ap);
void unmark_as_read(ARTICLE *ap);
void set_read(ARTICLE *ap);
void delay_unmark(ARTICLE *ap);
void mark_as_read(ARTICLE *ap);
void mark_missing_articles();
void check_first(ART_NUM min);
void yankback();
bool chase_xrefs(bool until_key);
