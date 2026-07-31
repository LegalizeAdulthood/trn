/* hash.cpp
*/
/* This file is an altered version of a set of hash routines by
** Geoffrey Collyer.  See the end of the file for his copyright.
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/hash.h>

#include <config/common.h>
#include <trn/final.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <vector>

#define BADTBL(tbl)     ((tbl) == nullptr || (tbl)->ht_magic != HASHMAG)

#define HASHMAG  ((char)0257)

struct HashEntry
{
    HashDatum he_data;
    int       he_key_len; // to help verify a match
};

struct HashTable
{
    std::vector<std::vector<HashEntry>> ht_addr;
    char                                ht_magic;
    HashCompareFunc                     ht_cmp;
};

static std::size_t hash_bucket_index(HashTable *tbl, std::string_view key);
static std::size_t hash_find(HashTable *tbl, std::size_t bucket_index, std::string_view key);
static unsigned    hash(std::string_view key);
static int         default_cmp(std::string_view key, HashDatum data);

// size - a crude guide to size
HashTable *hash_create(unsigned size, HashCompareFunc cmp_func)
{
    size = std::max(size, 1U); // size < 1 is nonsense
    HashTable *tbl = new HashTable{};
    tbl->ht_addr.resize(size);
    tbl->ht_magic = HASHMAG;
    tbl->ht_cmp = cmp_func == nullptr ? default_cmp : cmp_func;
    return tbl;
}

// Free all the memory associated with tbl, erase the pointers to it, and
// invalidate tbl to prevent further use via other pointers to it.
//
void hash_destroy(HashTable *tbl)
{
    if (BADTBL(tbl))
    {
        return;
    }
    tbl->ht_magic = 0; // de-certify this table
    delete tbl;
}

void hash_store(HashTable *tbl, std::string_view key, HashDatum data)
{
    const int               key_len = static_cast<int>(key.size());
    const std::size_t       bucket_index = hash_bucket_index(tbl, key);
    const std::size_t       entry_index = hash_find(tbl, bucket_index, key);
    std::vector<HashEntry> &bucket = tbl->ht_addr[bucket_index];
    if (entry_index == bucket.size()) // absent; allocate an entry
    {
        bucket.push_back({data, key_len});
        return;
    }
    bucket[entry_index].he_data = data; // supersede any old data for this key
}

void hash_delete(HashTable *tbl, std::string_view key)
{
    const std::size_t       bucket_index = hash_bucket_index(tbl, key);
    const std::size_t       entry_index = hash_find(tbl, bucket_index, key);
    std::vector<HashEntry> &bucket = tbl->ht_addr[bucket_index];
    if (entry_index == bucket.size()) // absent
    {
        return;
    }
    bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(entry_index));
}

static HashTable  *s_slast_table{};
static std::size_t s_slast_bucket{};
static std::size_t s_slast_index{};
static int         s_slast_keylen{};

// data corresponding to key
HashDatum hash_fetch(HashTable *tbl, std::string_view key)
{
    const int               key_len = static_cast<int>(key.size());
    const std::size_t       bucket_index = hash_bucket_index(tbl, key);
    const std::size_t       entry_index = hash_find(tbl, bucket_index, key);
    std::vector<HashEntry> &bucket = tbl->ht_addr[bucket_index];

    s_slast_table = tbl;
    s_slast_bucket = bucket_index;
    s_slast_index = entry_index;
    s_slast_keylen = key_len;
    if (entry_index == bucket.size()) // absent
    {
        return {nullptr, 0};
    }
    return bucket[entry_index].he_data;
}

void hash_store_last(HashDatum data)
{
    std::vector<HashEntry> &bucket = s_slast_table->ht_addr[s_slast_bucket];
    if (s_slast_index == bucket.size()) // absent; allocate an entry
    {
        bucket.push_back({data, s_slast_keylen});
        return;
    }
    bucket[s_slast_index].he_data = data; // supersede any old data for this key
}

// Visit each entry by calling nodefunc at each, with keylen, data,
// and extra as arguments.
//
void hash_walk(HashTable *tbl, HashWalkFunc node_func, int extra)
{
    if (BADTBL(tbl))
    {
        return;
    }
    for (std::size_t bucket_index = 0; bucket_index < tbl->ht_addr.size(); ++bucket_index)
    {
        std::vector<HashEntry> &bucket = tbl->ht_addr[bucket_index];
        for (std::size_t entry_index = 0; entry_index < bucket.size();)
        {
            HashEntry &entry = bucket[entry_index];
            s_slast_table = tbl;
            s_slast_bucket = bucket_index;
            s_slast_index = entry_index;
            s_slast_keylen = entry.he_key_len;
            if ((*node_func)(entry.he_key_len, &entry.he_data, extra) < 0)
            {
                bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(entry_index));
            }
            else
            {
                ++entry_index;
            }
        }
    }
}

static std::size_t hash_bucket_index(HashTable *tbl, std::string_view key)
{
    if (BADTBL(tbl))
    {
        fmt::print(stderr, "Hash table is invalid.");
        finalize(1);
    }
    return hash(key) % tbl->ht_addr.size();
}

static std::size_t hash_find(HashTable *tbl, std::size_t bucket_index, std::string_view key)
{
    const int               key_len = static_cast<int>(key.size());
    std::vector<HashEntry> &bucket = tbl->ht_addr[bucket_index];
    for (std::size_t entry_index = 0; entry_index < bucket.size(); ++entry_index)
    {
        HashEntry &entry = bucket[entry_index];
        if (entry.he_key_len == key_len && !(*tbl->ht_cmp)(key, entry.he_data))
        {
            return entry_index;
        }
    }
    return bucket.size();
}

// not yet taken modulus table size
static unsigned hash(std::string_view key)
{
    unsigned hash_value = 0;

    for (char ch : key)
    {
        hash_value += ch;
    }
    return hash_value;
}

static int default_cmp(std::string_view key, HashDatum data)
{
    // We already know that the lengths are equal, just compare the strings
    return key.compare(std::string_view{data.dat_ptr, key.size()});
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
