/* search.c
*/

/* This file implements a regular expression compatibility layer between the
 * PCRE package (perl-compatible regular expressions) and trn.  trn's classic
 * regexp format is based on what are called "basic" regular expressions,
 * while PCRE implements perl regular expressions, which are a superset of
 * "extended" regular expressions.  This module provides a trn-style API
 * into PCRE, and converts regexps from trn format to pcre format.  Additional
 * syntax is also supported to invert the sense of a regexp and to allow
 * using the PCRE form in the original regexp.
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "pcre/pcre.h"
#include "search.h"
#include "search.ih"

#ifndef isascii
#define isascii(x)	(((x) & ~0x7f) == 0)
#endif

/* Convert a plain string into a regexp by quoting all of the metacharacters. */
static void
cvt_str2pcre(from, to)
char* from;
char* to;
{
	while (*from) {
		/* Just quote all punctuation. */
		if (isascii(*from) && ispunct(*from)) {
			*to++ = '\\';
		}
		*to++ = *from++;
	}
	*to = 0;
}

/* Convert a trn character class into a perl character class. */
static char*
cvt_trn2pcre_cclass(from, to)
char* from;
char* to;
{
	*to++ = *from++;		/* Opening [ */
	if ('^' == *from)
		*to++ = *from++;	/* Negated character class */

	/* If the trn cclass contains a literal ']' it'll be next.  pcre
	 * backslashes the ] to make it literal instead.
	 */
	if (']' == *from) {
		*to++ = '\\'; *to++ = *from++;
	}

	while (*from) {
		if (']' == *from) {
			*from++; break;
		}
		else if ('\\' == *from) {
			/* To trn this is a literal '\' */
			*to++ = '\\'; *to++ = *from++;
		}
		else {
			/* plain old character */
			*to++ = *from++;
		}
	}
	*to++ = ']'; *to = 0;
	return from;
}


/* Convert a trn regexp into a perl regexp. */
static void
cvt_trn2pcre(from, to)
char* from;
char* to;
{
	while (*from) {
		if (isascii(*from) && !ispunct(*from)) {
			/* Common case--letters, digits, white space */
			*to++ = *from++;
		}
		else if ('[' == *from) {
			/* Character class. */
			from = cvt_trn2pcre_cclass(from, to);
			to += strlen(to);
		}
		else if ('\\' == *from) {
			/* Backslash.  Look at the next character */
			*from++;
			if (index(".[$*", *from)) {
				/* Not special to either when escaped */
				*to++ = '\\'; *to++ = *from++;
			}
			else if (index("wWbB123456789", *from)) {
				/* Special to both when escaped */
				*to++ = '\\'; *to++ = *from++;
			}
			else  {
				/* No reason for trn to escape this. Don't
				 * escape it for pcre.
				 */
				*to++ = *from++;
			}
		}
		else if (index("?|()+{", *from)) {
			/* Not special to trn but special to pcre. */
			*to++ = '\\';
			*to++ = *from++;
		}
		else {
			/* Some other nonspecial character */
			*to++ = *from++;
		}
	}
	*to = 0;
}


/* Check the beginning of a regexp pattern for "metapattern flags". These
 * flags will be in the form "$$X$$", where X is the string of flags.
 * Legal flags are:
 *
 * c	Force case-insensitive matching
 * C	Force case-sensitive matching
 * !	Invert the sense of the pattern. It will mismatch when it should
 *	match and vice versa.
 * st	Pattern is in trn syntax
 * sp	Pattern is in perl syntax
 * ss	Pattern is a plain substring
 * sw	Pattern is in filename wildcard syntax (not implemented)
 *
 * If an error is detected in the metapattern a negative number is returned which,
 * when negated into a positive number, is the index of an error message in the
 * global mp_errors array.  Otherwise, pattern_flags() returns the length of the
 * metapattern sequence (including one or two trailing $ characters); if there is
 * no metapattern sequence 0 is returned. The value pointed to by flags is modified
 * to reflect flags found in the metapattern.
 */
static int
pattern_flags(strp, flags)
char* strp;
int* flags;
{
	int i = 0;
	if (('$' == strp[0]) && ('$' == strp[1])) {
		i = 2;
		while ('$' != strp[i]) {
			switch(strp[i++]) {
			case 'c': /* case-insensitive */
				*flags |= SRCH_FOLD;
				break;
			case 'C': /* case-sensitive */
				*flags &= ~SRCH_FOLD;
				break;
			case '!': /* Invert match/nomatch */
				*flags |= SRCH_INVERT;
				break;
			case 's':
				switch (strp[i++]) {
				case 'r':
				case 't':
					*flags = (*flags & SRCH_SYN_MASK) |
						SRCH_SYN_RN;
					break;
				case 'p':
					*flags = (*flags & SRCH_SYN_MASK) |
						SRCH_SYN_PERL;
					break;
				case 's':
					*flags = (*flags & SRCH_SYN_MASK) |
						SRCH_SYN_SUBSTRING;
					break;
				case 'w':
					return ERR_MPSYN_WILD_UNSUPP;
				default:
					return ERR_MPSYN_UNKNOWN;
				}
				break;
			default:
				return ERR_MPFLAG_UNKNOWN;
			}
		}

		/* The metapattern flags should end with two '$' characters.
		 * We know strp[i] is a '$'; eat the next character if it
		 * is one too.
		 */
		if ('$' == strp[++i]) i++;
	}
	return i;
}


