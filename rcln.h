/* rcln.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* if true, silence is golden (universal scan mode) */
EXT bool toread_quiet INIT(false);

#define ST_STRICT	false
#define ST_LAX		true

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

void rcln_init _((void));
#ifdef CATCHUP
void catch_up _((NGDATA*,int,int));
#endif
int addartnum _((DATASRC*,ART_NUM,char*));
#ifdef MCHASE
void subartnum _((DATASRC*,ART_NUM,char*));
#endif
void prange _((char*,ART_NUM,ART_NUM));
void set_toread(NGDATA *np, bool lax_high_check);
void checkexpired _((NGDATA*,ART_NUM));
bool was_read_group _((DATASRC*,ART_NUM,char*));
