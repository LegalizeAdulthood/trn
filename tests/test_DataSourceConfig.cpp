// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/DataSourceConfig.h>

#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <cstring>

namespace
{

class DataSourceConfigTest : public testing::Test
{
protected:
    DataSourceConfig parse(const char *text)
    {
        std::strncpy(m_buffer, text, sizeof m_buffer);
        m_buffer[sizeof m_buffer - 1] = '\0';

        prep_ini_data(m_buffer, "test input");

        char *section{};
        char *condition{};
        char *section_body = next_ini_section(m_buffer, &section, &condition);
        EXPECT_STREQ("source", section);
        EXPECT_STREQ("", condition);
        EXPECT_NE(nullptr, section_body);

        parse_ini_section(section_body, DataSourceConfig::schema(), m_values);
        return DataSourceConfig::from(m_values);
    }

private:
    IniSectionValues m_values;
    char             m_buffer[1024]{};
};

} // namespace

TEST_F(DataSourceConfigTest, schemaRecognizesDataSourceFields)
{
    const IniSchema &schema = DataSourceConfig::schema();

    ASSERT_NE(nullptr, schema.find("NNTP Server"));
    ASSERT_NE(nullptr, schema.find("Thread Dir"));
    ASSERT_NE(nullptr, schema.find("Overview Format File"));
    EXPECT_EQ(static_cast<int>(DataSourceConfigField::ThreadDir), schema.find("Thread Dir")->id());
}

TEST_F(DataSourceConfigTest, parsesValuesIntoNamedFields)
{
    const DataSourceConfig config = parse("[source]\n"
                                          "NNTP Server = news.example.org\n"
                                          "Active File = active.cache\n"
                                          "Active File Refetch = 1 day\n"
                                          "Spool Dir = /var/spool/news\n"
                                          "Thread Dir = /var/threads\n"
                                          "Overview Dir = /var/overview\n"
                                          "Active Times = active.times\n"
                                          "Group Desc = newsgroups\n"
                                          "Group Desc Refetch = 2 days\n"
                                          "Auth User = reader\n"
                                          "Auth Password = secret\n"
                                          "Auth Command = get-auth\n"
                                          "XHDR Broken = y\n"
                                          "Xrefs = n\n"
                                          "Overview Format File = overview.fmt\n"
                                          "Force Auth = yes\n");

    EXPECT_STREQ("news.example.org", config.nntp_server());
    EXPECT_STREQ("active.cache", config.active_file());
    EXPECT_STREQ("1 day", config.active_file_refetch());
    EXPECT_STREQ("/var/spool/news", config.spool_dir());
    EXPECT_STREQ("/var/threads", config.thread_dir());
    EXPECT_STREQ("/var/overview", config.overview_dir());
    EXPECT_STREQ("active.times", config.active_times());
    EXPECT_STREQ("newsgroups", config.group_desc());
    EXPECT_STREQ("2 days", config.group_desc_refetch());
    EXPECT_STREQ("reader", config.auth_user());
    EXPECT_STREQ("secret", config.auth_password());
    EXPECT_STREQ("get-auth", config.auth_command());
    EXPECT_STREQ("y", config.xhdr_broken());
    EXPECT_STREQ("n", config.xrefs());
    EXPECT_STREQ("overview.fmt", config.overview_format_file());
    EXPECT_STREQ("yes", config.force_auth());
}

TEST_F(DataSourceConfigTest, missingValuesRemainNull)
{
    const DataSourceConfig config = parse("[source]\nActive File = active\n");

    EXPECT_STREQ("active", config.active_file());
    EXPECT_EQ(nullptr, config.nntp_server());
    EXPECT_EQ(nullptr, config.spool_dir());
    EXPECT_EQ(nullptr, config.thread_dir());
}
