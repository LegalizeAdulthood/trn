/* bits.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT int dmcount INIT(0);

void bits_init();
void rc_to_bits();
bool set_firstart(char *);
void bits_to_rc();
void find_existing_articles();
void onemore(ARTICLE *);
void oneless(ARTICLE *);
void oneless_artnum(ART_NUM);
void onemissing(ARTICLE *);
void unmark_as_read(ARTICLE *);
void set_read(ARTICLE *);
void delay_unmark(ARTICLE *);
void mark_as_read(ARTICLE *);
void mark_missing_articles();
void check_first(ART_NUM);
void yankback();
bool chase_xrefs(bool until_key);
