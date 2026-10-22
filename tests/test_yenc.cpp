// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/YencFixture.h"
#include <trn/yenc.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

TEST(YencPrescanTest, readsSingleAndMultipartHeadersWithFilenameLast)
{
    const std::optional<YencHeader> single = yenc_prescan("=ybegin line=128 size=3 name=path/a file.bin\r\n");
    ASSERT_TRUE(single);
    EXPECT_EQ("path/a file.bin", single->filename);
    EXPECT_EQ(3U, single->size);
    EXPECT_EQ(1, single->part);
    EXPECT_EQ(1, single->total);
    EXPECT_FALSE(single->multipart);
    const std::optional<YencHeader> split = yenc_prescan("=ybegin part=2 total=3 line=128 size=11 name=a.bin\n");
    ASSERT_TRUE(split);
    EXPECT_TRUE(split->multipart);
    EXPECT_EQ(2, split->part);
    EXPECT_EQ(3, split->total);
    ASSERT_TRUE(yenc_prescan("=ybegin part=1 line=128 size=3 name=a.bin"));
    EXPECT_EQ(0, yenc_prescan("=ybegin part=1 line=128 size=3 name=a.bin")->total);
}

TEST(YencPrescanTest, rejectsIncompleteAndInvalidMetadata)
{
    for (const std::string_view line :
         {"=ybegin2 line=128 size=3 name=a.bin", "=ybegin size=3 name=a.bin", "=ybegin line=0 size=3 name=a.bin",
          "=ybegin line=128 name=a.bin", "=ybegin line=128 size=-1 name=a.bin", "=ybegin line=128 size=3 name=",
          "=ybegin part=0 line=128 size=3 name=a.bin", "=ybegin part=3 total=2 line=128 size=3 name=a.bin",
          "=ybegin line=128 size=18446744073709551616 name=a.bin", "=ybegin line=128 size=3 size=4 name=a.bin"})
    {
        SCOPED_TRACE(line);
        EXPECT_FALSE(yenc_prescan(line));
    }
}

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

TEST(YencDecodeTest, rejectsCorruptionAndTruncation)
{
    for (const std::string_view text : {"=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n",
                                        "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n=yend size=2\n",
                                        "=ybegin line=128 size=4 name=a\n\x6D\x8B\x9E\n=yend size=3\n",
                                        "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n=yend size=3 crc32=00000000\n",
                                        "=ybegin line=128 size=3 name=a\n\x6D\x8B\x9E\n=yend size=3 crc32=xyz\n",
                                        "=ybegin line=128 size=1 name=a\n=\n=yend size=1\n"})
    {
        SCOPED_TRACE(text);
        EXPECT_FALSE(yenc_decode(text));
    }
}

TEST(YencDecodeTest, checksMultipartRangePartNumberAndChecksum)
{
    const std::string header{"=ybegin part=2 total=2 line=128 size=11 name=hello.bin\n"};
    const std::optional<YencPart> part = yenc_decode(header + "=ypart begin=7 end=11\n\xA1\x99\x9C\x96\x8E\n"
                                                              "=yend size=5 part=2 pcrc32=3a771143 crc32=8bd69e52\n");
    ASSERT_TRUE(part);
    EXPECT_EQ("world", part->data);
    EXPECT_EQ(7U, part->begin);
    EXPECT_EQ(11U, part->end);
    EXPECT_EQ(0x8BD69E52U, part->crc32);
    for (const std::string_view body :
         {"\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n",
          "=ypart begin=0 end=11\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n",
          "=ypart begin=7 end=12\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n",
          "=ypart begin=7 end=11\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=1 pcrc32=3a771143\n",
          "=ypart begin=7 end=11\n\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=00000000\n"})
    {
        EXPECT_FALSE(yenc_decode(header + std::string{body}));
    }
}

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
