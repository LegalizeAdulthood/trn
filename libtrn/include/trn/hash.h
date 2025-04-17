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
    char    *dat_ptr;
    unsigned dat_len;
};

struct HashTable;

using HashCompareFunc = int (*)(const char *key, int key_len, HashDatum data);
using HashWalkFunc = int (*)(int key_len, HashDatum *data, int extra);

HashTable *hash_create(unsigned size, HashCompareFunc cmp_func);
void hash_destroy(HashTable *tbl);
void hash_store(HashTable *tbl, const char *key, int key_len, HashDatum data);
void hash_delete(HashTable *tbl, const char *key, int key_len);
HashDatum hash_fetch(HashTable *tbl, const char *key, int key_len);
void hash_store_last(HashDatum data);
void hash_walk(HashTable *tbl, HashWalkFunc node_func, int extra);

#endif
