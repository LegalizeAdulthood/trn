/* rcln.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* if true, silence is golden (universal scan mode) */
EXT bool toread_quiet INIT(false);

#define ST_STRICT	false
#define ST_LAX		true

void rcln_init(void);
#ifdef CATCHUP
void catch_up(NGDATA *, int, int);
#endif
int addartnum(DATASRC *, ART_NUM, char *);
#ifdef MCHASE
void subartnum(DATASRC *, ART_NUM, char *);
#endif
void prange(char *, ART_NUM, ART_NUM);
void set_toread(NGDATA *np, bool lax_high_check);
void checkexpired(NGDATA *, ART_NUM);
bool was_read_group(DATASRC *, ART_NUM, char *);
