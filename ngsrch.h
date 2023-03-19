/* ngsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#define NGS_ABORT 0
#define NGS_FOUND 1
#define NGS_INTR 2
#define NGS_NOTFOUND 3
#define NGS_ERROR 4
#define NGS_DONE 5

EXT bool ng_doempty INIT(false);	/* search empty newsgroups? */

void ngsrch_init();
int ng_search(char *, int);
bool ng_wanted(NGDATA *np);
char *ng_comp(COMPEX *compex, char *pattern, bool RE, bool fold);
