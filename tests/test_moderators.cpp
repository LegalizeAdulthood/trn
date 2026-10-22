// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/moderators.h>

#include <test_config.h>
#include <trn/datasrc.h>

#include "MockNNTPConnection.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

namespace
{

using ModeratorPatternCase = std::tuple<std::string_view, std::string_view, std::string_view>;

class ModeratorPatternTest : public testing::TestWithParam<ModeratorPatternCase>
{
protected:
    void SetUp() override;

    std::string m_rules;
    std::string m_group;
    std::string m_expected;
};

void ModeratorPatternTest::SetUp()
{
    m_rules = std::get<0>(GetParam());
    m_group = std::get<1>(GetParam());
    m_expected = std::get<2>(GetParam());
}

constexpr const char *FIRST_MATCH_RULES = "# comment\n\ncomp.lang.[ac]*:specific@example.test\n"
                                          "comp.*:general@example.test\n*:fallback@example.test\n";
constexpr const char *PATTERN_LIST_RULES = "comp.*,!comp.test,news.announce:shared@example.test\n"
                                           "*:other@example.test\n";

class ModeratorSourceTest : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;
    void remote();

    DataSource                                               m_source{};
    std::filesystem::path                                    m_directory;
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
    DataSource                                              *m_old_source{};
    NNTPLink                                                 m_old_link{};
};

void ModeratorSourceTest::SetUp()
{
    const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
    m_directory = std::filesystem::path{TRN_TEST_TMP_DIR} / info->test_suite_name() / info->name();
    std::filesystem::create_directories(m_directory);
    m_source.m_moderators_file = (m_directory / "moderators").generic_string();
    m_old_source = g_data_source;
    m_old_link = g_nntp_link;
    g_data_source = nullptr;
    g_nntp_link = {};
    nntp_gets_clear_buffer();
    m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    m_source.m_nntp_link.connection = m_connection;
    m_source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
}

void ModeratorSourceTest::TearDown()
{
    g_data_source = m_old_source;
    g_nntp_link = m_old_link;
    nntp_gets_clear_buffer();
    std::error_code error;
    std::filesystem::remove_all(m_directory, error);
}

void ModeratorSourceTest::remote()
{
    m_source.m_flags = DF_REMOTE;
    m_source.m_moderators_file.clear();
    m_source.m_nntp_link.capabilities.available = true;
    m_source.m_nntp_link.capabilities.add("LIST ACTIVE MODERATORS");
}

} // namespace

TEST_P(ModeratorPatternTest, choosesTheExpectedRecipient)
{
    const std::string recipient = moderator_address(m_group, m_rules);

    EXPECT_EQ(m_expected, recipient);
}

INSTANTIATE_TEST_SUITE_P(
    FirstMatchingRule, ModeratorPatternTest,
    testing::Values(ModeratorPatternCase{FIRST_MATCH_RULES, "comp.lang.c++", "specific@example.test"},
                    ModeratorPatternCase{FIRST_MATCH_RULES, "comp.test", "general@example.test"},
                    ModeratorPatternCase{FIRST_MATCH_RULES, "news.announce", "fallback@example.test"},
                    ModeratorPatternCase{"comp.*:only@example.test\n", "misc.test", ""}));

INSTANTIATE_TEST_SUITE_P(
    PercentExpansion, ModeratorPatternTest,
    testing::Values(ModeratorPatternCase{"*:%s@example.test\n", "comp.lang.c++", "comp-lang-c++@example.test"},
                    ModeratorPatternCase{"*:tag%%%s@example.test\n", "comp.test", "tag%comp-test@example.test"}));

INSTANTIATE_TEST_SUITE_P(
    PatternListsAndExclusions, ModeratorPatternTest,
    testing::Values(ModeratorPatternCase{PATTERN_LIST_RULES, "comp.lang.apl", "shared@example.test"},
                    ModeratorPatternCase{PATTERN_LIST_RULES, "news.announce", "shared@example.test"},
                    ModeratorPatternCase{PATTERN_LIST_RULES, "comp.test", "other@example.test"}));

