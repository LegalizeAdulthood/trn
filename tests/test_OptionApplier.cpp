// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionApplier.h>

#include <trn/IniSectionValues.h>
#include <trn/ng.h>
#include <trn/opt.h>
#include <trn/OptionCatalog.h>
#include <trn/OptionDraft.h>

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace
{

struct AppliedOption
{
    OptionIndex      option;
    std::string_view value;
};

std::vector<AppliedOption> s_applied_options;

struct OptionValueRestorer
{
    OptionValueList def_vals;
    OptionValueList saved_vals;
    bool            use_threads;

    ~OptionValueRestorer()
    {
        g_option_def_vals = def_vals;
        g_option_saved_vals = saved_vals;
        g_use_threads = use_threads;
    }
};

void record_option(OptionIndex option, const char *value)
{
    s_applied_options.push_back({option, value});
}

class OptionApplierTest : public testing::Test
{
protected:
    void SetUp() override
    {
        s_applied_options.clear();
    }

    void TearDown() override
    {
        s_applied_options.clear();
    }
};

} // namespace

TEST_F(OptionApplierTest, appliesOneOptionThroughSink)
{
    OptionApplier applier{record_option};

    applier.apply(OI_USE_THREADS, "yes");

    ASSERT_EQ(1U, s_applied_options.size());
    EXPECT_EQ(OI_USE_THREADS, s_applied_options[0].option);
    EXPECT_EQ(std::string_view{"yes"}, s_applied_options[0].value);
}

TEST_F(OptionApplierTest, appliesSectionValuesByOptionIndex)
{
    const OptionCatalog catalog;
    IniSectionValues    values;
    const IniField     *terse = catalog.schema().find("Terse Output");
    const IniField     *threads = catalog.schema().find("Use Threads");
    ASSERT_NE(nullptr, terse);
    ASSERT_NE(nullptr, threads);
    ASSERT_TRUE(values.set(*terse, "no"));
    ASSERT_TRUE(values.set(*threads, "yes"));

    OptionApplier applier{record_option};
    applier.apply(values);

    ASSERT_EQ(2U, s_applied_options.size());
    EXPECT_EQ(OI_TERSE_OUTPUT, s_applied_options[0].option);
    EXPECT_EQ(std::string_view{"no"}, s_applied_options[0].value);
    EXPECT_EQ(OI_USE_THREADS, s_applied_options[1].option);
    EXPECT_EQ(std::string_view{"yes"}, s_applied_options[1].value);
}

TEST_F(OptionApplierTest, appliesDraftEditsByOptionIndex)
{
    OptionDraft draft{static_cast<std::size_t>(OptionCatalog().option_limit())};
    draft.set(OI_ERASE_SCREEN, "yes");
    draft.set(OI_USE_THREADS, "no");

    OptionApplier applier{record_option};
    applier.apply(draft);

    ASSERT_EQ(2U, s_applied_options.size());
    EXPECT_EQ(OI_ERASE_SCREEN, s_applied_options[0].option);
    EXPECT_EQ(std::string_view{"yes"}, s_applied_options[0].value);
    EXPECT_EQ(OI_USE_THREADS, s_applied_options[1].option);
    EXPECT_EQ(std::string_view{"no"}, s_applied_options[1].value);
}

TEST_F(OptionApplierTest, clearsSavedValueWhenDraftRestoresSavedValue)
{
    const OptionCatalog catalog;
    OptionValueRestorer restore{g_option_def_vals, g_option_saved_vals, g_use_threads};

    g_option_def_vals = OptionValueList(static_cast<std::size_t>(catalog.option_limit()));
    g_option_saved_vals = OptionValueList(static_cast<std::size_t>(catalog.option_limit()));
    g_option_saved_vals[OI_USE_THREADS] = "yes";
    g_option_def_vals[OI_USE_THREADS] = "yes";
    g_use_threads = false;

    OptionDraft draft{static_cast<std::size_t>(catalog.option_limit())};
    draft.set(OI_USE_THREADS, "yes");

    OptionApplier{}.apply(draft);

    EXPECT_FALSE(g_option_saved_vals[OI_USE_THREADS]);
    EXPECT_TRUE(g_use_threads);
}
