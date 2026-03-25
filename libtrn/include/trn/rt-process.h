/* trn/rt-process.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RT_PROCESS_H
#define TRN_RT_PROCESS_H

#include <config/typedef.h>
#include <trn/hash.h>

struct Article;
struct Subject;

Article *allocate_article(ArticleNum artnum);
int      msg_id_cmp(const char *key, int key_len, HashDatum data);
Article *get_article(char *msgid);
void     merge_threads(Subject *s1, Subject *s2);
void     fix_msg_id(char *msgid);
void     unlink_child(Article *child);

#endif
