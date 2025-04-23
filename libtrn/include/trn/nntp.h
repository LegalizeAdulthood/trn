/* trn/nntp.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_NNTP_H
#define TRN_NNTP_H

#include "addng.h"

#include <config/typedef.h>

#include <ctime>

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

int             nntp_list(const char *type, const char *arg, int len);
void            nntp_finish_list();
int             nntp_group(const char *group, NewsgroupData *gp);
int             nntp_stat(ArticleNum art_num);
ArticleNum      nntp_stat_id(const char *msg_id);
ArticleNum      nntp_next_art();
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
#if 0
int nntp_rover();
#endif
ArticleNum nntp_find_real_art(ArticleNum after);
char      *nntp_art_name(ArticleNum art_num, bool allocate);
char      *nntp_tmp_name(int ndx);
int        nntp_handle_nested_lists();
int        nntp_handle_timeout();
void       nntp_server_died(DataSource *dp);
#ifdef SUPPORT_XTHREAD
long nntp_read_check();
long nntp_read(char *buf, long n);
#endif

#endif
