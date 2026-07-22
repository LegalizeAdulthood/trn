/* trn/rt-process.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_PROCESS_H
#define TRN_RT_PROCESS_H

#include <config/typedef.h>
#include <trn/hash.h>

#include <string>
#include <string_view>

struct Article;
struct Subject;

HashDatum   make_pending_msg_id(std::string_view msg_id, unsigned flags);
std::string_view hash_msg_id_view(HashDatum data);
std::string take_pending_msg_id(HashDatum *data);
void        free_pending_msg_id(HashDatum *data);
int         msg_id_cmp(std::string_view key, HashDatum data);
Article    *get_article(std::string_view msgid);
void        merge_threads(Subject *s1, Subject *s2);
std::string fix_msg_id(std::string_view msgid);
void        unlink_child(Article *child);

#endif
