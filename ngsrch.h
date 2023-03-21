/* ngsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef NGSRCH_H
#define NGSRCH_H

enum
{
    NGS_ABORT = 0,
    NGS_FOUND = 1,
    NGS_INTR = 2,
    NGS_NOTFOUND = 3,
    NGS_ERROR = 4,
    NGS_DONE = 5
};

extern bool g_ng_doempty; /* search empty newsgroups? */

void ngsrch_init();
int ng_search(char *patbuf, int get_cmd);
bool ng_wanted(NGDATA *np);
char *ng_comp(COMPEX *compex, const char *pattern, bool RE, bool fold);

#endif
