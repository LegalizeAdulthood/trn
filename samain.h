/* This file Copyright 1992,1993 by Clifford A. Adams */
/* samain.h
 *
 * Main routines for article-scan mode.
 */

/* sa_flags character bitmap:
 * 0: If set, the article is "marked" (for reading).
 * 1: If set, the article is "selected" (for zoom mode display).
 * 2: Secondary selection--not currently used.
 * 3: If set, the author of this article influenced its score.
 */
#define sa_mark(a) (sa_ents[a].sa_flags |= 1)
#define sa_clearmark(a) (sa_ents[a].sa_flags &= 0xfe)
#define sa_marked(a) (sa_ents[a].sa_flags & 1)

#define sa_select1(a) (sa_ents[a].sa_flags |= 2)
#define sa_clearselect1(a) (sa_ents[a].sa_flags &= 0xfd)
#define sa_selected1(a) (sa_ents[a].sa_flags & 2)

/* sa_select2 is not currently used */
#define sa_select2(a) (sa_ents[a].sa_flags |= 4)
#define sa_clearselect2(a) (sa_ents[a].sa_flags &= 0xfb)
#define sa_selected2(a) (sa_ents[a].sa_flags & 4)

#define sa_setauthscored(a) (sa_ents[a].sa_flags |= 8)
#define sa_clearauthscored(a) (sa_ents[a].sa_flags &= 0xf7)
#define sa_authscored(a) (sa_ents[a].sa_flags & 8)

/* misc. buffer */
EXT char sa_buf[LBUFLEN];

/* true if in "zoom" (display only selected) mode */
EXT bool sa_mode_zoom INIT(false);

/* if true, the already-read articles have been added to the order arrays */
EXT bool sa_order_read INIT(false);

/* contains the scan-context number for the current article scan */
EXT int sa_scan_context INIT(-1);

void sa_init();
void sa_init_ents();
void sa_clean_ents();
long sa_add_ent(ART_NUM artnum);
void sa_cleanmain();
void sa_growarts(long oldlast, long last);
void sa_init_context();
bool sa_initarts();
void sa_initmode();
void sa_lookahead();
long sa_readmarked_elig();
