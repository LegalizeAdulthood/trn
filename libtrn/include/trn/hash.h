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
#include <vector>

struct HashDatum
{
    char    *dat_ptr;
    unsigned dat_len;
};

struct HashEntry
{
    HashDatum he_data;
    int       he_key_len; // to help verify a match
};

using HashCompareFunc = int(std::string_view key, HashDatum data);
using HashWalkFunc = int(int key_len, HashDatum *data, int extra);

struct HashTable
{
    std::vector<std::vector<HashEntry>> ht_addr;
    char                                ht_magic;
    HashCompareFunc                    *ht_cmp;

    HashTable(unsigned size, HashCompareFunc *cmp_func);
    ~HashTable();

    void store(std::string_view key, HashDatum data);
    void erase(std::string_view key);

    // TODO: private
    std::size_t     bucket_index(std::string_view key);
    std::size_t     find(std::size_t bucket_index, std::string_view key);
    static unsigned hash(std::string_view key);
    static int      default_cmp(std::string_view key, HashDatum data);
};

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

#endif
