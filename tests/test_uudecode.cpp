// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/uudecode.h>

#include <gtest/gtest.h>

TEST(UuePrescanTest, beginLineInitializesSinglePartDecode)
{
    char  old_filename[]{"old.bin"};
    char *filename = old_filename;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("begin 644 archive.bin", &filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ(nullptr, filename);
    EXPECT_EQ(1, part);
    EXPECT_EQ(0, total);
}

TEST(UuePrescanTest, sectionOfFileLineFindsFilenameAndPart)
{
    char *filename = nullptr;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("section 2 of 10 of file archive.bin trailing", &filename, &part, &total);

    EXPECT_EQ(1, result);
    ASSERT_NE(nullptr, filename);
    EXPECT_STREQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(0, total);
}

TEST(UuePrescanTest, postLineFindsFilenameAndPartCount)
{
    char *filename = nullptr;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("POST V1.0 archive.bin (Part 2/5)", &filename, &part, &total);

    EXPECT_EQ(1, result);
    ASSERT_NE(nullptr, filename);
    EXPECT_STREQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, fileLineFindsFilenameAndPartCount)
{
    char *filename = nullptr;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("File: archive.bin -- part 2 of 5 -- trailing", &filename, &part, &total);

    EXPECT_EQ(1, result);
    ASSERT_NE(nullptr, filename);
    EXPECT_STREQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, bracketedSectionLineFindsFilenameAndPartCount)
{
    char *filename = nullptr;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("[Section: 2/5 File: archive.bin trailing", &filename, &part, &total);

    EXPECT_EQ(1, result);
    ASSERT_NE(nullptr, filename);
    EXPECT_STREQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, fileNameHeaderStoresFilename)
{
    char *filename = nullptr;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("X-File-Name: archive.bin trailing", &filename, &part, &total);

    EXPECT_EQ(0, result);
    ASSERT_NE(nullptr, filename);
    EXPECT_STREQ("archive.bin", filename);
    EXPECT_EQ(-1, part);
    EXPECT_EQ(-1, total);
}

TEST(UuePrescanTest, partHeaderStoresPart)
{
    char *filename = nullptr;
    int   part = -1;
    int   total = -1;

    const int result = uue_prescan("X-Part: 2", &filename, &part, &total);

    EXPECT_EQ(0, result);
    EXPECT_EQ(nullptr, filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(-1, total);
}

TEST(UuePrescanTest, beginMarkerStartsKnownMultipartDecode)
{
    char  existing_filename[]{"archive.bin"};
    char *filename = existing_filename;
    int   part = 2;
    int   total = 5;

    const int result = uue_prescan("BEGIN", &filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ(existing_filename, filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}
