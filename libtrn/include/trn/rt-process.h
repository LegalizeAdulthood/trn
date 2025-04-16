/* trn/rt-process.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RT_PROCESS_H
#define TRN_RT_PROCESS_H

#include <config/typedef.h>

#include "trn/hash.h"

struct ARTICLE;
struct Subject;

ARTICLE *allocate_article(ART_NUM artnum);
int msgid_cmp(const char *key, int keylen, HASHDATUM data);
bool valid_article(ARTICLE *article);
ARTICLE *get_article(char *msgid);
void thread_article(ARTICLE *article, char *references);
void rover_thread(ARTICLE *article, char *s);
void link_child(ARTICLE *child);
void merge_threads(Subject *s1, Subject *s2);

#endif