TEST(ModeratorPatternTest, ignoresMalformedRulesAndUnsafeAddresses)
{
    const std::string rules{" \t# comment\r\nmissing colon\n:empty@example.test\ncomp.*:\n"
                            "comp.*:bad%q@example.test\ncomp.*:%s%s@example.test\n"
                            "comp.*:two addresses@example.test\ncomp.*:bad<addr>@example.test\n"
                            " comp.* : good+tag@example.test \r\n"};

    EXPECT_EQ("good+tag@example.test", moderator_address("comp.test", rules));
    EXPECT_TRUE(moderator_address("", "*:all@example.test").empty());
    EXPECT_TRUE(moderator_address("comp.test\nBcc: injected", "*:all@example.test").empty());
}

TEST_F(ModeratorSourceTest, readsLocalFileAndSeesUpdates)
{
    std::ofstream{m_source.m_moderators_file} << "comp.*:first@example.test\n";
    EXPECT_EQ("first@example.test", m_source.find_moderator("comp.test"));
    std::ofstream{m_source.m_moderators_file} << "comp.*:second@example.test\n";
    EXPECT_EQ("second@example.test", m_source.find_moderator("comp.test"));
}

TEST_F(ModeratorSourceTest, missingFileAndNoMatchFallBack)
{
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
    std::ofstream{m_source.m_moderators_file} << "news.*:news@example.test\n";
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
}

TEST_F(ModeratorSourceTest, advertisedRemoteListIsConsumedCompletely)
{
    remote();
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST MODERATORS"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 moderator list follows"))
        .WillOnce(testing::Return("comp.*:%s@example.test"))
        .WillOnce(testing::Return("*:other@example.test"))
        .WillOnce(testing::Return("."));

    EXPECT_EQ("comp-test@example.test", m_source.find_moderator("comp.test"));
    EXPECT_EQ(nullptr, g_data_source);
    EXPECT_TRUE(m_source.m_nntp_link.flags & NNTP_NEW_CMD_OK);
}

TEST_F(ModeratorSourceTest, doesNotRequestUnadvertisedOrLegacyLists)
{
    remote();
    m_source.m_nntp_link.capabilities = {};
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
    m_source.m_nntp_link.capabilities.available = true;
    m_source.m_nntp_link.capabilities.add("LIST ACTIVE");
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
}

TEST_F(ModeratorSourceTest, explicitFileOverridesRemoteList)
{
    const std::string file = m_source.m_moderators_file;
    remote();
    m_source.m_moderators_file = file;
    std::ofstream{file} << "comp.*:local@example.test\n";

    EXPECT_EQ("local@example.test", m_source.find_moderator("comp.test"));
}

TEST_F(ModeratorSourceTest, noneDisablesRemoteLookup)
{
    remote();
    m_source.m_moderators_file = "none";
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
}

TEST_F(ModeratorSourceTest, rejectedRemoteListFallsBack)
{
    remote();
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST MODERATORS"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_)).WillOnce(testing::Return("501 unsupported list"));

    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
    EXPECT_EQ(nullptr, g_data_source);
}

TEST_F(ModeratorSourceTest, incompleteRemoteListDoesNotPublishMatchingPrefix)
{
    remote();
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST MODERATORS"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return("comp.*:premature@example.test"))
        .WillOnce(testing::Invoke(
            [](error_code_t &error)
            {
                error = boost::system::errc::make_error_code(boost::system::errc::io_error);
                return std::string{};
            }));

    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
    EXPECT_EQ(nullptr, g_data_source);
}

TEST_F(ModeratorSourceTest, consumesLongRecordsAndEmptyLists)
{
    remote();
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST MODERATORS"), testing::_)).Times(2);
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return(std::string(900, 'x') + ":ignored@example.test"))
        .WillOnce(testing::Return("comp.*:found@example.test"))
        .WillOnce(testing::Return("."))
        .WillOnce(testing::Return("215 empty list follows"))
        .WillOnce(testing::Return("."));

    EXPECT_EQ("found@example.test", m_source.find_moderator("comp.test"));
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
}

TEST_F(ModeratorSourceTest, disconnectedRemoteFallsBack)
{
    remote();
    m_source.m_nntp_link.connection.reset();
    EXPECT_TRUE(m_source.find_moderator("comp.test").empty());
}
