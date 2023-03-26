/* rt-wumpus.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RT_WUMPUS_H
#define TRN_RT_WUMPUS_H

struct ARTICLE;

void init_tree();
ARTICLE *get_tree_artp(int x, int y);
int tree_puts(char *orig_line, ART_LINE header_line, int is_subject);
int  finish_tree(ART_LINE last_line);
void entire_tree(ARTICLE *ap);
char thread_letter(ARTICLE *ap);

#endif
