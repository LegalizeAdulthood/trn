// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/opt.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/charsubst.h>
#include <trn/file-contents.h>
#include <trn/head.h>
#include <trn/OptionCatalog.h>
#include <trn/OptionDraft.h>
#include <trn/rcstuff.h>
#include <trn/respond.h>
#include <trn/trn.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class CwdCheckTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_cwd = fs::current_path();
        m_old_home_dir = g_home_dir;
        m_old_priv_dir = g_priv_dir;
        m_old_verbose = g_verbose;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_root = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_home = m_root / "home";

        std::error_code error;
        fs::remove_all(m_root, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_home, error);
        ASSERT_FALSE(error) << error.message();
        fs::current_path(m_root, error);
        ASSERT_FALSE(error) << error.message();

        g_home_dir = m_home.generic_string();
        g_verbose = false;
    }

    void TearDown() override
    {
        std::error_code error;
        fs::current_path(m_old_cwd, error);
        fs::remove_all(m_root, error);

        g_home_dir = m_old_home_dir;
        g_priv_dir = m_old_priv_dir;
        g_verbose = m_old_verbose;
    }

    void expect_current_save_dir(const fs::path &path)
    {
        std::error_code error;
        EXPECT_TRUE(fs::equivalent(path, fs::current_path(), error));
        EXPECT_FALSE(error) << error.message();
        EXPECT_EQ(fs::current_path().generic_string(), g_priv_dir);
    }

    fs::path    m_old_cwd;
    fs::path    m_root;
    fs::path    m_home;
    std::string m_old_home_dir;
    std::string m_old_priv_dir;
    bool        m_old_verbose{};
};

class SaveOptionsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_def_vals = g_option_def_vals;
        m_old_saved_vals = g_option_saved_vals;

        const OptionCatalog catalog;
        g_option_def_vals = OptionValueList(static_cast<std::size_t>(catalog.option_limit()));
        g_option_saved_vals = OptionValueList(static_cast<std::size_t>(catalog.option_limit()));

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_root = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_root, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_root, error);
        ASSERT_FALSE(error) << error.message();
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_root, error);

        g_option_def_vals = m_old_def_vals;
        g_option_saved_vals = m_old_saved_vals;
    }

    fs::path        m_root;
    OptionValueList m_old_def_vals;
    OptionValueList m_old_saved_vals;
};

class UnicodeConfigurationTest : public SaveOptionsTest
{
protected:
    void SetUp() override
    {
        SaveOptionsTest::SetUp();
        m_old_profile = g_subst.unicode_substitutions;
        m_old_charsets = g_subst.charsets;
        m_old_mode = g_subst.current_mode();
        g_subst.unicode_substitutions = {};
        g_subst.charsets = "patm";
        g_subst.reset_mode();
    }

    void TearDown() override
    {
        g_subst.unicode_substitutions = m_old_profile;
        g_subst.charsets = m_old_charsets;
        g_subst.set_mode(m_old_mode);
        SaveOptionsTest::TearDown();
    }

    UnicodeSubstitutionProfile m_old_profile;
    std::string                m_old_charsets;
    char                       m_old_mode{};
};

class LoadedUnicodeMappingsTest : public UnicodeConfigurationTest
{
protected:
    void SetUp() override;

    const std::string m_text{"\xE2\x80\x99\xF0\x9F\x98\x8A"};
};

void LoadedUnicodeMappingsTest::SetUp()
{
    UnicodeConfigurationTest::SetUp();
    const fs::path path = m_root / "trnrc";
    std::ofstream{path} << "[unicode substitutions]\nU+2019 = custom\nU+1F60A = ':)'\n";
    load_options(path);
}

class EnabledUnicodeMappingsTest : public LoadedUnicodeMappingsTest
{
protected:
    void SetUp() override;
};

void EnabledUnicodeMappingsTest::SetUp()
{
    LoadedUnicodeMappingsTest::SetUp();
    set_option(OI_CHARSET, "up");
    g_subst.reset_mode();
}

class AutoSaveNameOptionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_check_flag = g_check_flag;
        m_old_save_dir = get_env_var("SAVEDIR");
        m_old_save_name = get_env_var("SAVENAME");

        g_check_flag = false;
        unset_env_var("SAVEDIR");
        unset_env_var("SAVENAME");
    }

    void TearDown() override
    {
        restore_env("SAVEDIR", m_old_save_dir);
        restore_env("SAVENAME", m_old_save_name);
        g_check_flag = m_old_check_flag;
    }

