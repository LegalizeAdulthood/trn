/* This file Copyright 1992, 1993 by Clifford A. Adams */
/* trn/spage.h
 *
 * functions to manage a page of entries.
 */
#ifndef TRN_SPAGE_H
#define TRN_SPAGE_H

bool s_fill_page_backward(long end);
bool s_fill_page_forward(long start);
bool s_refill_page();
int  s_fill_page();
void s_clean_page();
void s_go_top_page();
void s_go_bot_page();
bool s_go_top_ents();
bool s_go_bot_ents();
void s_go_next_page();
void s_go_prev_page();

#endif
