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

using HashCompareFunc = int (*)(const char *key, int keylen, HashDatum data);
using HashWalkFunc = int (*)(int keylen, HashDatum *data, int extra);

#define HASH_DEFCMPFUNC ((HashCompareFunc) nullptr)

HASHTABLE *hashcreate(unsigned size, HashCompareFunc cmpfunc);
void hashdestroy(HASHTABLE *tbl);
void hashstore(HASHTABLE *tbl, const char *key, int keylen, HashDatum data);
void hashdelete(HASHTABLE *tbl, const char *key, int keylen);
HashDatum hashfetch(HASHTABLE *tbl, const char *key, int keylen);
void hashstorelast(HashDatum data);
void hashwalk(HASHTABLE *tbl, HashWalkFunc nodefunc, int extra);

#endif
