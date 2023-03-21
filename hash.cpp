/* hash.c
*/
/* This file is an altered version of a set of hash routines by
** Geoffrey Collyer.  See the end of the file for his copyright.
*/

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "final.h"
#include "hash.h"

#define BADTBL(tbl)	((tbl) == nullptr || (tbl)->ht_magic != HASHMAG)

#define HASHMAG  ((char)0257)

struct HASHENT
{
    HASHENT* he_next;		/* in hash chain */
    HASHDATUM he_data;
    int he_keylen;		/* to help verify a match */
};

struct HASHTABLE
{
    HASHENT** ht_addr;		/* array of HASHENT pointers */
    unsigned ht_size;
    char ht_magic;
    HASHCMPFUNC ht_cmp;
};

static HASHENT **hashfind(HASHTABLE *tbl, const char *key, int keylen);
static unsigned hash(const char *key, int keylen);
static int default_cmp(const char *key, int keylen, HASHDATUM data);
static HASHENT *healloc();
static void hefree(HASHENT *hp);

/* CAA: In the future it might be a good idea to preallocate a region
 *      of hashents, since allocation overhead on some systems will be as
 *      large as the structure itself.
 */
/* grab this many hashents at a time (under 1024 for malloc overhead) */
enum
{
    HEBLOCKSIZE = 1000
};

/* define the following if you actually want to free the hashents
 * You probably don't want to do this with the usual malloc...
 */
/* #define HASH_FREE_ENTRIES */

/* CAA: increased from 600.  Each threaded article requires at least
 *      one hashent, and various newsgroup hashes can easily get large.
 */
/* tunable parameters */
enum
{
    RETAIN = 1000 /* retain & recycle this many HASHENTs */
};

static HASHENT *s_hereuse{};
static int s_reusables{};

/* size - a crude guide to size */
HASHTABLE *hashcreate(unsigned size, HASHCMPFUNC cmpfunc)
{
    HASHTABLE* tbl;
    /* allocate HASHTABLE and (HASHENT*) array together to reduce the
    ** number of malloc calls. */
    struct alignalloc {
	HASHTABLE ht;
	HASHENT* hepa[1];	/* longer than it looks */
    }* aap;

    if (size < 1)		/* size < 1 is nonsense */
	size = 1;
    aap = (struct alignalloc*)
	safemalloc(sizeof *aap + (size-1)*sizeof (HASHENT*));
    memset((char*)aap,0,sizeof *aap + (size-1)*sizeof (HASHENT*));
    tbl = &aap->ht;
    tbl->ht_size = size;
    tbl->ht_magic = HASHMAG;
    tbl->ht_cmp = cmpfunc == nullptr ? default_cmp : cmpfunc;
    tbl->ht_addr = aap->hepa;
    return tbl;
}

/* Free all the memory associated with tbl, erase the pointers to it, and
** invalidate tbl to prevent further use via other pointers to it.
*/
void hashdestroy(HASHTABLE *tbl)
{
    unsigned idx;
    HASHENT* hp;
    HASHENT* next;
    HASHENT** hepp;
    int tblsize;

    if (BADTBL(tbl))
	return;
    tblsize = tbl->ht_size;
    hepp = tbl->ht_addr;
    for (idx = 0; idx < tblsize; idx++) {
	for (hp = hepp[idx]; hp != nullptr; hp = next) {
	    next = hp->he_next;
	    hp->he_next = nullptr;
	    hefree(hp);
	}
	hepp[idx] = nullptr;
    }
    tbl->ht_magic = 0;			/* de-certify this table */
    tbl->ht_addr = nullptr;
    free((char*)tbl);
}

void hashstore(HASHTABLE *tbl, const char *key, int keylen, HASHDATUM data)
{
    HASHENT* hp;
    HASHENT** nextp;

    nextp = hashfind(tbl, key, keylen);
    hp = *nextp;
    if (hp == nullptr) {			/* absent; allocate an entry */
	hp = healloc();
	hp->he_next = nullptr;
	hp->he_keylen = keylen;
	*nextp = hp;			/* append to hash chain */
    }
    hp->he_data = data;		/* supersede any old data for this key */
}

void hashdelete(HASHTABLE *tbl, const char *key, int keylen)
{
    HASHENT* hp;
    HASHENT** nextp;

    nextp = hashfind(tbl, key, keylen);
    hp = *nextp;
    if (hp == nullptr)			/* absent */
	return;
    *nextp = hp->he_next;		/* skip this entry */
    hp->he_next = nullptr;
    hp->he_data.dat_ptr = nullptr;
    hefree(hp);
}

HASHENT** slast_nextp;
int slast_keylen;