/* Compile strp into compex.  Return NULL for success or a pointer to
 * an error message on failure. Syntax is the default syntax for the regexp if
 * it has no metapattern flags; see the SRCH_SYN flags in search.h.
 * Fold is nonzero to do case-insensitive matching.
 *
 * [With the old trn regexp engine, the third argument was 0 for substrings and 1
 * (well, nonzero) for trn's regexp format. The new usage is an extension, but
 * 0 and 1 still have their old meanings.]
 */
char*
compile(compex, strp, syntax, fold)
COMPEX* compex;
char* strp;
int syntax;
int fold;
{
	int ret;
	char* pcre_err;

	/* Make sure the structure is initialized */
	if (compex->re)
		free_compex(compex);

	/* Initialize flags based on our arguments */
	compex->flags = syntax;
	if (fold)
		compex->flags |= SRCH_FOLD;

	/* Check for metapattern flags in the pattern */
	ret = pattern_flags(strp, &compex->flags);
	if (ret < 0)
		return mp_errors[-ret];
	strp += ret;

	/* Allocate space for the converted regexp.  Worst case is each
	 * char in the original becomes two chars in the new one.
	 */
	compex->re = safemalloc((2 * strlen(strp)) + 1);

	/* Convert the regexp to pcre syntax */
	switch (compex->flags & ~SRCH_SYN_MASK) {
	case SRCH_SYN_RN:
		cvt_trn2pcre(strp, compex->re);
		break;
	case SRCH_SYN_SUBSTRING:
		cvt_str2pcre(strp, compex->re);
		break;
	case SRCH_SYN_PERL:
		strcpy(compex->re, strp);
		break;
	default:
		free_compex(compex);
		return mp_errors[-ERR_MPSYN_UNKNOWN];
	}

	/* Compile it */
	compex->cre = pcre_compile(compex->re,
		((compex->flags & SRCH_FOLD) ? PCRE_CASELESS : 0) | PCRE_DOTALL,
		(const char **) &pcre_err, &ret, 0);
	if (!compex->cre) {
		free_compex(compex);
		return pcre_err;
	}

	/* Study it -- Most trn regexps get many times after being compiled */
	compex->extra = pcre_study(compex->cre, 0, (const char **) &pcre_err);
	if (!compex->extra && pcre_err) {
		free_compex(compex);
		return pcre_err;
	}

	/* Find out how many "capturing subpatterns" the pattern has, and
	 * allocate an appropriate ovector.
	 */
	if (0 != pcre_fullinfo(compex->cre, compex->extra,
		PCRE_INFO_CAPTURECOUNT, &compex->ovecsize)) {
		free_compex(compex);
		return mp_errors[-ERR_MP_INTERNAL];
	}
	compex->ovector = (int *) safemalloc(sizeof(int) *
		(compex->ovecsize + 3) * 3);

	/* All is well. */
	return NULL;
}


char*
execute(compex, addr)
COMPEX* compex;
char* addr;
{
	do {
		compex->ovalid = pcre_exec(compex->cre, compex->extra,
			addr, strlen(addr), 0,
			0, compex->ovector, compex->ovecsize);
		if (0 == compex->ovalid) {
			compex->ovecsize += 3*sizeof(int);
			compex->ovector = (int *) saferealloc(
				(void *) compex->ovector,
				compex->ovecsize);
		}
	} while (0 == compex->ovalid);

	/* Invert success/failure if requested. */
	if (compex->flags & SRCH_INVERT) {
		if (PCRE_ERROR_NOMATCH == compex->ovalid) {
			/* Turn a failure into a success */
			compex->ovalid = 1;
			compex->ovector[0] = 0;
			compex->ovector[1] = strlen(addr);
		}
		else if (compex->ovalid > 0) {
			/* Turn a success into a failure */
			compex->ovalid = PCRE_ERROR_NOMATCH;
		}
	}
		
	if (compex->ovalid > 0) {
		int i, size;

		size = compex->ovector[1] - compex->ovector[0];
		compex->match = saferealloc(compex->match, size + 1);
		memcpy(compex->match, addr + compex->ovector[0], size);
		compex->match[size] = 0;

		/* Offsets in ovector are all based on the original string. Fix
		 * them to work with this copy of the match substring we just made.
		 */
		for (i = compex->ovalid * 2; i--; )
			compex->ovector[i] -= compex->ovector[0];
	}
	return compex->ovalid > 0 ? compex->match : NULL;
}

/* Return a pointer to a null-terminated string containing the nth
 * substring match. If n is zero, return the last substring match.
 */
char*
getbracket(compex, n)
COMPEX* compex;
int n;
{
	if (!compex->match)
		/* We haven't matched anything yet */
		return NULL;
	else if (compex->ovalid == 1)
		/* Pattern didn't contain any substring patterns */
		return NULL;
	else if (n > compex->ovalid)
		/* Pattern didn't have this many substring patterns */
		return "";
	else {
		static char *buf;

		if (0 == n)
			n = compex->ovalid;
		safefree(buf);
		pcre_get_substring(compex->match, compex->ovector,
			compex->ovalid, n, (const char**)&buf);
		return buf;
	}
}

void
search_init()
{
	/* Nothing to do */
}


void
init_compex(compex)
COMPEX* compex;
{
	compex->re = NULL;
	compex->cre = NULL;
	compex->extra = NULL;
	compex->ovector = NULL;
	compex->match = NULL;
}


void
free_compex(compex)
COMPEX* compex;
{
	safefree(compex->re);
	safefree(compex->cre);
	safefree(compex->extra);
	safefree(compex->ovector);
	safefree(compex->match);
	init_compex(compex);
}
