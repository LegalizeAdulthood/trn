/* This file Copyright 1992, 1993 by Clifford A. Adams */
/* spage.h
 *
 * functions to manage a page of entries.
 */

bool s_fillpage_backward(long);
bool s_fillpage_forward(long);
bool s_refillpage();
int s_fillpage();
void s_cleanpage();
void s_go_top_page();
void s_go_bot_page();
bool s_go_top_ents();
bool s_go_bot_ents();
void s_go_next_page();
void s_go_prev_page();
