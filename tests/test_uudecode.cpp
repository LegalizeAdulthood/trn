// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/uudecode.h>

#include <gtest/gtest.h>

TEST(UuePrescanTest, beginLineInitializesSinglePartDecode)
{
    std::string filename{"old.bin"};
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("begin 644 archive.bin", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_TRUE(filename.empty());
    EXPECT_EQ(1, part);
    EXPECT_EQ(0, total);
}

TEST(UuePrescanTest, sectionOfFileLineFindsFilenameAndPart)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("section 2 of 10 of file archive.bin trailing", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(0, total);
}

TEST(UuePrescanTest, postLineFindsFilenameAndPartCount)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("POST V1.0 archive.bin (Part 2/5)", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, fileLineFindsFilenameAndPartCount)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("File: archive.bin -- part 2 of 5 -- trailing", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, bracketedSectionLineFindsFilenameAndPartCount)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("[Section: 2/5 File: archive.bin trailing", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, fileNameHeaderStoresFilename)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("X-File-Name: archive.bin trailing", filename, &part, &total);

    EXPECT_EQ(0, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(-1, part);
    EXPECT_EQ(-1, total);
}

TEST(UuePrescanTest, partHeaderStoresPart)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("X-Part: 2", filename, &part, &total);

    EXPECT_EQ(0, result);
    EXPECT_TRUE(filename.empty());
    EXPECT_EQ(2, part);
    EXPECT_EQ(-1, total);
}

TEST(UuePrescanTest, beginMarkerStartsKnownMultipartDecode)
{
    std::string filename{"archive.bin"};
    int         part = 2;
    int         total = 5;

    const int result = uue_prescan("BEGIN", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}
