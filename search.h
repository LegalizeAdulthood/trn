/* search.h
*/

/* Search string syntax values; third argument to compile. */
#define SRCH_SYN_SUBSTRING      (0)             /* Substring, no wildcards */
#define SRCH_SYN_RN             (1)             /* Classic rn/trn syntax */
#define SRCH_SYN_PERL           (2)             /* Perl (pcre) syntax */
#define SRCH_SYN_WILD           (3)             /* Filename wildards */


struct compex {
	char *re;		/* Uncompiled regular expression */
	void *cre;		/* Compiled search string */
	void *extra;		/* pcre_study() return value */
	int *ovector;		/* pcre ovector (see pcre_exec()) */
	int ovecsize;		/* Size of ovector */
	int flags;		/* Metapattern flags */
	int ovalid;		/* Return value of last pcre_exec() */
	char *match;		/* Match string from last pcre_exec() */
};

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

char* compile _((COMPEX*,char*,int,int));
char* execute _((COMPEX*,char*));
char* getbracket _((COMPEX*,int));
void search_init _((void));
void init_compex _((COMPEX*));
void free_compex _((COMPEX*));