/* data corresponding to key */
HASHDATUM hashfetch(HASHTABLE *tbl, const char *key, int keylen)
{
    HASHENT* hp;
    HASHENT** nextp;
    static HASHDATUM errdatum = { nullptr, 0 };

    nextp = hashfind(tbl, key, keylen);
    slast_nextp = nextp;
    slast_keylen = keylen;
    hp = *nextp;
    if (hp == nullptr)			/* absent */
	return errdatum;
    return hp->he_data;
}

void hashstorelast(HASHDATUM data)
{
    HASHENT* hp;

    hp = *slast_nextp;
    if (hp == nullptr) {			/* absent; allocate an entry */
	hp = healloc();
	hp->he_next = nullptr;
	hp->he_keylen = slast_keylen;
	*slast_nextp = hp;		/* append to hash chain */
    }
    hp->he_data = data;		/* supersede any old data for this key */
}

/* Visit each entry by calling nodefunc at each, with keylen, data,
** and extra as arguments.
*/
void hashwalk(HASHTABLE *tbl, HASHWALKFUNC nodefunc, int extra)
{
    unsigned idx;
    HASHENT* hp;
    HASHENT* next;
    HASHENT** hepp;
    int tblsize;

    if (BADTBL(tbl))
	return;
    hepp = tbl->ht_addr;
    tblsize = tbl->ht_size;
    for (idx = 0; idx < tblsize; idx++) {
	slast_nextp = &hepp[idx];
	for (hp = *slast_nextp; hp != nullptr; hp = next) {
	    next = hp->he_next;
	    if ((*nodefunc)(hp->he_keylen, &hp->he_data, extra) < 0) {
		*slast_nextp = next;
		hp->he_next = nullptr;
		hefree(hp);
	    }
	    else
		slast_nextp = &hp->he_next;
	}
    }
}

/* The returned value is the address of the pointer that refers to the
** found object.  Said pointer may be nullptr if the object was not found;
** if so, this pointer should be updated with the address of the object
** to be inserted, if insertion is desired.
*/
static HASHENT **hashfind(HASHTABLE *tbl, const char *key, int keylen)
{
    HASHENT* hp;
    HASHENT* prevhp = nullptr;
    HASHENT** hepp;
    unsigned size; 

    if (BADTBL(tbl)) {
	fputs("Hash table is invalid.",stderr);
	finalize(1);
    }
    size = tbl->ht_size;
    hepp = &tbl->ht_addr[hash(key,keylen) % size];
    for (hp = *hepp; hp != nullptr; prevhp = hp, hp = hp->he_next) {
	if (hp->he_keylen == keylen && !(*tbl->ht_cmp)(key, keylen, hp->he_data))
	    break;
    }
    /* assert: *(returned value) == hp */
    return (prevhp == nullptr? hepp: &prevhp->he_next);
}

/* not yet taken modulus table size */
static unsigned hash(const char *key, int keylen)
{
    unsigned hash = 0;

    while (keylen--)
	hash += *key++;
    return hash;
}

static int default_cmp(const char *key, int keylen, HASHDATUM data)
{
    /* We already know that the lengths are equal, just compare the strings */
    return memcmp(key, data.dat_ptr, keylen);
}

/* allocate a hash entry */
static HASHENT *healloc()
{
    HASHENT* hp;

    if (s_hereuse == nullptr) {
	int i;

	/* make a nice big block of hashents to play with */
	hp = (HASHENT*)safemalloc(HEBLOCKSIZE * sizeof (HASHENT));
	/* set up the pointers within the block */
	for (i = 0; i < HEBLOCKSIZE-1; i++)
	    (hp+i)->he_next = hp + i + 1;
	/* The last block is the end of the list */
	(hp+i)->he_next = nullptr;
	s_hereuse = hp;		/* start of list is the first item */
	s_reusables += HEBLOCKSIZE;
    }

    /* pull the first reusable one off the pile */
    hp = s_hereuse;
    s_hereuse = s_hereuse->he_next;
    hp->he_next = nullptr;			/* prevent accidents */
    s_reusables--;
    return hp;
}

/* free a hash entry */
static void hefree(HASHENT *hp)
{
#ifdef HASH_FREE_ENTRIES
    if (s_reusables >= RETAIN)		/* compost heap is full? */
	free((char*)hp);		/* yup, just pitch this one */
    else {				/* no, just stash for reuse */
	++s_reusables;
	hp->he_next = s_hereuse;
	s_hereuse = hp;
    }
#else
    /* always add to list */
    ++s_reusables;
    hp->he_next = s_hereuse;
    s_hereuse = hp;
#endif
}

/*
 * Copyright (c) 1992 Geoffrey Collyer
 * All rights reserved.
 * Written by Geoffrey Collyer.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company, the Regents of the University of California, or
 * the Free Software Foundation.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */
