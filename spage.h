/* This file Copyright 1992, 1993 by Clifford A. Adams */
/* spage.h
 *
 * functions to manage a page of entries.
 */

bool s_fillpage_backward(long);
bool s_fillpage_forward(long);
bool s_refillpage(void);
int s_fillpage(void);
void s_cleanpage(void);
void s_go_top_page(void);
void s_go_bot_page(void);
bool s_go_top_ents(void);
bool s_go_bot_ents(void);
void s_go_next_page(void);
void s_go_prev_page(void);
