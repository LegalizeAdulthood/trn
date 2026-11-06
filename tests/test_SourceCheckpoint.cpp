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

class LegacyCheckpointRestoreTest : public testing::TestWithParam<LegacyRestoreCase>
{
protected:
    LastRunState     m_last{"comp.saved", 444, 222, 111, 333, "last-file", 555};
    SourceCheckpoint m_checkpoint{71, 72, 73};
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

} // namespace
