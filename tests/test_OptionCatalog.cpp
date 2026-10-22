// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionCatalog.h>

#include <trn/IniSchema.h>

#include <gtest/gtest.h>

#include <string_view>
#include <utility>

namespace
{

class OptionCatalogFixture : public testing::Test
{
protected:
    const OptionCatalog m_catalog;
};

using CatalogSchemaField = std::pair<std::string_view, OptionIndex>;

class CatalogSchemaLookupTest : public OptionCatalogFixture, public testing::WithParamInterface<CatalogSchemaField>
{
};

using CatalogHeading = std::pair<int, std::string_view>;

class CatalogHeadingTest : public OptionCatalogFixture, public testing::WithParamInterface<CatalogHeading>
{
};

using CatalogOptionRow = std::pair<OptionIndex, int>;

class CatalogOptionRowTest : public OptionCatalogFixture, public testing::WithParamInterface<CatalogOptionRow>
{
};

using CatalogPreviousGroup = std::pair<int, int>;

class CatalogPreviousGroupTest : public OptionCatalogFixture, public testing::WithParamInterface<CatalogPreviousGroup>
{
};

} // namespace

TEST_P(CatalogSchemaLookupTest, recognizesTheOptionNameIgnoringCase)
{
    const IniField *field = m_catalog.schema().find(GetParam().first);

    ASSERT_NE(nullptr, field);
    EXPECT_EQ(GetParam().second, field->id());
}

INSTANTIATE_TEST_SUITE_P(OptionNames, CatalogSchemaLookupTest,
                         testing::Values(CatalogSchemaField{"terse output", OI_TERSE_OUTPUT},
                                         CatalogSchemaField{"USE THREADS", OI_USE_THREADS}));

TEST_F(OptionCatalogFixture, threadOptionAdvertisesBooleanHelp)
{
    const IniField *threads = m_catalog.schema().find("USE THREADS");
    ASSERT_NE(nullptr, threads);

    const std::string_view help = threads->help();

    EXPECT_EQ(std::string_view{"yes/no"}, help);
}

TEST_F(OptionCatalogFixture, countsDisplayRows)
{
    const int count = m_catalog.row_count();

    EXPECT_EQ(95, count);
}

TEST_F(OptionCatalogFixture, reportsTheOptionIndexLimit)
{
    const int limit = m_catalog.option_limit();

    EXPECT_EQ(96, limit);
}

TEST_P(CatalogHeadingTest, retainsTheGroupHeadingAndTitle)
{
    const bool             is_group = m_catalog.is_group(GetParam().first);
    const std::string_view name = m_catalog.name(GetParam().first);

    EXPECT_TRUE(is_group);
    EXPECT_EQ(GetParam().second, name);
}

INSTANTIATE_TEST_SUITE_P(GroupRows, CatalogHeadingTest,
                         testing::Values(CatalogHeading{1, "Display Options"}, CatalogHeading{10, "Selector Options"},
                                         CatalogHeading{80, "Article Scan Mode Options"}));

TEST_P(CatalogOptionRowTest, mapsTheOptionToItsDisplayRow)
{
    const int row = m_catalog.row_for(GetParam().first);

    EXPECT_EQ(GetParam().second, row);
}

INSTANTIATE_TEST_SUITE_P(ValidOptions, CatalogOptionRowTest,
                         testing::Values(CatalogOptionRow{OI_TERSE_OUTPUT, 2}, CatalogOptionRow{OI_USE_THREADS, 32},
                                         CatalogOptionRow{OI_SC_VERBOSE, 95}));

INSTANTIATE_TEST_SUITE_P(MissingOption, CatalogOptionRowTest, testing::Values(CatalogOptionRow{OI_NONE, 0}));

TEST_F(OptionCatalogFixture, mapsTheLastDisplayRowBackToItsOption)
{
    const OptionIndex option = m_catalog.option(95);

    EXPECT_EQ(OI_SC_VERBOSE, option);
}

TEST_P(CatalogPreviousGroupTest, findsTheGroupBeforeTheOptionRow)
{
    const int group = m_catalog.previous_group_row(GetParam().first);

    EXPECT_EQ(GetParam().second, group);
}

INSTANTIATE_TEST_SUITE_P(OptionRows, CatalogPreviousGroupTest,
                         testing::Values(CatalogPreviousGroup{2, 1}, CatalogPreviousGroup{44, 31},
                                         CatalogPreviousGroup{95, 94}));