private:
    static void restore_env(std::string_view name, const std::string &value)
    {
        if (value.empty())
        {
            unset_env_var(name);
        }
        else
        {
            set_env_var(name, value);
        }
    }

    bool        m_old_check_flag{};
    std::string m_old_save_dir;
    std::string m_old_save_name;
};

class SelectorCommandOptionTest : public testing::TestWithParam<OptionIndex>
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string m_saved_value;
};

void SelectorCommandOptionTest::SetUp()
{
    m_saved_value = option_value(GetParam());
    set_option(GetParam(), "ab");
}

void SelectorCommandOptionTest::TearDown()
{
    set_option(GetParam(), m_saved_value);
}

using QuotedOptionValue = std::pair<std::string_view, std::string_view>;

class QuotedOptionValueTest : public testing::TestWithParam<QuotedOptionValue>
{
};

struct OptionGroupToggle
{
    OptionFlags initial;
    OptionFlags expected;
};

void PrintTo(const OptionGroupToggle &value, std::ostream *output)
{
    *output << (value.initial & OF_INCLUDED ? "Included" : "") << (value.initial & OF_SEL ? "Expanded" : "Collapsed");
}

class OptionGroupToggleTest : public testing::TestWithParam<OptionGroupToggle>
{
};

struct OptionEditCase
{
    const char                     *name;
    std::optional<std::string_view> initial;
    std::string_view                oldval;
    std::string_view                parsed;
    std::optional<std::string_view> expected;
    int                             delta;
};

void PrintTo(const OptionEditCase &value, std::ostream *output)
{
    *output << value.name;
}

class OptionRecordEditTest : public testing::TestWithParam<OptionEditCase>
{
protected:
    void SetUp() override;

    OptionDraft       m_draft{OI_SCORE_LAST + 1};
    OptionEditSession m_session{&m_draft, nullptr};
};

void OptionRecordEditTest::SetUp()
{
    m_draft.set(OI_SAVE_DIR, "unrelated");
    if (GetParam().initial.has_value())
    {
        m_draft.set(OI_TERSE_OUTPUT, *GetParam().initial);
    }
}

using OptionAttachment = std::pair<bool, bool>;

class OptionAttachTest : public testing::TestWithParam<OptionAttachment>
{
protected:
    OptionDraft m_previous{1};
    OptionDraft m_next{1};
};

class HeaderListOptionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        for (int i = 0; i < HEAD_LAST; ++i)
        {
            m_old_header_type[static_cast<std::size_t>(i)] = g_header_rules.type[i];
            m_old_header_positions[static_cast<std::size_t>(i)] = g_parsed_header.positions[i];
        }
        m_old_user_header_type = g_header_rules.user_type;
        m_old_user_header_type_index = g_header_rules.user_type_index;
        m_old_user_header_type_count = g_header_rules.user_type_count;
        m_old_user_header_type_max = g_header_rules.user_type_max;

        reset_user_header_type();
        set_option(OI_HEADER_HIDING, "");
        set_option(OI_HEADER_MAGIC, "");
    }

    void TearDown() override
    {
        for (int i = 0; i < HEAD_LAST; ++i)
        {
            g_header_rules.type[i] = m_old_header_type[static_cast<std::size_t>(i)];
            g_parsed_header.positions[i] = m_old_header_positions[static_cast<std::size_t>(i)];
        }
        g_header_rules.user_type = m_old_user_header_type;
        g_header_rules.user_type_index = m_old_user_header_type_index;
        g_header_rules.user_type_count = m_old_user_header_type_count;
        g_header_rules.user_type_max = m_old_user_header_type_max;
    }

    static bool has_flag(HeaderLineType header, HeaderTypeFlags flag)
    {
        return (g_header_rules.type[header].flags & flag) != HT_NONE;
    }

