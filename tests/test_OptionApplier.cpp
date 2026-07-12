// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionApplier.h>

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

TEST_F(OptionApplierTest, appliesIniValuesByOptionIndex)
{
    init_option_ini_words();
    char **values = prep_ini_words(g_options_ini);
    values[OI_TERSE_OUTPUT] = const_cast<char *>("no");
    values[OI_USE_THREADS] = const_cast<char *>("yes");

    OptionApplier applier{record_option};
    applier.apply(values);

    unprep_ini_words(g_options_ini);

    ASSERT_EQ(2U, s_applied_options.size());
    EXPECT_EQ(OI_TERSE_OUTPUT, s_applied_options[0].option);
    EXPECT_EQ(std::string_view{"no"}, s_applied_options[0].value);
    EXPECT_EQ(OI_USE_THREADS, s_applied_options[1].option);
    EXPECT_EQ(std::string_view{"yes"}, s_applied_options[1].value);
}

TEST_F(OptionApplierTest, appliesDraftEditsByOptionIndex)
{
    init_option_ini_words();
    prep_ini_words(g_options_ini);
    OptionDraft draft{static_cast<std::size_t>(ini_len(g_options_ini))};
    draft.set(OI_ERASE_SCREEN, "yes");
    draft.set(OI_USE_THREADS, "no");

    OptionApplier applier{record_option};
    applier.apply(draft);

    unprep_ini_words(g_options_ini);

    ASSERT_EQ(2U, s_applied_options.size());
    EXPECT_EQ(OI_ERASE_SCREEN, s_applied_options[0].option);
    EXPECT_EQ(std::string_view{"yes"}, s_applied_options[0].value);
    EXPECT_EQ(OI_USE_THREADS, s_applied_options[1].option);
    EXPECT_EQ(std::string_view{"no"}, s_applied_options[1].value);
}
