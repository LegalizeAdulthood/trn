/* This file is Copyright 1993 by Clifford A. Adams */
/* smisc.c
 *
 * Lots of misc. stuff.
 */

#include "EXTERN.h"
#include "common.h"
#ifdef SCAN
#include "hash.h"
#include "cache.h"
#include "rt-util.h"	/* for from-compression stuff */
#include "intrp.h"
#include "term.h"
#include "util.h"
#include "scan.h"
#include "sdisp.h"
#include "sorder.h"
#include "charsubst.h"
#ifdef SCAN_ART
#include "samisc.h"
#include "sadesc.h"
#endif
#include "INTERN.h"
#include "smisc.h"

bool s_eligible(long ent)
{
    switch (s_cur_type) {
#ifdef SCAN_ART
      case S_ART:
	return sa_eligible(ent);
#endif
      default:
	printf("s_eligible: current type is bad!\n") FLUSH;
	return false;
    }
}

void s_beep()
{
    putchar(7);
    fflush(stdout);
}

char *s_get_statchars(long ent, int line)
{
    if (s_status_cols == 0)
	return nullstr;
    switch (s_cur_type) {
#ifdef SCAN_ART
      case S_ART:
	return sa_get_statchars(ent,line);
#endif
      default:
	return nullptr;
    }
}

char *s_get_desc(long ent, int line, bool trunc)
{
    switch (s_cur_type) {
#ifdef SCAN_ART
      case S_ART:
	return sa_get_desc(ent,line,trunc);
#endif
      default:
	return nullptr;
    }
}

int s_ent_lines(long ent)
{
    switch (s_cur_type) {
#ifdef SCAN_ART
      case S_ART:
	return sa_ent_lines(ent);
#endif
      default:
	return 1;
    }
}
#endif /* SCAN */
