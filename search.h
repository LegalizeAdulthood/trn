/* search.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#ifndef NBRA
#define	NBRA	10		/* the maximum number of meta-brackets in an
				   RE -- \( \) */
#define NALTS	10		/* the maximum number of \|'s */
 
struct compex {
    char* expbuf;		/* The compiled search string */
    int eblen;			/* Length of above buffer */
    char* alternatives[NALTS+1];/* The list of \| seperated alternatives */
    char* braslist[NBRA];	/* RE meta-bracket start list */
    char* braelist[NBRA];	/* RE meta-bracket end list */
    char* brastr;		/* saved match string after execute() */
    char nbra;			/* The number of meta-brackets int the most
				   recenlty compiled RE */
    bool do_folding;		/* fold upper and lower case? */
};
#endif

void search_init(void);
void init_compex(COMPEX *);
void free_compex(COMPEX *);
char *getbracket(COMPEX *, int);
void case_fold(bool which);
char *compile(COMPEX *compex, char *strp, bool RE, bool fold);
char *grow_eb(COMPEX *, char *, char **);
char *execute(COMPEX *, char *);
bool advance(COMPEX *, char *, char *);
bool backref(COMPEX *, int, char *);
bool cclass(char *, int, int);
