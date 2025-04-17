/* hash.c
*/
/* This file is an altered version of a set of hash routines by
** Geoffrey Collyer.  See the end of the file for his copyright.
*/

#include "config/common.h"
#include "trn/hash.h"

#include "trn/final.h"
#include "trn/util.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define BADTBL(tbl)     ((tbl) == nullptr || (tbl)->ht_magic != HASHMAG)

#define HASHMAG  ((char)0257)

struct HashEntry
{
    HashEntry* he_next;           /* in hash chain */
    HashDatum he_data;
    int he_keylen;              /* to help verify a match */
};

struct HashTable
{
    HashEntry** ht_addr;          /* array of HASHENT pointers */
    unsigned ht_size;
    char ht_magic;
    HashCompareFunc ht_cmp;
};

static HashEntry **hashfind(HashTable *tbl, const char *key, int keylen);
static unsigned hash(const char *key, int keylen);
static int default_cmp(const char *key, int keylen, HashDatum data);
static HashEntry *healloc();
static void hefree(HashEntry *hp);

/* In the future it might be a good idea to preallocate a region
 * of hashents, since allocation overhead on some systems will be as
 * large as the structure itself.
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

/* increased from 600.  Each threaded article requires at least
 * one hashent, and various newsgroup hashes can easily get large.
 */
/* tunable parameters */
enum
{
    RETAIN = 1000 /* retain & recycle this many HASHENTs */
};

static HashEntry *s_hereuse{};
static int      s_reusables{};

/* size - a crude guide to size */
HashTable *hash_create(unsigned size, HashCompareFunc cmp_func)
{
    size = std::max(size, 1U);  /* size < 1 is nonsense */
    struct alignalloc
    {
        HashTable ht;
        HashEntry *hepa[1]; /* longer than it looks */
    };
    alignalloc *aap = (alignalloc *)safe_malloc(sizeof *aap + (size - 1) * sizeof(HashEntry *));
    std::memset((char*)aap,0,sizeof *aap + (size-1)*sizeof (HashEntry*));
    HashTable *tbl = &aap->ht;
    tbl->ht_size = size;
    tbl->ht_magic = HASHMAG;
    tbl->ht_cmp = cmp_func == nullptr ? default_cmp : cmp_func;
    tbl->ht_addr = aap->hepa;
    return tbl;
}

/* Free all the memory associated with tbl, erase the pointers to it, and
** invalidate tbl to prevent further use via other pointers to it.
*/
void hash_destroy(HashTable *tbl)
{
    HashEntry* next;

    if (BADTBL(tbl))
    {
        return;
    }
    int tblsize = tbl->ht_size;
    HashEntry **hepp = tbl->ht_addr;
    for (unsigned idx = 0; idx < tblsize; idx++)
    {
        for (HashEntry *hp = hepp[idx]; hp != nullptr; hp = next)
        {
            next = hp->he_next;
            hp->he_next = nullptr;
            hefree(hp);
        }
        hepp[idx] = nullptr;
    }
    tbl->ht_magic = 0;                  /* de-certify this table */
    tbl->ht_addr = nullptr;
    std::free((char*)tbl);
}

void hash_store(HashTable *tbl, const char *key, int key_len, HashDatum data)
{
    HashEntry **nextp = hashfind(tbl, key, key_len);
    HashEntry *hp = *nextp;
    if (hp == nullptr)              /* absent; allocate an entry */
    {
        hp = healloc();
        hp->he_next = nullptr;
        hp->he_keylen = key_len;
        *nextp = hp;                /* append to hash chain */
    }
    hp->he_data = data;             /* supersede any old data for this key */
}

void hash_delete(HashTable *tbl, const char *key, int key_len)
{
    HashEntry **nextp = hashfind(tbl, key, key_len);
    HashEntry *hp = *nextp;
    if (hp == nullptr)                  /* absent */
    {
        return;
    }
    *nextp = hp->he_next;               /* skip this entry */
    hp->he_next = nullptr;
    hp->he_data.dat_ptr = nullptr;
    hefree(hp);
}

static HashEntry **s_slast_nextp{};
static int       s_slast_keylen{};

