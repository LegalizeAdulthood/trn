// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/terminal.h>

#include <config/env.h>
#include <trn/artsrch.h>
#include <trn/final.h>
#include <trn/ng.h>
#include <trn/opt.h>
#include <trn/smisc.h>
#include <trn/univ.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <string>
#include <string_view>

namespace
{

void drain_macro_buffer()
{
    while (macro_pending())
    {
        char discarded{};
        read_tty(&discarded, 1);
    }
}

class TerminalTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_errno = errno;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_int_count = g_int_count;
        m_old_tc_cr = g_tc_CR;
        m_old_tc_ce = g_tc_CE;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_s_default_cmd = g_s_default_cmd;
        m_old_univ_default_cmd = g_univ_default_cmd;
        errno = 0;
        g_int_count = 0;
        g_tc_CR = m_carriage_return;
        g_tc_CE = m_erase_line;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
        drain_macro_buffer();
    }

    void TearDown() override
    {
        drain_macro_buffer();
        errno = m_old_errno;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_int_count = m_old_int_count;
        g_tc_CR = m_old_tc_cr;
        g_tc_CE = m_old_tc_ce;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_s_default_cmd = m_old_s_default_cmd;
        g_univ_default_cmd = m_old_univ_default_cmd;
    }

    char m_carriage_return[1]{};
    char m_erase_line[1]{};
    char m_standout_start[1]{};
    char m_standout_end[1]{};

    int         m_old_errno{};
    GeneralMode m_old_general_mode{};
    MinorMode   m_old_mode{};
    int         m_old_term_line{};
    int         m_old_term_col{};
    char        m_old_int_count{};
    const char *m_old_tc_cr{};
    const char *m_old_tc_ce{};
    const char *m_old_tc_so{};
    const char *m_old_tc_se{};
    bool        m_old_s_default_cmd{};
    bool        m_old_univ_default_cmd{};
};

class MacroDisplayTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_int_count = g_int_count;
        m_old_erase_screen = g_erase_screen;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_am = g_tc_AM;
        m_old_fire_is_out = g_fire_is_out;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_page_line = g_page_line;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;

        g_int_count = 0;
        g_erase_screen = false;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
        g_tc_AM = false;
        g_tc_LINES = 100;
        g_tc_COLS = 80;
        g_page_line = 1;
        g_term_line = 0;
        g_term_col = 0;
    }

    void TearDown() override
    {
        g_int_count = m_old_int_count;
        g_erase_screen = m_old_erase_screen;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_AM = m_old_tc_am;
        g_fire_is_out = m_old_fire_is_out;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_page_line = m_old_page_line;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
    }

    char m_standout_start[5]{"<so>"};
    char m_standout_end[5]{"<se>"};

    char  m_old_int_count{};
    bool  m_old_erase_screen{};
    const char *m_old_tc_so{};
    const char *m_old_tc_se{};
    bool  m_old_tc_am{};
    int   m_old_fire_is_out{};
    int   m_old_tc_lines{};
    int   m_old_tc_cols{};
    int   m_old_page_line{};
    int   m_old_term_line{};
    int   m_old_term_col{};
};

class ChoiceInputTest : public MacroDisplayTest
{
protected:
    void SetUp() override
    {
        MacroDisplayTest::SetUp();
        drain_macro_buffer();

        m_old_last_pat = g_last_pat;
        m_old_art_do_read = g_art_do_read;
        m_old_art_how_much = g_art_how_much;
        m_old_tc_cr = g_tc_CR;
        m_old_tc_ce = g_tc_CE;
        g_tc_CR = m_carriage_return;
        g_tc_CE = m_erase_line;
    }

    void TearDown() override
    {
        drain_macro_buffer();
        g_last_pat = m_old_last_pat;
        g_art_do_read = m_old_art_do_read;
        g_art_how_much = m_old_art_how_much;
        g_tc_CR = m_old_tc_cr;
        g_tc_CE = m_old_tc_ce;
        MacroDisplayTest::TearDown();
    }

    char m_carriage_return[5]{"<cr>"};
    char m_erase_line[5]{"<ce>"};

    std::string m_old_last_pat;
    bool        m_old_art_do_read{};
    ArtScope    m_old_art_how_much{};
    const char *m_old_tc_cr{};
    const char *m_old_tc_ce{};
};

class MouseBarTest : public testing::Test
{
protected:
    void SetUp() override
    {
        drain_macro_buffer();

        m_old_univ_sel_btns = g_univ_sel_btns;
        m_old_mouse_bar_cnt = g_mouse_bar_cnt;
        m_old_mouse_bar_width = g_mouse_bar_width;
        m_old_use_mouse = g_use_mouse;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_tc_cm = g_tc_CM;
        m_old_tc_bc = g_tc_BC;
        m_old_tc_up = g_tc_UP;
        m_old_tc_cr = g_tc_CR;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;

        g_univ_sel_btns.clear();
        g_mouse_bar_cnt = 0;
        g_mouse_bar_width = 0;
        g_use_mouse = true;
        g_general_mode = GM_SELECTOR;
        g_mode = MM_UNIVERSAL;
        g_tc_LINES = 24;
        g_tc_COLS = 80;
        g_term_line = 23;
        g_term_col = 67;
        g_tc_CM = m_cursor_motion;
        g_tc_BC = m_backspace;
        g_tc_UP = m_cursor_up;
        g_tc_CR = m_carriage_return;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
    }

