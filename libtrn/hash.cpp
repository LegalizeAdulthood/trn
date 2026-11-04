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

// size - a crude guide to size
HashTable::HashTable(unsigned size, HashCompareFunc *cmp_func) :
    ht_addr(std::max(size, 1U)), // size < 1 is nonsense
    ht_magic(HASHMAG),
    ht_cmp(cmp_func == nullptr ? default_cmp : cmp_func)
{
}

// Invalidate the table before its entry storage is released.
HashTable::~HashTable()
{
    ht_magic = 0; // de-certify this table
}

void HashTable::store(std::string_view key, HashDatum data)
{
    const int               key_len = static_cast<int>(key.size());
    const std::size_t       bucket_index = this->bucket_index(key);
    const std::size_t       entry_index = find(bucket_index, key);
    std::vector<HashEntry> &bucket = ht_addr[bucket_index];
    if (entry_index == bucket.size()) // absent; allocate an entry
    {
        bucket.push_back({data, key_len});
        return;
    }
    bucket[entry_index].he_data = data; // supersede any old data for this key
}

void HashTable::erase(std::string_view key)
{
    const std::size_t       bucket_index = this->bucket_index(key);
    const std::size_t       entry_index = find(bucket_index, key);
    std::vector<HashEntry> &bucket = ht_addr[bucket_index];
    if (entry_index == bucket.size()) // absent
    {
        return;
    }
    bucket.erase(bucket.begin() + static_cast<std::ptrdiff_t>(entry_index));
}

HashLookupCursor g_hash_cursor{};

// data corresponding to key
// Table validation and comparator callbacks retain their existing side effects.
HashDatum HashLookupCursor::fetch(HashTable *tbl, std::string_view key)
{
    if (tbl == nullptr)
    {
        fmt::print(stderr, "Hash table is invalid.");
        finalize(1);
    }
    const int               key_len = static_cast<int>(key.size());
    const std::size_t       bucket_index = tbl->bucket_index(key);
    const std::size_t       entry_index = tbl->find(bucket_index, key);
    std::vector<HashEntry> &bucket = tbl->ht_addr[bucket_index];

    table = tbl;
    this->bucket = bucket_index;
    index = entry_index;
    keylen = key_len;
    if (entry_index == bucket.size()) // absent
    {
        return {nullptr, 0};
    }
    return bucket[entry_index].he_data;
}

// Mutates entries in the borrowed table; storage ownership stays with it.
void HashLookupCursor::store_last(HashDatum data)
{
    std::vector<HashEntry> &bucket = table->ht_addr[this->bucket];
    if (index == bucket.size()) // absent; allocate an entry
    {
        bucket.push_back({data, keylen});
        return;
    }
    bucket[index].he_data = data; // supersede any old data for this key
}

// Visit each entry by calling nodefunc at each, with keylen, data,
// and extra as arguments.
//
// Callbacks retain their payload mutations, cleanup, I/O, and global effects.
void HashLookupCursor::walk(HashTable *tbl, HashWalkFunc *node_func, int extra)
{
    walk(tbl, [node_func, extra](int key_len, HashDatum *data) { return node_func(key_len, data, extra); });
}

void HashLookupCursor::walk(HashTable *tbl, const std::function<HashVisitFunc> &node_func)
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
            table = tbl;
            this->bucket = bucket_index;
            index = entry_index;
            keylen = entry.he_key_len;
            if (node_func(entry.he_key_len, &entry.he_data) < 0)
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

std::size_t HashTable::bucket_index(std::string_view key)
{
    if (ht_magic != HASHMAG)
    {
        fmt::print(stderr, "Hash table is invalid.");
        finalize(1);
    }
    return hash(key) % ht_addr.size();
}

std::size_t HashTable::find(std::size_t bucket_index, std::string_view key)
{
    const int               key_len = static_cast<int>(key.size());
    std::vector<HashEntry> &bucket = ht_addr[bucket_index];
    for (std::size_t entry_index = 0; entry_index < bucket.size(); ++entry_index)
    {
        HashEntry &entry = bucket[entry_index];
        if (entry.he_key_len == key_len && !ht_cmp(key, entry.he_data))
        {
            return entry_index;
        }
    }
    return bucket.size();
}

// not yet taken modulus table size
unsigned HashTable::hash(std::string_view key)
{
    unsigned hash_value = 0;

    for (char ch : key)
    {
        hash_value += ch;
    }
    return hash_value;
}

int HashTable::default_cmp(std::string_view key, HashDatum data)
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
