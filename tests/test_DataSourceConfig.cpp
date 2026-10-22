// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/DataSourceConfig.h>

#include <trn/IniDocument.h>
#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

namespace
{

class DataSourceConfigTest : public testing::Test
{
protected:
    DataSourceConfig parse(const char *text);

private:
    IniSectionValues m_values;
};

DataSourceConfig DataSourceConfigTest::parse(const char *text)
{
    IniDocument document{text, "test input"};

    auto iterator = document.begin();
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

void expect_value(std::string_view expected, std::optional<std::string_view> actual)
{
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(expected, *actual);
}

using DataSourceSchemaField = std::pair<std::string_view, bool>;

class DataSourceSchemaLookupTest : public testing::TestWithParam<DataSourceSchemaField>
{
};

using DataSourceFieldGetter = std::optional<std::string_view> (DataSourceConfig::*)() const;

struct DataSourceFieldCase
{
    const char                     *name;
    const char                     *text;
    DataSourceFieldGetter           getter;
    std::optional<std::string_view> expected;
};

void PrintTo(const DataSourceFieldCase &value, std::ostream *stream)
{
    *stream << value.name;
}

constexpr char CONFIGURED_SOURCE[]{"[source]\n"
                                   "NNTP Server = news.example.org\n"
                                   "Active File = active.cache\n"
                                   "Active File Refetch = 1 day\n"
                                   "Spool Dir = /var/spool/news\n"
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
                                   "Force Auth = yes\n"};

constexpr char PARTIAL_SOURCE[]{"[source]\nActive File = active\n"};

const DataSourceFieldCase CONFIGURED_FIELDS[]{
    {"NNTP Server", CONFIGURED_SOURCE, &DataSourceConfig::nntp_server, "news.example.org"},
    {"Active File", CONFIGURED_SOURCE, &DataSourceConfig::active_file, "active.cache"},
    {"Active File Refetch", CONFIGURED_SOURCE, &DataSourceConfig::active_file_refetch, "1 day"},
    {"Spool Dir", CONFIGURED_SOURCE, &DataSourceConfig::spool_dir, "/var/spool/news"},
    {"Overview Dir", CONFIGURED_SOURCE, &DataSourceConfig::overview_dir, "/var/overview"},
    {"Active Times", CONFIGURED_SOURCE, &DataSourceConfig::active_times, "active.times"},
    {"Group Desc", CONFIGURED_SOURCE, &DataSourceConfig::group_desc, "newsgroups"},
    {"Group Desc Refetch", CONFIGURED_SOURCE, &DataSourceConfig::group_desc_refetch, "2 days"},
    {"Auth User", CONFIGURED_SOURCE, &DataSourceConfig::auth_user, "reader"},
    {"Auth Password", CONFIGURED_SOURCE, &DataSourceConfig::auth_password, "secret"},
    {"Auth Command", CONFIGURED_SOURCE, &DataSourceConfig::auth_command, "get-auth"},
    {"XHDR Broken", CONFIGURED_SOURCE, &DataSourceConfig::xhdr_broken, "y"},
    {"Xrefs", CONFIGURED_SOURCE, &DataSourceConfig::xrefs, "n"},
    {"Overview Format File", CONFIGURED_SOURCE, &DataSourceConfig::overview_format_file, "overview.fmt"},
    {"Force Auth", CONFIGURED_SOURCE, &DataSourceConfig::force_auth, "yes"}};

class ParsedDataSourceFieldTest : public DataSourceConfigTest, public testing::WithParamInterface<DataSourceFieldCase>
{
protected:
    void SetUp() override;

    DataSourceConfig m_config;
};

void ParsedDataSourceFieldTest::SetUp()
{
    m_config = parse(GetParam().text);
}

class ModeratorFilePolicyTest : public testing::Test
{
protected:
    DataSourceConfig m_config;
};

} // namespace

TEST_P(DataSourceSchemaLookupTest, recognizesCurrentFieldsAndRejectsObsoleteNames)
{
    const IniField *field = DataSourceConfig::schema().find(GetParam().first);

    EXPECT_EQ(GetParam().second, field != nullptr);
}

INSTANTIATE_TEST_SUITE_P(SchemaNames, DataSourceSchemaLookupTest,
                         testing::Values(DataSourceSchemaField{"NNTP Server", true},
                                         DataSourceSchemaField{"Overview Format File", true},
                                         DataSourceSchemaField{"Thread Dir", false},
                                         DataSourceSchemaField{"STARTTLS", true},
                                         DataSourceSchemaField{"Require TLS For Auth", true},
                                         DataSourceSchemaField{"Require TLS For Posting", true}));

TEST_P(ParsedDataSourceFieldTest, mapsTheConfiguredFieldOrPreservesItsAbsence)
{
    const std::optional<std::string_view> value = (m_config.*GetParam().getter)();

    EXPECT_EQ(GetParam().expected, value);
}

INSTANTIATE_TEST_SUITE_P(CompleteConfiguration, ParsedDataSourceFieldTest, testing::ValuesIn(CONFIGURED_FIELDS));

INSTANTIATE_TEST_SUITE_P(
    PartialConfiguration, ParsedDataSourceFieldTest,
    testing::Values(DataSourceFieldCase{"Active File", PARTIAL_SOURCE, &DataSourceConfig::active_file, "active"},
                    DataSourceFieldCase{"NNTP Server", PARTIAL_SOURCE, &DataSourceConfig::nntp_server, std::nullopt},
                    DataSourceFieldCase{"Spool Dir", PARTIAL_SOURCE, &DataSourceConfig::spool_dir, std::nullopt}));

TEST_F(DataSourceConfigTest, recognizesImplicitTlsField)
{
    EXPECT_NE(nullptr, DataSourceConfig::schema().find("Implicit TLS"));
}

TEST_F(DataSourceConfigTest, recognizesLegacyAuthPolicy)
{
    EXPECT_NE(nullptr, DataSourceConfig::schema().find("Allow Legacy Auth"));
}

TEST_F(DataSourceConfigTest, recognizesModeratorsFile)
{
    EXPECT_NE(nullptr, DataSourceConfig::schema().find("Moderators File"));
}

INSTANTIATE_TEST_SUITE_P(ParsedModeratorFile, ParsedDataSourceFieldTest,
                         testing::Values(DataSourceFieldCase{"Moderators File",
                                                             "[source]\nModerators File = ~/moderators\n",
                                                             &DataSourceConfig::moderators_file, "~/moderators"}));

TEST_F(ModeratorFilePolicyTest, moderatorPolicyIsInitiallyAbsent)
{
    const std::optional<std::string_view> policy = m_config.moderators_file();

    EXPECT_FALSE(policy.has_value());
}

TEST_F(ModeratorFilePolicyTest, setterPreservesTheDisabledPolicy)
{
    m_config.set_moderators_file("none");

    expect_value("none", m_config.moderators_file());
}
