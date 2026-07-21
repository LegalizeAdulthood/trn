// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/opt.h>

#include <file_contents.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/head.h>
#include <trn/OptionCatalog.h>
#include <trn/rcstuff.h>
#include <trn/respond.h>
#include <trn/trn.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
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

struct SavedSelectorCommand
{
    OptionIndex option;
    std::string value;
};

class SelectorCommandOptionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        for (SavedSelectorCommand &saved : m_saved_values)
        {
            saved.value = option_value(saved.option);
        }
    }

    void TearDown() override
    {
        for (const SavedSelectorCommand &saved : m_saved_values)
        {
            set_option(saved.option, saved.value);
        }
    }

private:
    std::array<SavedSelectorCommand, 6> m_saved_values{{
        {OI_UNIV_SEL_CMDS, {}},
        {OI_NEWSRC_SEL_CMDS, {}},
        {OI_ADD_SEL_CMDS, {}},
        {OI_NEWSGROUP_SEL_CMDS, {}},
        {OI_NEWS_SEL_CMDS, {}},
        {OI_OPTION_SEL_CMDS, {}},
    }};
};

class HeaderListOptionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        for (int i = 0; i < HEAD_LAST; ++i)
        {
            m_old_header_type[static_cast<std::size_t>(i)] = g_header_type[i];
        }
        m_old_user_header_type = g_user_header_type;
        for (int i = 0; i < 26; ++i)
        {
            m_old_user_header_type_index[static_cast<std::size_t>(i)] = g_user_header_type_index[i];
        }
        m_old_user_header_type_count = g_user_header_type_count;
        m_old_user_header_type_max = g_user_header_type_max;

        reset_user_header_type();
        set_option(OI_HEADER_HIDING, "");
        set_option(OI_HEADER_MAGIC, "");
    }

    void TearDown() override
    {
        for (int i = 0; i < HEAD_LAST; ++i)
        {
            g_header_type[i] = m_old_header_type[static_cast<std::size_t>(i)];
        }
        g_user_header_type = m_old_user_header_type;
        for (int i = 0; i < 26; ++i)
        {
            g_user_header_type_index[i] = m_old_user_header_type_index[static_cast<std::size_t>(i)];
        }
        g_user_header_type_count = m_old_user_header_type_count;
        g_user_header_type_max = m_old_user_header_type_max;
    }

    static bool has_flag(HeaderLineType header, HeaderTypeFlags flag)
    {
        return (g_header_type[header].flags & flag) != HT_NONE;
    }

private:
    static void reset_user_header_type()
    {
        g_user_header_type_max = 10;
        g_user_header_type.assign(static_cast<std::size_t>(g_user_header_type_max), UserHeaderType{});
        g_user_header_type_count = 1;
        g_user_header_type[0].name = "*";
        for (short &index : g_user_header_type_index)
        {
            index = 0;
        }
    }

    std::array<HeaderType, HEAD_LAST> m_old_header_type;
    std::array<short, 26>             m_old_user_header_type_index;
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

TEST(QuoteStringTest, leavesSimpleValuesUnquoted)
{
    EXPECT_EQ(std::string{"plain-value"}, quote_string("plain-value"));
}

TEST(QuoteStringTest, quotesWhitespaceAndComments)
{
    EXPECT_EQ(std::string{"\" leading\""}, quote_string(" leading"));
    EXPECT_EQ(std::string{"\"two  spaces\""}, quote_string("two  spaces"));
    EXPECT_EQ(std::string{"\"value#comment\""}, quote_string("value#comment"));
}

TEST(QuoteStringTest, escapesChosenQuoteAndBackslash)
{
    EXPECT_EQ(std::string{"\"can't\""}, quote_string("can't"));
    EXPECT_EQ(std::string{"'say \"hi\"'"}, quote_string("say \"hi\""));
    EXPECT_EQ(std::string{"\"c:\\\\tmp\""}, quote_string("c:\\tmp"));
}

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

TEST_F(SelectorCommandOptionTest, commandOptionsKeepOnlyFirstTwoCharacters)
{
    set_option(OI_UNIV_SEL_CMDS, "abcd");
    set_option(OI_NEWSRC_SEL_CMDS, "abcd");
    set_option(OI_ADD_SEL_CMDS, "abcd");
    set_option(OI_NEWSGROUP_SEL_CMDS, "abcd");
    set_option(OI_NEWS_SEL_CMDS, "abcd");
    set_option(OI_OPTION_SEL_CMDS, "abcd");

    EXPECT_EQ("ab", option_value(OI_UNIV_SEL_CMDS));
    EXPECT_EQ("ab", option_value(OI_NEWSRC_SEL_CMDS));
    EXPECT_EQ("ab", option_value(OI_ADD_SEL_CMDS));
    EXPECT_EQ("ab", option_value(OI_NEWSGROUP_SEL_CMDS));
    EXPECT_EQ("ab", option_value(OI_NEWS_SEL_CMDS));
    EXPECT_EQ("ab", option_value(OI_OPTION_SEL_CMDS));
}

TEST_F(SelectorCommandOptionTest, oneCharacterCommandOptionsPreservePageCommand)
{
    set_option(OI_UNIV_SEL_CMDS, "ab");
    set_option(OI_NEWSRC_SEL_CMDS, "ab");
    set_option(OI_ADD_SEL_CMDS, "ab");
    set_option(OI_NEWSGROUP_SEL_CMDS, "ab");
    set_option(OI_NEWS_SEL_CMDS, "ab");
    set_option(OI_OPTION_SEL_CMDS, "ab");

    set_option(OI_UNIV_SEL_CMDS, "x");
    set_option(OI_NEWSRC_SEL_CMDS, "x");
    set_option(OI_ADD_SEL_CMDS, "x");
    set_option(OI_NEWSGROUP_SEL_CMDS, "x");
    set_option(OI_NEWS_SEL_CMDS, "x");
    set_option(OI_OPTION_SEL_CMDS, "x");

    EXPECT_EQ("xb", option_value(OI_UNIV_SEL_CMDS));
    EXPECT_EQ("xb", option_value(OI_NEWSRC_SEL_CMDS));
    EXPECT_EQ("xb", option_value(OI_ADD_SEL_CMDS));
    EXPECT_EQ("xb", option_value(OI_NEWSGROUP_SEL_CMDS));
    EXPECT_EQ("xb", option_value(OI_NEWS_SEL_CMDS));
    EXPECT_EQ("xb", option_value(OI_OPTION_SEL_CMDS));
}

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
