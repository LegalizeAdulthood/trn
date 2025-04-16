/* trn/rcln.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RCLN_H
#define TRN_RCLN_H

#include <config/typedef.h>

struct DataSource;
struct NewsgroupData;

/* if true, silence is golden (universal scan mode) */
extern bool g_toread_quiet;

enum : bool
{
    ST_STRICT = false,
    ST_LAX = true
};

void rcln_init();
void catch_up(NewsgroupData *np, int leave_count, int output_level);
int addartnum(DataSource *dp, ART_NUM artnum, const char *ngnam);
#ifdef MCHASE
void subartnum(DTASRC *dp, ART_NUM artnum, char *ngnam);
void prange(char *where, ART_NUM min, ART_NUM max);
#endif
void set_toread(NewsgroupData *np, bool lax_high_check);
void checkexpired(NewsgroupData *np, ART_NUM a1st);
bool was_read_group(DataSource *dp, ART_NUM artnum, char *ngnam);

#endif
