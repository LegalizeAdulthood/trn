// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/SourceCheckpoint.h>

#include <trn/datasrc.h>
#include <trn/last.h>
#include <trn/NewsrcCheckpoint.h>
#include <trn/rcstuff.h>
#include <trn/trn.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

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

namespace fs = std::filesystem;

class NewsrcCheckpointFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
        m_dir = fs::path{TRN_TEST_TMP_DIR} / info->test_suite_name() / info->name();
        std::error_code error;
        fs::remove_all(m_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_dir, error);
        ASSERT_FALSE(error) << error.message();

        m_rc.data_source = &m_source;
        m_rc.info_name = m_dir / "newsrc.info";
        m_source.m_last_new_group = 84;
        m_source.m_act_sf.m_recent_cnt = 82;
        m_source.m_desc_sf.m_recent_cnt = 83;
        g_last_run = {"comp.prior", 44, 55, 81, 66, (m_dir / "last-run").generic_string(), 77};
        g_newsgroup_name = "comp.current";
    }

    void TearDown() override
    {
        g_last_run = std::move(m_saved_last);
        g_newsgroup_name = std::move(m_saved_group);
        std::error_code error;
        fs::remove_all(m_dir, error);
        EXPECT_FALSE(error) << error.message();
    }

    void write_file(const fs::path &path, std::string_view text)
    {
        std::ofstream output{path};
        ASSERT_TRUE(output);
        output << text;
        output.close();
        ASSERT_TRUE(output);
    }

    std::string read_file(const fs::path &path) const
    {
        std::ifstream input{path};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

    fs::path         m_dir;
    DataSource       m_source{};
    Newsrc           m_rc{};
    NewsrcCheckpoint m_checkpoint{m_rc};

private:
    LastRunState m_saved_last{g_last_run};
    std::string  m_saved_group{g_newsgroup_name};
};

struct ModernRestoreCase
{
    const char *name;
    const char *info;
    long        time;
    long        active_cnt;
    long        desc_cnt;
    const char *group;
};

class ModernCheckpointRestoreTest : public NewsrcCheckpointFileTest,
                                    public testing::WithParamInterface<ModernRestoreCase>
{
protected:
    void SetUp() override
    {
        NewsrcCheckpointFileTest::SetUp();
        if (GetParam().info != nullptr)
        {
            write_file(m_rc.info_name, GetParam().info);
        }
    }
};

TEST_P(ModernCheckpointRestoreTest, restoresOnlyTheFieldsConvertedFromTheInfoFile)
{
    m_checkpoint.restore();

    EXPECT_EQ(GetParam().time, m_source.m_last_new_group);
    EXPECT_EQ(GetParam().time, g_last_run.new_time);
    EXPECT_EQ(GetParam().active_cnt, m_source.m_act_sf.m_recent_cnt);
    EXPECT_EQ(GetParam().desc_cnt, m_source.m_desc_sf.m_recent_cnt);
    EXPECT_EQ(GetParam().group, g_last_run.newsgroup_name);
}

INSTANTIATE_TEST_SUITE_P(
    InfoRecords, ModernCheckpointRestoreTest,
    testing::Values(
        ModernRestoreCase{"Complete", "Last-Group: comp.loaded\nNew-Group-State: 111,222,333\n", 111, 222, 333,
                          "comp.loaded"},
        ModernRestoreCase{"Missing", nullptr, 81, 82, 83, "comp.prior"},
        ModernRestoreCase{"Empty", "", 81, 82, 83, "comp.prior"},
        ModernRestoreCase{"BlankFirstRecord", "\nNew-Group-State: 111,222,333\n", 111, 222, 333, "comp.prior"},
        ModernRestoreCase{"UnterminatedFirstRecord", "Last-Group: comp.loaded", 81, 82, 83, "comp.loaded"},
        ModernRestoreCase{"OtherFirstLabel", "Other: comp.other\nNew-Group-State: 111,222,333\n", 111, 222, 333,
                          "comp.other"},
        ModernRestoreCase{"NoGroupColon", "Last-Group comp.loaded\nNew-Group-State: 111,222,333\n", 111, 222, 333,
                          "comp.prior"},
        ModernRestoreCase{"NoGroupSpace", "Last-Group:comp.loaded\nNew-Group-State: 111,222,333\n", 111, 222, 333,
                          "comp.prior"},
        ModernRestoreCase{"EmptyGroup", "Last-Group: \nNew-Group-State: 111,222,333\n", 111, 222, 333, "comp.prior"},
        ModernRestoreCase{"TrailingGroupSpace", "Last-Group: comp.loaded \nNew-Group-State: 111,222,333\n", 111, 222,
                          333, "comp.loaded "},
        ModernRestoreCase{"OtherStateLabel", "Last-Group: comp.loaded\nOther: 111,222,333\n", 81, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"OnlyTimestamp", "Last-Group: comp.loaded\nNew-Group-State: 111\n", 111, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"OnlyTwoNumbers", "Last-Group: comp.loaded\nNew-Group-State: 111,222\n", 111, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"BadTimestamp", "Last-Group: comp.loaded\nNew-Group-State: bad,222,333\n", 81, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"BadActiveCount", "Last-Group: comp.loaded\nNew-Group-State: 111,bad,333\n", 111, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"BadDescriptionCount", "Last-Group: comp.loaded\nNew-Group-State: 111,222,bad\n", 111, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"LeadingStateSpace", "Last-Group: comp.loaded\n New-Group-State: 111,222,333\n", 81, 82, 83,
                          "comp.loaded"},
        ModernRestoreCase{"NumbersAcrossLines", "Last-Group: comp.loaded\nNew-Group-State: 111,\n222,\n333\n", 111, 222,
                          333, "comp.loaded"},
        ModernRestoreCase{"TrailingStateText", "Last-Group: comp.loaded\nNew-Group-State: 111,222,333suffix\n", 111,
                          222, 333, "comp.loaded"},
        ModernRestoreCase{"ZeroTimestamp", "Last-Group: comp.loaded\nNew-Group-State: 0,222,333\n", 0, 222, 333,
                          "comp.loaded"}),
    [](const testing::TestParamInfo<ModernRestoreCase> &info) { return info.param.name; });

