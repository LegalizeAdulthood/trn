/* ngsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#ifdef NGSEARCH
#define NGS_ABORT 0
#define NGS_FOUND 1
#define NGS_INTR 2
#define NGS_NOTFOUND 3
#define NGS_ERROR 4
#define NGS_DONE 5

EXT bool ng_doempty INIT(false);	/* search empty newsgroups? */
#endif

void ngsrch_init(void);
#ifdef NGSEARCH
int ng_search(char *, int);
bool ng_wanted(NGDATA *);
#endif
char *ng_comp(COMPEX *compex, char *pattern, bool RE, bool fold);
