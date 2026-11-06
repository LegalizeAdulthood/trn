// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/SourceCheckpoint.h>

#include <trn/last.h>

#include <gtest/gtest.h>

namespace
{

struct LegacyRestoreCase
{
    const char *name;
    bool        remote;
    long        active_cnt;
    long        desc_cnt;
};

class LegacyCheckpointTest : public testing::Test
{
protected:
    LastRunState     m_last{"comp.saved", 444, 222, 111, 333, "last-file", 555};
    SourceCheckpoint m_checkpoint{71, 72, 73};
};

class LegacyCheckpointRestoreTest : public LegacyCheckpointTest, public testing::WithParamInterface<LegacyRestoreCase>
{
};

TEST_P(LegacyCheckpointRestoreTest, restoresTheSourceCheckpointUsingItsStorageFormat)
{
    m_checkpoint.restore_legacy(m_last, GetParam().remote);

    EXPECT_EQ(111, m_checkpoint.new_time);
    EXPECT_EQ(GetParam().active_cnt, m_checkpoint.active_cnt);
    EXPECT_EQ(GetParam().desc_cnt, m_checkpoint.desc_cnt);
}

INSTANTIATE_TEST_SUITE_P(LocalAndRemote, LegacyCheckpointRestoreTest,
                         testing::Values(LegacyRestoreCase{"Remote", true, 222, 333},
                                         LegacyRestoreCase{"Local", false, 333, 0}),
                         [](const testing::TestParamInfo<LegacyRestoreCase> &info) { return info.param.name; });

struct LegacyUpdateCase
{
    const char *name;
    bool        remote;
    long        active_size;
    long        extra_num;
};

class LegacyCheckpointUpdateTest : public LegacyCheckpointTest, public testing::WithParamInterface<LegacyUpdateCase>
{
};

TEST_P(LegacyCheckpointUpdateTest, updatesOnlyTheCheckpointFieldsForTheStorageFormat)
{
    m_checkpoint.update_legacy(m_last, GetParam().remote);

    EXPECT_EQ(71, m_last.new_time);
    EXPECT_EQ(GetParam().active_size, m_last.active_size);
    EXPECT_EQ(GetParam().extra_num, m_last.extra_num);
    EXPECT_EQ("comp.saved", m_last.newsgroup_name);
    EXPECT_EQ(444, m_last.time);
    EXPECT_EQ("last-file", m_last.file);
    EXPECT_EQ(555, m_last.start_time);
}

INSTANTIATE_TEST_SUITE_P(LocalAndRemote, LegacyCheckpointUpdateTest,
                         testing::Values(LegacyUpdateCase{"Remote", true, 72, 73},
                                         LegacyUpdateCase{"Local", false, 222, 72}),
                         [](const testing::TestParamInfo<LegacyUpdateCase> &info) { return info.param.name; });

struct InfoFormatCase
{
    const char      *name;
    SourceCheckpoint checkpoint;
    const char      *group;
    const char      *expected;
};

class CheckpointInfoFormatTest : public testing::TestWithParam<InfoFormatCase>
{
};

TEST_P(CheckpointInfoFormatTest, formatsTheExactModernCheckpointRecords)
{
    const std::string text = GetParam().checkpoint.format_info(GetParam().group);

    EXPECT_EQ(GetParam().expected, text);
}

INSTANTIATE_TEST_SUITE_P(
    RecordValues, CheckpointInfoFormatTest,
    testing::Values(InfoFormatCase{"GroupAndCounts",
                                   {111, 222, 333},
                                   "comp.current",
                                   "Last-Group: comp.current\nNew-Group-State: 111,222,333\n"},
                    InfoFormatCase{"EmptyGroupAndZeros", {0, 0, 0}, "", "Last-Group: \nNew-Group-State: 0,0,0\n"},
                    InfoFormatCase{"SignedCounts",
                                   {-111, -222, -333},
                                   "comp.signed",
                                   "Last-Group: comp.signed\nNew-Group-State: -111,-222,-333\n"}),
    [](const testing::TestParamInfo<InfoFormatCase> &info) { return info.param.name; });

} // namespace