struct LegacyFileRestoreCase
{
    const char *name;
    bool        remote;
    const char *text;
    long        time;
    long        active_cnt;
    long        desc_cnt;
    const char *group;
};

class LegacyCheckpointFileRestoreTest : public NewsrcCheckpointFileTest,
                                        public testing::WithParamInterface<LegacyFileRestoreCase>
{
protected:
    void SetUp() override
    {
        NewsrcCheckpointFileTest::SetUp();
        m_rc.info_name.clear();
        m_source.m_flags = GetParam().remote ? DF_REMOTE : DF_NONE;
        if (GetParam().text != nullptr)
        {
            write_file(g_last_run.file, GetParam().text);
        }
    }
};

TEST_P(LegacyCheckpointFileRestoreTest, restoresTheLegacyFileUsingTheSourceStorageFormat)
{
    m_checkpoint.restore();

    EXPECT_EQ(GetParam().time, m_source.m_last_new_group);
    EXPECT_EQ(GetParam().active_cnt, m_source.m_act_sf.m_recent_cnt);
    EXPECT_EQ(GetParam().desc_cnt, m_source.m_desc_sf.m_recent_cnt);
    EXPECT_EQ(GetParam().group, g_last_run.newsgroup_name);
}

INSTANTIATE_TEST_SUITE_P(
    LegacyRecords, LegacyCheckpointFileRestoreTest,
    testing::Values(
        LegacyFileRestoreCase{"Remote", true, "comp.loaded\n444\n222\n111\n333\n", 111, 222, 333, "comp.loaded"},
        LegacyFileRestoreCase{"Local", false, "comp.loaded\n444\n222\n111\n333\n", 111, 333, 0, "comp.loaded"},
        LegacyFileRestoreCase{"MissingRemote", true, nullptr, 81, 55, 66, "comp.prior"},
        LegacyFileRestoreCase{"MissingLocal", false, nullptr, 81, 66, 0, "comp.prior"},
        LegacyFileRestoreCase{"LegacyZeroTime", true, "comp.loaded\n444\n222\n0\n333\n", 77, 222, 333, "comp.loaded"}),
    [](const testing::TestParamInfo<LegacyFileRestoreCase> &info) { return info.param.name; });

TEST_F(NewsrcCheckpointFileTest, savesLiveSourceValuesWithTheCurrentGroupName)
{
    m_source.m_last_new_group = 111;
    m_source.m_act_sf.m_recent_cnt = 222;
    m_source.m_desc_sf.m_recent_cnt = 333;

    m_checkpoint.save();

    EXPECT_EQ("Last-Group: comp.current\nNew-Group-State: 111,222,333\n", read_file(m_rc.info_name));
}

TEST_F(NewsrcCheckpointFileTest, unavailableInfoOutputDoesNotFallBackToTheLegacyFile)
{
    m_rc.info_name = m_dir / "missing" / "newsrc.info";

    m_checkpoint.save();

    EXPECT_FALSE(fs::exists(m_rc.info_name));
    EXPECT_FALSE(fs::exists(g_last_run.file));
}

struct LegacyFileSaveCase
{
    const char *name;
    bool        remote;
    const char *expected;
};

class LegacyCheckpointFileSaveTest : public NewsrcCheckpointFileTest,
                                     public testing::WithParamInterface<LegacyFileSaveCase>
{
protected:
    void SetUp() override
    {
        NewsrcCheckpointFileTest::SetUp();
        m_rc.info_name.clear();
        m_source.m_flags = GetParam().remote ? DF_REMOTE : DF_NONE;
        m_source.m_last_new_group = 911;
        m_source.m_act_sf.m_recent_cnt = 922;
        m_source.m_desc_sf.m_recent_cnt = 933;
        write_file(g_last_run.file, "comp.loaded\n444\n222\n111\n333\n");
    }
};

TEST_P(LegacyCheckpointFileSaveTest, reloadsTheLegacyRecordBeforeUpdatingItsCheckpointFields)
{
    m_checkpoint.save();

    EXPECT_EQ(GetParam().expected, read_file(g_last_run.file));
}

INSTANTIATE_TEST_SUITE_P(LocalAndRemote, LegacyCheckpointFileSaveTest,
                         testing::Values(LegacyFileSaveCase{"Remote", true, "comp.current\n444\n922\n911\n933\n"},
                                         LegacyFileSaveCase{"Local", false, "comp.current\n444\n222\n911\n922\n"}),
                         [](const testing::TestParamInfo<LegacyFileSaveCase> &info) { return info.param.name; });

} // namespace
