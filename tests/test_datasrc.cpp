// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/datasrc.h>

#include <trn/hash.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

namespace
{

class SourceFileOwner
{
public:
    SourceFileOwner()
    {
        m_source.m_hp = hash_create(17, nullptr);
    }

    SourceFileOwner(const SourceFileOwner &) = delete;
    SourceFileOwner &operator=(const SourceFileOwner &) = delete;

    ~SourceFileOwner()
    {
        m_source.close();
    }

    SourceFile &get()
    {
        return m_source;
    }

private:
    SourceFile m_source{};
};

} // namespace

TEST(SourceFileAppendTest, storesNormalizedLineAndReturnsStoredStorage)
{
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();
    char            line[] = "comp.lang.c++     C++ language discussion";
    const int       key_len = static_cast<int>(std::strlen("comp.lang.c++"));

    const std::string_view stored_line = source_file.append(line, key_len);

    ASSERT_EQ(1U, source_file.m_lines.size());
    EXPECT_EQ(source_file.m_lines.back().data(), stored_line.data());
    EXPECT_EQ("comp.lang.c++ C++ language discussion\n", stored_line);
    EXPECT_EQ("comp.lang.c++ C++ language discussion\n", source_file.m_lines.back());
    EXPECT_EQ("C++ language discussion\n", std::string_view{stored_line.data() + key_len + 1});
}