    void TearDown() override
    {
        drain_macro_buffer();
        xmouse_off();
        g_univ_sel_btns = m_old_univ_sel_btns;
        g_mouse_bar_cnt = m_old_mouse_bar_cnt;
        g_mouse_bar_width = m_old_mouse_bar_width;
        g_use_mouse = m_old_use_mouse;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_tc_CM = m_old_tc_cm;
        g_tc_BC = m_old_tc_bc;
        g_tc_UP = m_old_tc_up;
        g_tc_CR = m_old_tc_cr;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
    }

    MouseButtonList m_old_univ_sel_btns;
    int             m_old_mouse_bar_cnt{};
    int             m_old_mouse_bar_width{};
    bool            m_old_use_mouse{};
    GeneralMode     m_old_general_mode{};
    MinorMode       m_old_mode{};
    int             m_old_tc_lines{};
    int             m_old_tc_cols{};
    int             m_old_term_line{};
    int             m_old_term_col{};
    const char     *m_old_tc_cm{};
    const char     *m_old_tc_bc{};
    const char     *m_old_tc_up{};
    const char     *m_old_tc_cr{};
    const char     *m_old_tc_so{};
    const char     *m_old_tc_se{};
    char            m_cursor_motion[1]{};
    char            m_backspace[1]{};
    char            m_cursor_up[1]{};
    char            m_carriage_return[1]{};
    char            m_standout_start[5]{"<so>"};
    char            m_standout_end[5]{"<se>"};
};

class XMouseInitTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_can_home = g_can_home;
        m_old_use_threads = g_use_threads;
        m_old_use_mouse = g_use_mouse;
        m_old_xterm_mouse = get_env_var(XTERM_MOUSE_ENV);

        g_can_home = true;
        g_use_threads = true;
        g_use_mouse = false;
        unset_env_var(XTERM_MOUSE_ENV);
    }

    void TearDown() override
    {
        g_can_home = m_old_can_home;
        g_use_threads = m_old_use_threads;
        g_use_mouse = m_old_use_mouse;
        restore_env(XTERM_MOUSE_ENV, m_old_xterm_mouse);
    }

private:
    static constexpr std::string_view XTERM_MOUSE_ENV{"XTERMMOUSE"};

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

    bool        m_old_can_home{};
    bool        m_old_use_threads{};
    bool        m_old_use_mouse{};
    std::string m_old_xterm_mouse;
};

} // namespace

