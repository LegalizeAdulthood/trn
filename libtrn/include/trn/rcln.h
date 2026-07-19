/* trn/rcln.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RCLN_H
#define TRN_RCLN_H

#include <config/typedef.h>

#include <string_view>

struct DataSource;
struct NewsgroupData;

// if true, silence is golden (universal scan mode)
extern bool g_to_read_quiet;

enum : bool
{
    ST_STRICT = false,
    ST_LAX = true
};

void rcln_init();
int  add_art_num(DataSource *dp, ArticleNum art_num, std::string_view newsgroup_name);
#ifdef MCHASE
void sub_art_num(DataSource *dp, ArticleNum art_num, std::string_view newsgroup_name);
#endif
bool was_read_group(ArticleNum artnum, std::string_view ngnam);

#endif
