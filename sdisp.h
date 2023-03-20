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

void s_goxy(int x, int y);
void s_mail_and_place();
void s_refresh_top();
void s_refresh_bot();
void s_refresh_entzone();
void s_place_ptr();
void s_refresh_status(int line);
void s_refresh_description(int line);
void s_ref_entry(int line, int jump);
void s_rub_ptr();
void s_refresh();
int s_initscreen();
void s_ref_status_onpage(long ent);
void s_resize_win();