TEST_F(TerminalTest, getCommandExpandsMacroString)
{
    set_macro("~", "z");

    push_char('~');
    const std::string command = get_cmd();

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('z', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
}

#ifdef MSDOS
TEST_F(TerminalTest, tgotoStringFormatsDosCursorMotion)
{
    EXPECT_EQ("\033[8;4H", tgoto_string("\033[%d;%dH", 3, 7));
}
#endif

TEST_F(TerminalTest, macLineParsesExpandedKey)
{
    const std::string_view line{"^B q\n"};

    mac_line(line);

    push_char('\002');
    get_cmd(g_buf);

    EXPECT_EQ('q', g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
}

TEST_F(TerminalTest, pushStringExpandsInInputOrderWithMacroBits)
{
    push_string("a^B", 0200);

    char command{};
    read_tty(&command, 1);
    EXPECT_EQ(static_cast<unsigned char>('a' ^ 0200), static_cast<unsigned char>(command));

    read_tty(&command, 1);
    EXPECT_EQ(static_cast<unsigned char>('\002' ^ 0200), static_cast<unsigned char>(command));
}

TEST_F(TerminalTest, pauseGetCommandReturnsPushedCommand)
{
    push_char('x');

    testing::internal::CaptureStdout();
    const int command = pause_get_cmd();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ('x', command);
}

TEST_F(TerminalTest, setDefUsesDefaultCommandForBlankInput)
{
    g_buf[0] = ' ';
    g_buf[1] = '\0';

    set_def(g_buf, "n");

    EXPECT_EQ('n', g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(TerminalTest, setDefExpandsControlDefaultCommand)
{
    g_buf[0] = ' ';
    g_buf[1] = '\0';

    set_def(g_buf, "^N");

    EXPECT_EQ(Ctl('N'), g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
}

TEST_F(TerminalTest, inCharPrintsPromptDefaultAndStoresInput)
{
    const std::string prompt{"Continue?"};
    const std::string dflt{"yn"};
    push_char('y');

    testing::internal::CaptureStdout();
    in_char(prompt, MM_ADD_NEWSGROUP_PROMPT, dflt);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Continue? [yn] ", output);
    EXPECT_EQ('y', g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
}

TEST_F(TerminalTest, inCharCountsPromptNewlinesAndAppliesDefault)
{
    g_term_line = 4;
    g_term_col = 12;
    push_char(' ');

    testing::internal::CaptureStdout();
    in_char("First\nSecond", MM_ADD_NEWSGROUP_PROMPT, "^N");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("First\nSecond [^N] ", output);
    EXPECT_EQ(Ctl('N'), g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
    EXPECT_EQ(5, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_F(TerminalTest, inAnswerPrintsPromptAndStoresCommand)
{
    g_term_line = 4;
    g_term_col = 12;
    const std::string prompt{"Really? "};
    push_char('\n');
    push_char('y');

    testing::internal::CaptureStdout();
    in_answer(prompt, MM_FOLLOWUP_NEW_TOPIC_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Really? y\n", output);
    EXPECT_EQ('y', g_buf[0]);
    EXPECT_EQ('\0', g_buf[1]);
    EXPECT_EQ(5, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_F(MacroDisplayTest, showMacrosFormatsNestedControlKey)
{
    set_macro("\001A", "result");

    testing::internal::CaptureStdout();
    show_macros();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("<so>Macros:<se>\n^AA   result\n", output);
}

TEST_F(MacroDisplayTest, printLinesUsesStringViewExtent)
{
    const std::string text{"alpha\nbeta\nunused"};

    testing::internal::CaptureStdout();
    const int         cmd = print_lines(std::string_view{text.data(), 11}, NO_MARKING);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(0, cmd);
    EXPECT_EQ("alpha\nbeta\n", output);
}

TEST_F(ChoiceInputTest, inChoiceCyclesToNextValue)
{
    push_char('\n');
    push_char(' ');

    testing::internal::CaptureStdout();
    const bool        clean_screen = in_choice("> ", "yes", "yes/no", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("no", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> yes<cr><cr><ce><cr>> no", output);
}

TEST_F(ChoiceInputTest, inChoiceCyclesValueWithinPrefix)
{
    push_char('\n');
    push_char(' ');

    testing::internal::CaptureStdout();
    const bool clean_screen =
        in_choice("> ", "reverse date", "[reverse] date/subject/author/groups/cnt/points", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("reverse subject", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> reverse date<cr><cr><ce><cr>> reverse subject", output);
}

TEST_F(ChoiceInputTest, inChoicePreservesNumericValue)
{
    push_char('\n');

    testing::internal::CaptureStdout();
    const bool        clean_screen = in_choice("> ", "12", "no/<# lines>", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("12", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> 12", output);
}

TEST_F(ChoiceInputTest, inChoiceDoesNotSplitSlashInsideFreeFormValue)
{
    push_char('\n');

    testing::internal::CaptureStdout();
    const bool        clean_screen = in_choice("> ", "a/b", "<e.g. a/b>", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("a/b", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> a/b", output);
}

TEST_F(ChoiceInputTest, inChoiceEscapeEscapeInterpolatesWholeBuffer)
{
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;
    push_char('\n');
    push_char('\033');
    push_char('\033');

    testing::internal::CaptureStdout();
    in_choice("> ", "%/", "<search>", MM_OPTION_EDIT_PROMPT);
    testing::internal::GetCapturedStdout();

    EXPECT_STREQ("/needle/", g_buf);
}

TEST_F(ChoiceInputTest, inChoiceEscapeSlashInsertsSearchPattern)
{
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;
    push_char('\n');
    push_char('/');
    push_char('\033');

    testing::internal::CaptureStdout();
    const bool clean_screen = in_choice("> ", "prefix", "<search>", MM_OPTION_EDIT_PROMPT);
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("prefix/needle/", g_buf);
}

TEST_F(MouseBarTest, checkMouseBarPushesClickedButtonCommand)
{
    set_option(OI_UNIV_SEL_BTNS, "[Quit]q");

    testing::internal::CaptureStdout();
    xmouse_check();
    EXPECT_TRUE(check_mouse_bar(3, 75, 23, 0, 75, 23));
    testing::internal::GetCapturedStdout();

    ASSERT_TRUE(macro_pending());
    char command{};
    read_tty(&command, 1);

    EXPECT_EQ('q', command);
}

TEST_F(XMouseInitTest, environmentValueEnablesMouse)
{
    set_env_var("XTERMMOUSE", "y");

    xmouse_init("trn");

    EXPECT_TRUE(g_use_mouse);
}

TEST_F(XMouseInitTest, environmentValueWinsOverProgramNameSuffix)
{
    set_env_var("XTERMMOUSE", "n");

    xmouse_init("trnx");

    EXPECT_FALSE(g_use_mouse);
}

TEST_F(XMouseInitTest, programNameSuffixEnablesMouseWhenEnvironmentMissing)
{
    xmouse_init("trnx");

    EXPECT_TRUE(g_use_mouse);
}

TEST_F(XMouseInitTest, disabledTerminalStateSkipsMouseSetup)
{
    set_env_var("XTERMMOUSE", "y");
    g_can_home = false;

    xmouse_init("trnx");

    EXPECT_FALSE(g_use_mouse);
}
