// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/DataSourceConfig.h>

#include <trn/IniDocument.h>
#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <optional>
#include <string_view>

namespace
{

class DataSourceConfigTest : public testing::Test
{
protected:
    DataSourceConfig parse(const char *text)
    {
        IniDocument document{text, "test input"};

        IniDocument::Iterator iterator = document.begin();
        if (iterator == document.end())
        {
            ADD_FAILURE() << "missing section";
            return {};
        }
        const IniSection &section = *iterator;
        EXPECT_EQ(std::string_view{"source"}, section.name());
        EXPECT_TRUE(section.condition().empty());

        parse_ini_section(section, DataSourceConfig::schema(), m_values);
        return DataSourceConfig::from(m_values);
    }

private:
    IniSectionValues m_values;
};

void expect_value(std::string_view expected, std::optional<std::string_view> actual)
{
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(expected, *actual);
}

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

    expect_value("news.example.org", config.nntp_server());
    expect_value("active.cache", config.active_file());
    expect_value("1 day", config.active_file_refetch());
    expect_value("/var/spool/news", config.spool_dir());
    expect_value("/var/threads", config.thread_dir());
    expect_value("/var/overview", config.overview_dir());
    expect_value("active.times", config.active_times());
    expect_value("newsgroups", config.group_desc());
    expect_value("2 days", config.group_desc_refetch());
    expect_value("reader", config.auth_user());
    expect_value("secret", config.auth_password());
    expect_value("get-auth", config.auth_command());
    expect_value("y", config.xhdr_broken());
    expect_value("n", config.xrefs());
    expect_value("overview.fmt", config.overview_format_file());
    expect_value("yes", config.force_auth());
}

TEST_F(DataSourceConfigTest, missingValuesRemainAbsent)
{
    const DataSourceConfig config = parse("[source]\nActive File = active\n");

    expect_value("active", config.active_file());
    EXPECT_FALSE(config.nntp_server().has_value());
    EXPECT_FALSE(config.spool_dir().has_value());
    EXPECT_FALSE(config.thread_dir().has_value());
}
