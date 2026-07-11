/* trn/rt-process.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_PROCESS_H
#define TRN_RT_PROCESS_H

#include <config/typedef.h>
#include <trn/hash.h>

struct Article;
struct Subject;

int      msg_id_cmp(std::string_view key, HashDatum data);
Article *get_article(char *msgid);
void     merge_threads(Subject *s1, Subject *s2);
void     fix_msg_id(char *msgid);
void     unlink_child(Article *child);

#endif
