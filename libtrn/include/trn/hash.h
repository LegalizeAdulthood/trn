/*
 * general-purpose in-core hashing
 */
/* This file is an altered version of a set of hash routines by
 * Geoffrey Collyer.  See hash.c for his copyright.
 */
#ifndef TRN_HASH_H
#define TRN_HASH_H

struct HashDatum
{
    char *dat_ptr;
    unsigned dat_len;
};

struct HASHTABLE;

using HASHCMPFUNC = int (*)(const char *key, int keylen, HashDatum data);
using HASHWALKFUNC = int (*)(int keylen, HashDatum *data, int extra);

#define HASH_DEFCMPFUNC ((HASHCMPFUNC) nullptr)

HASHTABLE *hashcreate(unsigned size, HASHCMPFUNC cmpfunc);
void hashdestroy(HASHTABLE *tbl);
void hashstore(HASHTABLE *tbl, const char *key, int keylen, HashDatum data);
void hashdelete(HASHTABLE *tbl, const char *key, int keylen);
HashDatum hashfetch(HASHTABLE *tbl, const char *key, int keylen);
void hashstorelast(HashDatum data);
void hashwalk(HASHTABLE *tbl, HASHWALKFUNC nodefunc, int extra);

#endif
