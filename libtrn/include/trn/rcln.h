/* trn/rcln.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RCLN_H
#define TRN_RCLN_H

#include <config/typedef.h>

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
int add_art_num(DataSource *dp, ArticleNum art_num, const char *newsgroup_name);
#ifdef MCHASE
void sub_art_num(DataSource *dp, ArticleNum art_num, char *ng_name);
void prange(char *where, ArticleNum min_num, ArticleNum max_num);
#endif
bool was_read_group(DataSource *dp, ArticleNum artnum, char *ngnam);

#endif
