/* rcln.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef RCLN_H
#define RCLN_H

struct DATASRC;
struct NGDATA;

/* if true, silence is golden (universal scan mode) */
extern bool toread_quiet;

#define ST_STRICT	false
#define ST_LAX		true

void rcln_init();
void catch_up(NGDATA *np, int leave_count, int output_level);
int addartnum(DATASRC *dp, ART_NUM artnum, const char *ngnam);
#ifdef MCHASE
void subartnum(DTASRC *dp, ART_NUM artnum, char *ngnam);
void prange(char *where, ART_NUM min, ART_NUM max);
#endif
void set_toread(NGDATA *np, bool lax_high_check);
void checkexpired(NGDATA *np, ART_NUM a1st);
bool was_read_group(DATASRC *dp, ART_NUM artnum, char *ngnam);

#endif
