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
void catch_up(NewsgroupData *np, int leave_count, int output_level);
int add_art_num(DataSource *dp, ArticleNum art_num, const char *newsgroup_name);
#ifdef MCHASE
void sub_art_num(DTASRC *dp, ART_NUM artnum, char *ngnam);
void prange(char *where, ART_NUM min, ART_NUM max);
#endif
void set_to_read(NewsgroupData *np, bool lax_high_check);
void check_expired(NewsgroupData *np, ArticleNum a1st);
bool was_read_group(DataSource *dp, ArticleNum artnum, char *ngnam);

#endif
