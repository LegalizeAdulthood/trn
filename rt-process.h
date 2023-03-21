/* rt-process.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef RT_PROCESS_H
#define RT_PROCESS_H

ARTICLE *allocate_article(ART_NUM artnum);
int msgid_cmp(const char *key, int keylen, HASHDATUM data);
bool valid_article(ARTICLE *article);
ARTICLE *get_article(char *msgid);
void thread_article(ARTICLE *article, char *references);
void rover_thread(ARTICLE *article, char *s);
void link_child(ARTICLE *child);
void merge_threads(SUBJECT *s1, SUBJECT *s2);

#endif
