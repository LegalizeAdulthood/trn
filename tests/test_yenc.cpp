// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/YencFixture.h"
#include <trn/yenc.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace
{

class YencHeaderTest : public testing::Test
{
protected:
    std::string m_header;
};

using YencHeaderCase = std::tuple<std::string_view, std::string_view, std::uint64_t, int, int, bool>;

class ValidYencHeaderTest : public YencHeaderTest, public testing::WithParamInterface<YencHeaderCase>
{
protected:
    void SetUp() override
    {
        m_header = std::get<0>(GetParam());
    }
};

class InvalidYencHeaderTest : public YencHeaderTest, public testing::WithParamInterface<std::string_view>
{
protected:
    void SetUp() override
    {
        m_header = GetParam();
    }
};

constexpr const char *YENC_MULTIPART_HEADER = "=ybegin part=2 total=2 line=128 size=11 name=hello.bin\n";

class YencPartTest : public testing::Test
{
protected:
    std::string m_text;
};

class MultipartYencPartTest : public YencPartTest
{
protected:
    void SetUp() override;
};

void MultipartYencPartTest::SetUp()
{
    m_text = YENC_MULTIPART_HEADER;
    m_text += "=ypart begin=7 end=11\n\xA1\x99\x9C\x96\x8E\n"
              "=yend size=5 part=2 pcrc32=3a771143 crc32=8bd69e52\n";
}

using InvalidYencPartInput = std::pair<std::string_view, std::string_view>;

class InvalidYencPartTest : public YencPartTest, public testing::WithParamInterface<InvalidYencPartInput>
{
protected:
    void SetUp() override;
};

void InvalidYencPartTest::SetUp()
{
    m_text = GetParam().first;
    m_text += GetParam().second;
}

} // namespace

TEST_P(ValidYencHeaderTest, readsTheHeaderMetadataWithFilenameLast)
{
    const auto &[text, filename, size, part, total, multipart] = GetParam();

    const std::optional<YencHeader> header = yenc_prescan(m_header);

    ASSERT_TRUE(header);
    EXPECT_EQ(filename, header->filename);
    EXPECT_EQ(size, header->size);
    EXPECT_EQ(part, header->part);
    EXPECT_EQ(total, header->total);
    EXPECT_EQ(multipart, header->multipart);
}

INSTANTIATE_TEST_SUITE_P(
    SingleAndMultipart, ValidYencHeaderTest,
    testing::Values(YencHeaderCase{"=ybegin line=128 size=3 name=path/a file.bin\r\n", "path/a file.bin", 3, 1, 1,
                                   false},
                    YencHeaderCase{"=ybegin part=2 total=3 line=128 size=11 name=a.bin\n", "a.bin", 11, 2, 3, true},
                    YencHeaderCase{"=ybegin part=1 line=128 size=3 name=a.bin", "a.bin", 3, 1, 0, true}));

TEST_P(InvalidYencHeaderTest, rejectsIncompleteAndInvalidMetadata)
{
    const std::optional<YencHeader> header = yenc_prescan(m_header);

    EXPECT_FALSE(header);
}

INSTANTIATE_TEST_SUITE_P(InvalidMetadata, InvalidYencHeaderTest,
                         testing::Values("=ybegin2 line=128 size=3 name=a.bin", "=ybegin size=3 name=a.bin",
                                         "=ybegin line=0 size=3 name=a.bin", "=ybegin line=128 name=a.bin",
                                         "=ybegin line=128 size=-1 name=a.bin",
                                         "=ybegin line=128 size=3 name=", "=ybegin part=0 line=128 size=3 name=a.bin",
                                         "=ybegin part=3 total=2 line=128 size=3 name=a.bin",
                                         "=ybegin line=128 size=18446744073709551616 name=a.bin",
                                         "=ybegin line=128 size=3 size=4 name=a.bin"));