private:
    static void reset_user_header_type()
    {
        g_header_rules.user_type_max = 10;
        g_header_rules.user_type.assign(static_cast<std::size_t>(g_header_rules.user_type_max), UserHeaderType{});
        g_header_rules.user_type_count = 1;
        g_header_rules.user_type[0].name = "*";
        g_header_rules.user_type_index.fill(0);
    }

    std::array<HeaderType, HEAD_LAST> m_old_header_type;
    std::array<HeaderPosition, HEAD_LAST> m_old_header_positions;
    std::array<int, 26>               m_old_user_header_type_index;
    std::vector<UserHeaderType>       m_old_user_header_type;
    int                               m_old_user_header_type_count{};
    int                               m_old_user_header_type_max{};
};

} // namespace

TEST_F(CwdCheckTest, defaultsEmptySaveDirectoryToHomeNews)
{
    const fs::path save_dir = m_home / "News";
    g_priv_dir.clear();

    cwd_check();

    EXPECT_TRUE(fs::is_directory(save_dir));
    expect_current_save_dir(save_dir);
}

TEST_F(CwdCheckTest, expandsConfiguredSaveDirectoryBeforeCreatingIt)
{
    const fs::path save_dir = m_home / "Saved";
    g_priv_dir = "~/Saved";

    cwd_check();

    EXPECT_TRUE(fs::is_directory(save_dir));
    expect_current_save_dir(save_dir);
}

TEST_F(SaveOptionsTest, preservesNonOptionTextWhenReplacingOptionsSection)
{
    const fs::path path = m_root / "trnrc";
    std::ofstream{path} << "# before\n"
                        << "[environment]\n"
                        << "TERM = xterm\n"
                        << "\n"
                        << "[options]\n"
                        << "# old option comment\n"
                        << "Bogus Option = old\n"
                        << "# keep this note\n"
                        << "\n"
                        << "[extra]\n"
                        << "value = yes\n";

    save_options(path);

    const std::string output = file_contents(path);

    EXPECT_NE(std::string::npos, output.find("# before\n[environment]\nTERM = xterm\n\n[options]\n"));
    EXPECT_EQ(std::string::npos, output.find("# old option comment"));
    EXPECT_EQ(std::string::npos, output.find("Bogus Option = old"));
    EXPECT_NE(std::string::npos, output.find("# keep this note\n\n[extra]\nvalue = yes\n"));
}

TEST_F(LoadedUnicodeMappingsTest, loadingMappingsDoesNotEnableSubstitution)
{
    const std::string text = g_subst.subst(m_text, g_subst.current_mode());

    EXPECT_EQ(m_text, text);
}

TEST_F(EnabledUnicodeMappingsTest, explicitOptInUsesLoadedMappings)
{
    const std::string text = g_subst.subst(m_text, g_subst.current_mode());

    EXPECT_EQ("custom:)", text);
}

TEST_F(EnabledUnicodeMappingsTest, cyclingReturnsToPlainOutput)
{
    g_subst.next_mode();
    const std::string text = g_subst.subst(m_text, g_subst.current_mode());

    EXPECT_EQ(m_text, text);
}

TEST_F(UnicodeConfigurationTest, savesAndReloadsOptInAndEscapedMappings)
{
    const fs::path    path = m_root / "trnrc";
    const std::string section{"[unicode substitutions]\nU+2019 = \"\\t\\n\\\\\"\nU+2014 = \"\"\n"};
    std::ofstream{path} << "[options]\nCharset = up\n" << section;
    load_options(path);
    save_options(path);
    EXPECT_NE(std::string::npos, file_contents(path).find(section));

    g_subst.unicode_substitutions = {};
    g_subst.charsets = "patm";
    load_options(path);
    g_subst.reset_mode();
    EXPECT_EQ('u', g_subst.current_mode());
    EXPECT_EQ("\t\n\\", g_subst.subst("\xE2\x80\x99\xE2\x80\x94", g_subst.current_mode()));
}

TEST_F(UnicodeConfigurationTest, invalidSectionReportsErrorAndKeepsPreviousProfile)
{
    const fs::path path = m_root / "trnrc";
    std::ofstream{path} << "[options]\nCharset = up\n[unicode substitutions]\nU+2019 = old\n";
    load_options(path);
    std::ofstream{path} << "[unicode substitutions]\nU+2019 = new\n0x91 = invalid\n";
    testing::internal::CaptureStdout();
    load_options(path);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("Invalid Unicode substitution section"));
    g_subst.reset_mode();
    EXPECT_EQ("old", g_subst.subst("\xE2\x80\x99", g_subst.current_mode()));
}

