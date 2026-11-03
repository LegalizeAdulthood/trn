/*
 * general-purpose in-core hashing
 */
/* This file is an altered version of a set of hash routines by
 * Geoffrey Collyer.  See hash.c for his copyright.
 */
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_HASH_H
#define TRN_HASH_H

#include <cstddef>
#include <string_view>

struct HashDatum
{
    char    *dat_ptr;
    unsigned dat_len;
};

struct HashTable;

using HashCompareFunc = int(std::string_view key, HashDatum data);
using HashWalkFunc = int(int key_len, HashDatum *data, int extra);

struct HashLookupCursor
{
    HashTable  *table{};
    std::size_t bucket{};
    std::size_t index{};
    int         keylen{};

    void store_last(HashDatum data);
    HashDatum fetch(HashTable *tbl, std::string_view key);
    void      walk(HashTable *tbl, HashWalkFunc *node_func, int extra);
};

extern HashLookupCursor g_hash_cursor;

HashTable *hash_create(unsigned size, HashCompareFunc *cmp_func);
void hash_destroy(HashTable *tbl);
void hash_store(HashTable *tbl, std::string_view key, HashDatum data);
void hash_delete(HashTable *tbl, std::string_view key);

#endif
