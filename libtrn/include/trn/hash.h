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

struct HashTable;

using HashCompareFunc = int (*)(const char *key, int keylen, HashDatum data);
using HashWalkFunc = int (*)(int keylen, HashDatum *data, int extra);

#define HASH_DEFCMPFUNC ((HashCompareFunc) nullptr)

HashTable *hashcreate(unsigned size, HashCompareFunc cmpfunc);
void hashdestroy(HashTable *tbl);
void hashstore(HashTable *tbl, const char *key, int keylen, HashDatum data);
void hashdelete(HashTable *tbl, const char *key, int keylen);
HashDatum hashfetch(HashTable *tbl, const char *key, int keylen);
void hashstorelast(HashDatum data);
void hashwalk(HashTable *tbl, HashWalkFunc nodefunc, int extra);

#endif
