/* trn/nntp.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NNTP_H
#define TRN_NNTP_H

#include <config/typedef.h>
#include <trn/addng.h>

#include <ctime>
#include <string_view>

struct DataSource;
struct NewsgroupData;

enum FinishBodyMode
{
    FB_BACKGROUND = 0,
    FB_OUTPUT = 1,
    FB_SILENT = 2,
    FB_DISCARD = 3
};

enum
{
    MAX_NNTP_ARTICLES = 10
};

int             nntp_list(std::string_view type, std::string_view arg);
void            nntp_finish_list();
int             nntp_group(std::string_view group, NewsgroupData *gp);
int             nntp_stat(ArticleNum art_num);
ArticleNum      nntp_stat_id(std::string_view msg_id);
int             nntp_header(ArticleNum art_num);
void            nntp_body(ArticleNum art_num);
ArticlePosition nntp_art_size();
int             nntp_finish_body(FinishBodyMode bmode);
int             nntp_seek_art(ArticlePosition pos);
ArticlePosition nntp_tell_art();
char           *nntp_read_art(char *s, int limit);
std::time_t     nntp_time();
int             nntp_new_groups(std::time_t t);
int             nntp_art_nums();
ArticleNum      nntp_find_real_art(ArticleNum after);
char           *nntp_art_name(ArticleNum art_num, bool allocate);
char           *nntp_tmp_name(int ndx);
int             nntp_handle_nested_lists();
int             nntp_handle_timeout();

#endif