TEST(YencDecodeTest, decodesSinglePartWithCrcAndIgnoresSurroundingProse)
{
    const std::optional<YencPart> part = yenc_decode("Introduction\r\n=ybegin line=128 size=3 name=cat.bin\r\n"
                                                     "\x6D\x8B\x9E\r\n=yend size=3 crc32=a6130548\r\nSignature\r\n");
    ASSERT_TRUE(part);
    EXPECT_EQ("Cat", part->data);
    EXPECT_EQ(1U, part->begin);
    EXPECT_EQ(3U, part->end);
    EXPECT_EQ(0xA6130548U, part->crc32);
}

TEST(YencDecodeTest, decodesCriticalEscapesNulAndHighBytes)
{
    const std::optional<YencPart> part =
        yenc_decode("=ybegin line=128 size=6 name=bytes.bin\n=@=J=M=}*)\n=yend size=6\n");
    ASSERT_TRUE(part);
    EXPECT_EQ(std::string("\xD6\xE0\xE3\x13\0\xFF", 6), part->data);
}

TEST_P(InvalidYencPartTest, rejectsTheEncodedPart)
{
    const std::optional<YencPart> part = yenc_decode(m_text);

    EXPECT_FALSE(part);
}

INSTANTIATE_TEST_SUITE_P(
    CorruptionAndTruncation, InvalidYencPartTest,
    testing::Values(InvalidYencPartInput{"", "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n"},
                    InvalidYencPartInput{"", "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n=yend size=2\n"},
                    InvalidYencPartInput{"", "=ybegin line=128 size=4 name=a\n\x6D\x8B\x9E\n=yend size=3\n"},
                    InvalidYencPartInput{"",
                                         "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n=yend size=3 crc32=00000000\n"},
                    InvalidYencPartInput{"", "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n=yend size=3 crc32=xyz\n"},
                    InvalidYencPartInput{"", "=ybegin line=128 size=1 name=a\n=\n=yend size=1\n"}));

TEST_F(MultipartYencPartTest, checksMultipartRangePartNumberAndChecksum)
{
    const std::optional<YencPart> part = yenc_decode(m_text);

    ASSERT_TRUE(part);
    EXPECT_EQ("world", part->data);
    EXPECT_EQ(7U, part->begin);
    EXPECT_EQ(11U, part->end);
    EXPECT_EQ(0x8BD69E52U, part->crc32);
}

INSTANTIATE_TEST_SUITE_P(
    MultipartMetadata, InvalidYencPartTest,
    testing::Values(
        InvalidYencPartInput{YENC_MULTIPART_HEADER, "\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n"},
        InvalidYencPartInput{YENC_MULTIPART_HEADER,
                             "=ypart begin=0 end=11\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n"},
        InvalidYencPartInput{YENC_MULTIPART_HEADER,
                             "=ypart begin=7 end=12\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n"},
        InvalidYencPartInput{YENC_MULTIPART_HEADER,
                             "=ypart begin=7 end=11\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=1 pcrc32=3a771143\n"},
        InvalidYencPartInput{YENC_MULTIPART_HEADER,
                             "=ypart begin=7 end=11\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=00000000\n"}));

TEST(YencDecodeTest, crcMatchesPublishedVectorAndIncrementalInput)
{
    EXPECT_EQ(0U, yenc_crc32({}));
    EXPECT_EQ(0xCBF43926U, yenc_crc32("123456789"));
    EXPECT_EQ(0x8BD69E52U, yenc_crc32("world", yenc_crc32("Hello ")));
}

TEST(YencDecodeTest, decodesAllByteValues)
{
    const std::optional<YencPart> part = yenc_decode(YENC_ALL_BYTES);
    ASSERT_TRUE(part);
    ASSERT_EQ(256U, part->data.size());
    for (int i = 0; i < 256; ++i)
    {
        EXPECT_EQ(i, static_cast<unsigned char>(part->data[i]));
    }
}

TEST(YencDecodeTest, rejectsRawNulFollowingEscape)
{
    std::string text{"=ybegin line=128 size=1 name=bad.bin\n="};
    text += '\0';
    text += "\n=yend size=1\n";
    EXPECT_FALSE(yenc_decode(text));
}
