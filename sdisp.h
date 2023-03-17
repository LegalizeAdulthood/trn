/* This file Copyright 1992 by Clifford A. Adams */
/* sdisp.h
 *
 * scan display functions
 */

	/* height of screen in characters */
EXT int scr_height INIT(0);
	/* width of screen in characters */
EXT int scr_width INIT(0);

/* has the window been resized? */
EXT bool s_resized INIT(false);

void s_goxy(int, int);
void s_mail_and_place(void);
void s_refresh_top(void);
void s_refresh_bot(void);
void s_refresh_entzone(void);
void s_place_ptr(void);
void s_refresh_status(int);
void s_refresh_description(int);
void s_ref_entry(int, int);
void s_rub_ptr(void);
void s_refresh(void);
int s_initscreen(void);
void s_ref_status_onpage(long);
void s_resize_win(void);
