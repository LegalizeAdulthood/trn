/* trn/rt-wumpus.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RT_WUMPUS_H
#define TRN_RT_WUMPUS_H

#include <config/typedef.h>

struct Article;

extern int g_max_tree_lines;

void init_tree();
Article *get_tree_artp(int x, int y);
int tree_puts(char *orig_line, ArticleLine header_line, int is_subject);
int  finish_tree(ArticleLine last_line);
void entire_tree(Article *ap);
char thread_letter(Article *ap);

#endif
