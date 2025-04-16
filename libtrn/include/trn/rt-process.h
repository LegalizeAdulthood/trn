/* trn/rt-process.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RT_PROCESS_H
#define TRN_RT_PROCESS_H

#include <config/typedef.h>

#include "trn/hash.h"

struct Article;
struct Subject;

Article *allocate_article(ArticleNum artnum);
int msgid_cmp(const char *key, int keylen, HashDatum data);
bool valid_article(Article *article);
Article *get_article(char *msgid);
void thread_article(Article *article, char *references);
void rover_thread(Article *article, char *s);
void link_child(Article *child);
void merge_threads(Subject *s1, Subject *s2);

#endif
