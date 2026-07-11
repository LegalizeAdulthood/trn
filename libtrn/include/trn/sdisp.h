/* trn/sdisp.h
 *
 * scan display functions
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SDISP_H
#define TRN_SDISP_H

extern int  g_scr_height; // height of screen in characters
extern int  g_scr_width;  // width of screen in characters
extern bool g_s_resized;  // has the window been resized?

void s_goxy(int x, int y);
void s_mail_and_place();
void s_place_ptr();
void s_rub_ptr();
void s_refresh();
int  s_init_screen();
void s_ref_status_on_page(long ent);
void s_resize_win();

#endif
