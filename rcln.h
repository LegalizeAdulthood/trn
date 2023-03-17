/* rcln.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* if true, silence is golden (universal scan mode) */
EXT bool toread_quiet INIT(false);

#define ST_STRICT	false
#define ST_LAX		true

void rcln_init();
#ifdef CATCHUP
void catch_up(NGDATA *np, int leave_count, int output_level);
#endif
int addartnum(DATASRC *dp, ART_NUM artnum, char *ngnam);
#ifdef MCHASE
void subartnum(DTASRC *dp, ART_NUM artnum, char *ngnam);
#endif
void prange(char *, ART_NUM, ART_NUM);
void set_toread(NGDATA *np, bool lax_high_check);
void checkexpired(NGDATA *np, ART_NUM a1st);
bool was_read_group(DATASRC *dp, ART_NUM artnum, char *ngnam);
