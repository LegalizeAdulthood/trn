/* artsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#ifndef NBRA
#include "search.h"
#endif

#ifdef ARTSEARCH

enum
{
    SRCH_ABORT = 0,
    SRCH_INTR = 1,
    SRCH_FOUND = 2,
    SRCH_NOTFOUND = 3,
    SRCH_DONE = 4,
    SRCH_SUBJDONE = 5,
    SRCH_ERROR = 6
};

#endif

EXT char* lastpat INIT(nullstr);	/* last search pattern */
#ifdef ARTSEARCH
EXT COMPEX sub_compex;		/* last compiled subject search */
EXT COMPEX art_compex;		/* last compiled normal search */
EXT COMPEX* bra_compex INIT(&(art_compex)); /* current compex with brackets */

enum
{
    ARTSCOPE_SUBJECT = 0,
    ARTSCOPE_FROM = 1,
    ARTSCOPE_ONEHDR = 2,
    ARTSCOPE_HEAD = 3,
    ARTSCOPE_BODY_NOSIG = 4,
    ARTSCOPE_BODY = 5,
    ARTSCOPE_ARTICLE = 6
};

EXT char scopestr[] INIT("sfHhbBa");
EXT int art_howmuch;		/* search scope */
EXT int art_srchhdr;		/* specific header number to search */
EXT bool art_doread;		/* search read articles? */
#endif

void artsrch_init();
#ifdef ARTSEARCH
int art_search(char *, int, int);
bool wanted(COMPEX *compex, ART_NUM artnum, char_int scope);
#endif
