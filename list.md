<!-- Copyright (c) 2026, Richard Thomson -->

# List Storage Audit

## Scope

This audit covers the home-grown `List` container in `libtrn/list.cpp`
and each current caller.  The goal is to migrate stored data to standard
C++ containers, not to preserve the old allocator shape.

`List` is a chunked raw-byte store.  `new_list` records an item size,
items per node, flags, and an optional node initializer.  `list_get_item`
allocates a node on demand, returns an interior byte pointer, and updates
`m_recent`.  `LF_ZERO_MEM` initializes records by `memset`; `LF_SPARSE`
allocates a whole chunk around the requested index.  `delete_list` frees
nodes without running object destructors.

That design blocks owned C++ fields in stored records.  The replacement
containers should be chosen by the data being stored.

## Findings

### MIME Cap Entries

Data stored: mailcap/mimecap entries.  This is normally tens of records,
read sequentially and searched linearly by content type.

Use `std::vector<MimeCapEntry>`.

The data is small and dense.  A vector gives ordinary object lifetime and
lets `MimeCapEntry` own `std::string` fields directly.  No sparse lookup
or chunking is useful here.

### Data Sources

Data stored: configured news sources plus the default source.  This is a
small dense set built during startup.

Use `std::vector<DataSource>`.

The configured source count is small.  Build the vector completely before
publishing `DataSource *` pointers through `Newsrc`.  After publication,
do not append more sources unless pointer invalidation is addressed.

### Multirc Groups

Data stored: configured access-file group records.  This is a small set
of newsrc groups, each keyed by a group number and traversed in numeric
order.

Use `std::vector<Multirc>` sorted by `m_num`.

The data is small enough that a sorted vector is simpler than a map.
`multirc_low`, `multirc_high`, `multirc_next`, and `multirc_prev` can use
indices or `lower_bound` over `m_num`.

### Newsgroup Data

Data stored: one record per newsgroup line in the current newsrc set.
This can be thousands of records, but it is dense input data.

Use `std::vector<NewsgroupData>`.

The backing store should hold stable records for a loaded newsrc set.
The current selector/order behavior is already represented by
`m_prev`/`m_next` links and can later become an order vector or index
list.  Do not move `NewsgroupData` objects after pointers are published.

### Articles

Data stored: per-article metadata for the current newsgroup.  Article
numbers are sparse by definition: a newsgroup can have gaps, expired
articles, or a very large numeric range.

Use `std::map<ArticleNum, Article>`.

The key is the article number.  This directly models the data, supports
first/next/previous existing article navigation, and preserves stable
addresses for article cross-links.  Do not use a vector indexed by
article number; that would encode sparsity as empty storage.

### Source Files

Data stored: normalized lines from active and newsgroups metadata files,
plus a lookup by the first token.  This is not a list of objects and not
article data.

Use `std::vector<std::string>` for owned lines plus a lookup table from
group name to line index.

The current `SourceFile` uses `List` as a byte arena and stores
`ListNode *` plus an offset in the hash table.  Replace that with owned
line strings.  Keep the cache file handle and refetch metadata as
ordinary `SourceFile` state.

## Implementation Slices

### Slice 7: Newsgroup Order Storage

After slice 6, replace intrusive newsgroup ordering with an explicit
order container.

- Use `std::vector<NewsgroupData *>` or indices for selector order.
- Replace sorting by rebuilding the order vector.
- Remove or reduce `m_prev`/`m_next` only when all selector traversal has
  moved to the order container.

### Slice 8: Article Map

Replace `g_article_list` with `std::map<ArticleNum, Article>`.

- Implement `article_ptr` as create-or-return for the article number.
- Implement `article_find` as lookup without insertion.
- Implement `article_first`, `article_next`, `article_last`, and
  `article_prev` with `lower_bound`, `upper_bound`, and reverse
  iterators.
- Replace `article_walk` with map iteration.
- Initialize `Article::m_num` when inserting the record.
- Update tests that construct `g_article_list` directly.

### Slice 9: Source File Line Store

Replace `SourceFile::m_lp` with owned line storage.

- Store normalized lines in `std::vector<std::string>`.
- Replace hash values that hold `ListNode *` plus offset with a line
  index or iterator-safe handle.
- Make `find_active_group` and `find_group_desc` read from owned lines.
- Keep append behavior that updates the backing cache file.
- Only then remove the `SourceFile` dependency on `ListNode`.

### Slice 10: Remove Home-Grown List

After all callers are migrated, delete `libtrn/list.cpp` and
`libtrn/include/trn/list.h`.

- Remove `List` from build files and includes.
- Remove tests that only exist to allocate old `List` state.
- Verify the full workflow.
