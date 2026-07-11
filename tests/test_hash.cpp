// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/hash.h>

#include <gtest/gtest.h>

#include <cctype>
#include <string>
#include <string_view>

namespace
{

class HashTableOwner
{
public:
    explicit HashTableOwner(unsigned size = 17, HashCompareFunc cmp_func = nullptr) :
        m_table{hash_create(size, cmp_func)}
    {
    }

    HashTableOwner(const HashTableOwner &) = delete;
    HashTableOwner &operator=(const HashTableOwner &) = delete;

    ~HashTableOwner()
    {
        hash_destroy(m_table);
    }

    HashTable *get() const
    {
        return m_table;
    }

private:
    HashTable *m_table{};
};

HashDatum make_datum(std::string &text, unsigned length)
{
    return {text.data(), length};
}

int add_extra_to_datum(int, HashDatum *data, int extra)
{
    data->dat_len += static_cast<unsigned>(extra);
    return 0;
}

int remove_matching_datum(int, HashDatum *data, int extra)
{
    return data->dat_len == static_cast<unsigned>(extra) ? -1 : 0;
}

int case_insensitive_cmp(std::string_view key, HashDatum data)
{
    const std::string_view data_view{data.dat_ptr, key.size()};

    for (std::string_view::size_type idx = 0; idx < key.size(); ++idx)
    {
        const auto key_ch = static_cast<unsigned char>(key[idx]);
        const auto data_ch = static_cast<unsigned char>(data_view[idx]);
        const int  diff = std::tolower(key_ch) - std::tolower(data_ch);

        if (diff != 0)
        {
            return diff;
        }
    }
    return 0;
}

} // namespace

TEST(HashTest, fetchMissingReturnsEmptyDatum)
{
    HashTableOwner table{};

    const HashDatum data = hash_fetch(table.get(), "missing");

    EXPECT_EQ(nullptr, data.dat_ptr);
    EXPECT_EQ(0U, data.dat_len);
}

TEST(HashTest, storeAndFetchFindsCollidingEntries)
{
    HashTableOwner table{1};
    std::string    alpha{"alpha"};
    std::string    beta{"beta"};

    hash_store(table.get(), alpha, make_datum(alpha, 11));
    hash_store(table.get(), beta, make_datum(beta, 22));

    const HashDatum alpha_data = hash_fetch(table.get(), alpha);
    const HashDatum beta_data = hash_fetch(table.get(), beta);

    EXPECT_EQ(alpha.data(), alpha_data.dat_ptr);
    EXPECT_EQ(11U, alpha_data.dat_len);
    EXPECT_EQ(beta.data(), beta_data.dat_ptr);
    EXPECT_EQ(22U, beta_data.dat_len);
}

TEST(HashTest, storeAcceptsBoundedStringViewKeys)
{
    HashTableOwner         table{};
    const std::string      buffer{"prefix-key-suffix"};
    std::string            key{"key"};
    const auto             key_pos = buffer.find(key);
    const std::string_view key_view{buffer.data() + key_pos, key.size()};

    hash_store(table.get(), key_view, make_datum(key, 33));

    const HashDatum data = hash_fetch(table.get(), key);

    EXPECT_EQ(key.data(), data.dat_ptr);
    EXPECT_EQ(33U, data.dat_len);
}

TEST(HashTest, storeReplacesExistingDatum)
{
    HashTableOwner table{};
    std::string    alpha{"alpha"};

    hash_store(table.get(), alpha, make_datum(alpha, 1));
    hash_store(table.get(), alpha, make_datum(alpha, 2));

    const HashDatum data = hash_fetch(table.get(), alpha);

    EXPECT_EQ(alpha.data(), data.dat_ptr);
    EXPECT_EQ(2U, data.dat_len);
}

TEST(HashTest, storeLastInsertsAndUpdatesLastFetch)
{
    HashTableOwner table{};
    std::string    alpha{"alpha"};

    const HashDatum missing = hash_fetch(table.get(), alpha);
    EXPECT_EQ(nullptr, missing.dat_ptr);

    hash_store_last(make_datum(alpha, 3));
    EXPECT_EQ(3U, hash_fetch(table.get(), alpha).dat_len);

    hash_store_last(make_datum(alpha, 4));
    EXPECT_EQ(4U, hash_fetch(table.get(), alpha).dat_len);
}

TEST(HashTest, deleteRemovesOnlyMatchingKey)
{
    HashTableOwner table{1};
    std::string    alpha{"alpha"};
    std::string    beta{"beta"};

    hash_store(table.get(), alpha, make_datum(alpha, 10));
    hash_store(table.get(), beta, make_datum(beta, 20));

    hash_delete(table.get(), alpha);

    EXPECT_EQ(nullptr, hash_fetch(table.get(), alpha).dat_ptr);
    EXPECT_EQ(20U, hash_fetch(table.get(), beta).dat_len);
}

TEST(HashTest, walkCanUpdateAndRemoveEntries)
{
    HashTableOwner table{1};
    std::string    alpha{"alpha"};
    std::string    beta{"beta"};
    std::string    gamma{"gamma"};

    hash_store(table.get(), alpha, make_datum(alpha, 1));
    hash_store(table.get(), beta, make_datum(beta, 2));
    hash_store(table.get(), gamma, make_datum(gamma, 3));

    hash_walk(table.get(), add_extra_to_datum, 10);

    EXPECT_EQ(11U, hash_fetch(table.get(), alpha).dat_len);
    EXPECT_EQ(12U, hash_fetch(table.get(), beta).dat_len);
    EXPECT_EQ(13U, hash_fetch(table.get(), gamma).dat_len);

    hash_walk(table.get(), remove_matching_datum, 12);

    EXPECT_EQ(11U, hash_fetch(table.get(), alpha).dat_len);
    EXPECT_EQ(nullptr, hash_fetch(table.get(), beta).dat_ptr);
    EXPECT_EQ(13U, hash_fetch(table.get(), gamma).dat_len);
}

TEST(HashTest, customComparatorIsUsed)
{
    HashTableOwner table{1, case_insensitive_cmp};
    std::string    alpha{"Alpha"};

    hash_store(table.get(), alpha, make_datum(alpha, 44));

    const HashDatum data = hash_fetch(table.get(), "alpha");

    EXPECT_EQ(alpha.data(), data.dat_ptr);
    EXPECT_EQ(44U, data.dat_len);
}