TEST(QuoteStringTest, leavesSimpleValuesUnquoted)
{
    EXPECT_EQ(std::string{"plain-value"}, quote_string("plain-value"));
}

TEST_P(QuotedOptionValueTest, quotesAndEscapesTheValue)
{
    const std::string quoted = quote_string(GetParam().first);

    EXPECT_EQ(GetParam().second, quoted);
}

INSTANTIATE_TEST_SUITE_P(
    ValuesNeedingQuotes, QuotedOptionValueTest,
    testing::Values(QuotedOptionValue{" leading", "\" leading\""}, QuotedOptionValue{"two  spaces", "\"two  spaces\""},
                    QuotedOptionValue{"value#comment", "\"value#comment\""}, QuotedOptionValue{"can't", "\"can't\""},
                    QuotedOptionValue{"say \"hi\"", "'say \"hi\"'"}, QuotedOptionValue{"c:\\tmp", "\"c:\\\\tmp\""}));

TEST_F(AutoSaveNameOptionTest, yesUsesArticleNameInNewsgroupDirectory)
{
    set_option(OI_AUTO_SAVE_NAME, "yes");

    EXPECT_EQ("%p/%c", get_env_var("SAVEDIR"));
    EXPECT_EQ("%a", get_env_var("SAVENAME"));
}

TEST_F(AutoSaveNameOptionTest, noRestoresClassicSaveDefaultsWhenAutoDefaultsActive)
{
    set_env_var("SAVEDIR", "%p/%c");
    set_env_var("SAVENAME", "%a");

    set_option(OI_AUTO_SAVE_NAME, "no");

    EXPECT_EQ("%p", get_env_var("SAVEDIR"));
    EXPECT_EQ("%^C", get_env_var("SAVENAME"));
}

TEST_F(AutoSaveNameOptionTest, checkModeDoesNotChangeSaveDefaults)
{
    set_env_var("SAVEDIR", "%p/%c");
    set_env_var("SAVENAME", "%a");
    g_check_flag = true;

    set_option(OI_AUTO_SAVE_NAME, "no");

    EXPECT_EQ("%p/%c", get_env_var("SAVEDIR"));
    EXPECT_EQ("%a", get_env_var("SAVENAME"));
}

TEST_F(AutoSaveNameOptionTest, optionValueReportsYesForCategorySaveDirectory)
{
    set_env_var("SAVEDIR", "%p/%c");

    EXPECT_EQ("yes", option_value(OI_AUTO_SAVE_NAME));
}

TEST_F(AutoSaveNameOptionTest, optionValueReportsNoForPrivateSaveDirectory)
{
    set_env_var("SAVEDIR", "%p");

    EXPECT_EQ("no", option_value(OI_AUTO_SAVE_NAME));
}

TEST_F(AutoSaveNameOptionTest, optionValueFallsBackToCompiledSaveDirectory)
{
    const bool uses_category_save_directory = std::string_view{SAVEDIR} == "%p/%c";

    EXPECT_EQ(yes_or_no(uses_category_save_directory), option_value(OI_AUTO_SAVE_NAME));
}

TEST_P(SelectorCommandOptionTest, commandOptionKeepsOnlyFirstTwoCharacters)
{
    set_option(GetParam(), "xy");

    set_option(GetParam(), "abcd");

    EXPECT_EQ("ab", option_value(GetParam()));
}

