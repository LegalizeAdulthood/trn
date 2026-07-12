// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionCatalog.h>

#include <trn/IniSchema.h>

#include <gtest/gtest.h>

#include <string_view>

TEST(OptionCatalogTest, schemaRecognizesOptionsByName)
{
    const OptionCatalog &catalog = OptionCatalog::instance();
    const IniSchema     &schema = catalog.schema();

    const IniField *terse = schema.find("terse output");
    const IniField *threads = schema.find("USE THREADS");

    ASSERT_NE(nullptr, terse);
    ASSERT_NE(nullptr, threads);
    EXPECT_EQ(OI_TERSE_OUTPUT, terse->id());
    EXPECT_EQ(OI_USE_THREADS, threads->id());
    EXPECT_EQ(std::string_view{"yes/no"}, threads->help());
}

TEST(OptionCatalogTest, displayRowsPreserveGroups)
{
    const OptionCatalog &catalog = OptionCatalog::instance();

    EXPECT_EQ(95, catalog.row_count());
    EXPECT_EQ(96, catalog.option_limit());
    EXPECT_TRUE(catalog.is_group(1));
    EXPECT_EQ(std::string_view{"Display Options"}, catalog.name(1));
    EXPECT_TRUE(catalog.is_group(10));
    EXPECT_EQ(std::string_view{"Selector Options"}, catalog.name(10));
    EXPECT_TRUE(catalog.is_group(80));
    EXPECT_EQ(std::string_view{"Article Scan Mode Options"}, catalog.name(80));
}

TEST(OptionCatalogTest, mapsOptionIndexesToDisplayRows)
{
    const OptionCatalog &catalog = OptionCatalog::instance();

    EXPECT_EQ(2, catalog.row_for(OI_TERSE_OUTPUT));
    EXPECT_EQ(32, catalog.row_for(OI_USE_THREADS));
    EXPECT_EQ(95, catalog.row_for(OI_SC_VERBOSE));
    EXPECT_EQ(OI_SC_VERBOSE, catalog.option(95));
    EXPECT_EQ(0, catalog.row_for(OI_NONE));
}

TEST(OptionCatalogTest, findsPreviousGroupForOptionRows)
{
    const OptionCatalog &catalog = OptionCatalog::instance();

    EXPECT_EQ(1, catalog.previous_group_row(2));
    EXPECT_EQ(31, catalog.previous_group_row(44));
    EXPECT_EQ(94, catalog.previous_group_row(95));
}