/* data corresponding to key */
HashDatum hash_fetch(HashTable *tbl, const char *key, int key_len)
{
    static HashDatum errdatum{nullptr, 0};

    HashEntry **nextp = hashfind(tbl, key, key_len);
    s_slast_nextp = nextp;
    s_slast_keylen = key_len;
    HashEntry *hp = *nextp;
    if (hp == nullptr)                  /* absent */
    {
        return errdatum;
    }
    return hp->he_data;
}

void hash_store_last(HashDatum data)
{
    HashEntry *hp = *s_slast_nextp;
    if (hp == nullptr)                          /* absent; allocate an entry */
    {
        hp = healloc();
        hp->he_next = nullptr;
        hp->he_keylen = s_slast_keylen;
        *s_slast_nextp = hp;              /* append to hash chain */
    }
    hp->he_data = data;         /* supersede any old data for this key */
}

/* Visit each entry by calling nodefunc at each, with keylen, data,
** and extra as arguments.
*/
void hash_walk(HashTable *tbl, HashWalkFunc node_func, int extra)
{
    HashEntry* next;
    HashEntry** hepp;

    if (BADTBL(tbl))
    {
        return;
    }
    hepp = tbl->ht_addr;
    int tblsize = tbl->ht_size;
    for (unsigned idx = 0; idx < tblsize; idx++)
    {
        s_slast_nextp = &hepp[idx];
        for (HashEntry *hp = *s_slast_nextp; hp != nullptr; hp = next)
        {
            next = hp->he_next;
            if ((*node_func)(hp->he_keylen, &hp->he_data, extra) < 0)
            {
                *s_slast_nextp = next;
                hp->he_next = nullptr;
                hefree(hp);
            }
            else
            {
                s_slast_nextp = &hp->he_next;
            }
        }
    }
}

/* The returned value is the address of the pointer that refers to the
** found object.  Said pointer may be nullptr if the object was not found;
** if so, this pointer should be updated with the address of the object
** to be inserted, if insertion is desired.
*/
static HashEntry **hashfind(HashTable *tbl, const char *key, int keylen)
{
    HashEntry* prevhp = nullptr;

    if (BADTBL(tbl))
    {
        std::fputs("Hash table is invalid.",stderr);
        finalize(1);
    }
    unsigned size = tbl->ht_size;
    HashEntry **hepp = &tbl->ht_addr[hash(key, keylen) % size];
    for (HashEntry *hp = *hepp; hp != nullptr; prevhp = hp, hp = hp->he_next)
    {
        if (hp->he_keylen == keylen && !(*tbl->ht_cmp)(key, keylen, hp->he_data))
        {
            break;
        }
    }
    /* TRN_ASSERT: *(returned value) == hp */
    return (prevhp == nullptr? hepp: &prevhp->he_next);
}

/* not yet taken modulus table size */
static unsigned hash(const char *key, int keylen)
{
    unsigned hash = 0;

    while (keylen--)
    {
        hash += *key++;
    }
    return hash;
}

static int default_cmp(const char *key, int keylen, HashDatum data)
{
    /* We already know that the lengths are equal, just compare the strings */
    return std::memcmp(key, data.dat_ptr, keylen);
}

/* allocate a hash entry */
static HashEntry *healloc()
{
    HashEntry* hp;

    if (s_hereuse == nullptr)
    {
        int i;

        /* make a nice big block of hashents to play with */
        hp = (HashEntry*)safe_malloc(HEBLOCKSIZE * sizeof (HashEntry));
        /* set up the pointers within the block */
        for (i = 0; i < HEBLOCKSIZE-1; i++)
        {
            (hp + i)->he_next = hp + i + 1;
        }
        /* The last block is the end of the list */
        (hp+i)->he_next = nullptr;
        s_hereuse = hp;         /* start of list is the first item */
        s_reusables += HEBLOCKSIZE;
    }

    /* pull the first reusable one off the pile */
    hp = s_hereuse;
    s_hereuse = s_hereuse->he_next;
    hp->he_next = nullptr;                      /* prevent accidents */
    s_reusables--;
    return hp;
}

/* free a hash entry */
static void hefree(HashEntry *hp)
{
#ifdef HASH_FREE_ENTRIES
    if (s_reusables >= RETAIN)          /* compost heap is full? */
    {
        std::free((char*)hp);                /* yup, just pitch this one */
    }
    else                                /* no, just stash for reuse */
    {
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