TEST_P(SelectorCommandOptionTest, oneCharacterCommandOptionPreservesPageCommand)
{
    set_option(GetParam(), "x");

    EXPECT_EQ("xb", option_value(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Selectors, SelectorCommandOptionTest,
                         testing::Values(OI_UNIV_SEL_CMDS, OI_NEWSRC_SEL_CMDS, OI_ADD_SEL_CMDS, OI_NEWSGROUP_SEL_CMDS,
                                         OI_NEWS_SEL_CMDS, OI_OPTION_SEL_CMDS));

TEST_P(OptionGroupToggleTest, togglesOnlyTheRequestedGroupsSelectionBit)
{
    OptionFlags       flags[]{OF_INCLUDED, GetParam().initial, OF_SEL};
    OptionEditSession session{nullptr, flags};

    session.toggle_group(1);

    EXPECT_EQ(GetParam().expected, flags[1]);
    EXPECT_EQ(OF_INCLUDED, flags[0]);
    EXPECT_EQ(OF_SEL, flags[2]);
}

INSTANTIATE_TEST_SUITE_P(SelectionAndInclusion, OptionGroupToggleTest,
                         testing::Values(OptionGroupToggle{OF_NONE, OF_SEL}, OptionGroupToggle{OF_SEL, OF_NONE},
                                         OptionGroupToggle{OF_INCLUDED, OF_INCLUDED | OF_SEL},
                                         OptionGroupToggle{OF_INCLUDED | OF_SEL, OF_INCLUDED}));

TEST_P(OptionRecordEditTest, recordsThePendingEditAndReturnsItsCountAdjustment)
{
    const OptionEditCase &edit = GetParam();

    const int delta = m_session.record_edit(OI_TERSE_OUTPUT, edit.initial.has_value(), edit.oldval, edit.parsed);

    EXPECT_EQ(edit.delta, delta);
    EXPECT_EQ(edit.expected, m_draft.value(OI_TERSE_OUTPUT));
    EXPECT_EQ(std::optional<std::string_view>{"unrelated"}, m_draft.value(OI_SAVE_DIR));
}

INSTANTIATE_TEST_SUITE_P(
    PendingEdits, OptionRecordEditTest,
    testing::Values(OptionEditCase{"NewValue", std::nullopt, "current", "new", "new", 1},
                    OptionEditCase{"NewValueEqualToCurrent", std::nullopt, "current", "current", "current", 1},
                    OptionEditCase{"ReplaceValue", "pending", "current", "new", "new", 0},
                    OptionEditCase{"RestoreCurrent", "pending", "current", "current", std::nullopt, -1},
                    OptionEditCase{"NewEmpty", std::nullopt, "current", "", "", 1},
                    OptionEditCase{"ReplaceWithEmpty", "pending", "current", "", "", 0},
                    OptionEditCase{"RestoreEmpty", "pending", "", "", std::nullopt, -1},
                    OptionEditCase{"QuotedCurrent", "pending", "\"current value\"", "current value", "current value",
                                   0}));

TEST_P(OptionAttachTest, exchangesTheDraftAndReturnsThePreviousAttachment)
{
    OptionDraft      *old_draft = GetParam().first ? &m_previous : nullptr;
    OptionDraft      *new_draft = GetParam().second ? &m_next : nullptr;
    OptionFlags       flags[]{OF_INCLUDED};
    OptionEditSession session{old_draft, flags};

    OptionDraft *previous = session.attach(new_draft);

    EXPECT_EQ(old_draft, previous);
    EXPECT_EQ(new_draft, session.draft);
    EXPECT_EQ(flags, session.flags);
}

INSTANTIATE_TEST_SUITE_P(PreviousAndNewAttachment, OptionAttachTest,
                         testing::Values(OptionAttachment{false, true}, OptionAttachment{true, true},
                                         OptionAttachment{true, false}, OptionAttachment{false, false}));

TEST_F(HeaderListOptionTest, hidingOptionParsesCommaSeparatedOverrides)
{
    set_option(OI_HEADER_HIDING, "subject,!xref,x-widget");

    EXPECT_TRUE(has_flag(SUBJ_LINE, HT_HIDE));
    EXPECT_FALSE(has_flag(XREF_LINE, HT_HIDE));
    EXPECT_EQ("subject,!xref,x-widget", option_value(OI_HEADER_HIDING));
}

TEST_F(HeaderListOptionTest, magicOptionParsesCommaSeparatedOverrides)
{
    set_option(OI_HEADER_MAGIC, "!date,from");

    EXPECT_FALSE(has_flag(DATE_LINE, HT_MAGIC));
    EXPECT_TRUE(has_flag(FROM_LINE, HT_MAGIC));
    EXPECT_EQ("!date,from", option_value(OI_HEADER_MAGIC));
}

TEST_F(HeaderListOptionTest, emptyHeaderListRestoresDefaults)
{
    set_option(OI_HEADER_MAGIC, "!date,from");

    set_option(OI_HEADER_MAGIC, "");

    EXPECT_TRUE(has_flag(DATE_LINE, HT_MAGIC));
    EXPECT_FALSE(has_flag(FROM_LINE, HT_MAGIC));
    EXPECT_EQ("", option_value(OI_HEADER_MAGIC));
}
