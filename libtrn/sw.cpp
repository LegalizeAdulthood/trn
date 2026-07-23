/* sw.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/sw.h>

#include <file_contents.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/head.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/only.h>
#include <trn/opt.h>
#include <trn/rcstuff.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

struct InitEnvironmentString
{
    std::string name;
    std::string value;
};

static std::vector<InitEnvironmentString> s_init_environment_strings;

static void save_init_environment(std::string_view name, std::string_view value);

void sw_file(std::string_view filename)
{
    const std::string switches = file_contents(filename);
    if (!switches.empty())
    {
        sw_list(switches);
    }
}

// decode a list of space separated switches

void sw_list(std::string_view switches)
{
    std::vector<std::string> tokens;
    std::string              token;
    token.reserve(switches.size());
    char        inquote = 0;
    std::size_t index = 0;
    while (index < switches.size() && switches[index] != '\0') // "String, or nothing"
    {
        const char ch = switches[index];
        if (!inquote && std::isspace(static_cast<unsigned char>(ch))) // word delimiter?
        {
            while (index < switches.size())
            {
                while (index < switches.size() && std::isspace(static_cast<unsigned char>(switches[index])))
                {
                    ++index;
                }
                if (index == switches.size() || switches[index] != '#')
                {
                    break;
                }
                while (index < switches.size() && switches[index] != '\n' && switches[index] != '\0')
                {
                    ++index;
                }
            }
            if (!token.empty())
            {
                tokens.push_back(token);
                token.clear();
            }
        }
        else if (inquote == ch)
        {
            ++index;     // delete trailing quote
            inquote = 0; // no longer quoting
        }
        else if (!inquote && (ch == '"' || ch == '\''))
        {
            // OK, I know when I am not wanted
            inquote = ch; // remember & del single or double
            ++index;
        }
        else if (ch == '\\') // quoted something?
        {
            ++index;
            if (index < switches.size() && switches[index] != '\n' && switches[index] != '\0')
            {
                std::string_view escape = switches.substr(index);
                if (const std::size_t end = escape.find('\0'); end != std::string_view::npos)
                {
                    escape = escape.substr(0, end);
                }
                const std::size_t original_size = escape.size();
                token.push_back(interp_backslash(escape));
                index += original_size - escape.size();
            }
            else if (index < switches.size() && switches[index] == '\n')
            {
                ++index;
            }
            else
            {
                token.push_back('\\');
            }
        }
        else
        {
            token.push_back(ch); // normal char
            ++index;
        }
    }
    if (!token.empty())
    {
        tokens.push_back(token);
    }
    if (inquote)
    {
        fmt::print("Unmatched {} in switch\n", inquote);
        term_down(1);
    }
    for (const std::string &switch_token : tokens)
    {
        decode_switch(switch_token);
    }
}

// decode a single switch

void decode_switch(std::string_view s)
{
    const auto is_space = [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) != 0; };
    const auto is_digit = [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; };
    const std::string_view::const_iterator first = std::find_if_not(s.begin(), s.end(), is_space);
    s.remove_prefix(static_cast<std::size_t>(first - s.begin()));
    const auto switch_suffix = [](std::string_view text)
    {
        if (text.size() < 2)
        {
            return std::string_view{};
        }
        text.remove_prefix(2);
        return text;
    };
    const auto switch_argument = [switch_suffix](std::string_view text)
    {
        text = switch_suffix(text);
        if (!text.empty() && text.front() == '=')
        {
            text.remove_prefix(1);
        }
        return text;
    };
    const auto skip_digits_view = [is_digit](std::string_view text)
    {
        const std::string_view::const_iterator first_non_digit = std::find_if_not(text.begin(), text.end(), is_digit);
        text.remove_prefix(static_cast<std::size_t>(first_non_digit - text.begin()));
        return text;
    };

#ifdef DEBUG
    if (g_debug)
    {
        fmt::print("Switch: {}\n", s);
        term_down(1);
    }
#endif
    if (s.empty() || (s.front() != '-' && s.front() != '+')) // newsgroup pattern
    {
        set_newsgroup_to_do(s);
        if (g_mode == MM_INITIALIZING)
        {
            g_newsgroup_min_to_read = 0;
        }
    }
    else                                // normal switch
    {
        bool       upordown = s.front() == '-';
        const char option = s.size() > 1 ? s[1] : '\0';

        switch (option)
        {
        case '/':
            set_option(OI_AUTO_SAVE_NAME, yes_or_no(upordown));
            break;

        case '+':
            set_option(OI_USE_ADD_SEL, yes_or_no(upordown));
            set_option(OI_USE_NEWSGROUP_SEL, yes_or_no(upordown));
            if (upordown)
            {
                set_option(OI_INITIAL_GROUP_LIST, yes_or_no(false));
            }
            else
            {
                set_option(OI_USE_NEWSRC_SEL, yes_or_no(false));
            }
            break;

        case 'a':
            set_option(OI_BACKGROUND_THREADING, yes_or_no(!upordown));
            break;

        case 'A':
            set_option(OI_AUTO_ARROW_MACROS, yes_or_no(upordown));
            break;

        case 'b':
            set_option(OI_READ_BREADTH_FIRST, yes_or_no(upordown));
            break;

        case 'B':
            set_option(OI_BACKGROUND_SPINNER, yes_or_no(upordown));
            break;

        case 'c':
            g_check_flag = upordown;
            break;

        case 'C':
            set_option(OI_CHECKPOINT_NEWSRC_FREQUENCY, switch_argument(s));
            break;

        case 'd':
            set_option(OI_SAVE_DIR, switch_argument(s));
            break;

        case 'D':
#ifdef DEBUG
            if (const std::string_view argument = switch_argument(s); !argument.empty())
            {
                std::string_view                       value = argument;
                const std::string_view::const_iterator first_digit =
                    std::find_if_not(value.begin(), value.end(), is_space);
                value.remove_prefix(static_cast<std::size_t>(first_digit - value.begin()));
                if (!value.empty() && value.front() == '+')
                {
                    value.remove_prefix(1);
                }
                int debug_flags{};
                std::from_chars(value.data(), value.data() + value.size(), debug_flags);
                if (upordown)
                {
                    g_debug |= debug_flags;
                }
                else
                {
                    g_debug &= ~debug_flags;
                }
            }
            else
            {
                if (upordown)
                {
                    g_debug |= 1;
                }
                else
                {
                    g_debug = 0;
                }
            }
#else
            fmt::print("Trn was not compiled with -DDEBUG.\n");
            term_down(1);
#endif
            break;

        case 'e':
            set_option(OI_ERASE_SCREEN, yes_or_no(upordown));
            break;

        case 'E':
        {
            const std::string_view            assignment = switch_argument(s);
            const std::string_view::size_type separator = assignment.find('=');
            if (separator != std::string_view::npos)
            {
                const std::string_view name = assignment.substr(0, separator);
                const std::string_view value = assignment.substr(separator + 1);
                set_env_var(name, value);
                if (g_mode == MM_INITIALIZING)
                {
                    save_init_environment(name, value);
                }
            }
            else
            {
                set_env_var(assignment, "");
                if (g_mode == MM_INITIALIZING)
                {
                    save_init_environment(assignment, "");
                }
            }
            break;
        }

        case 'f':
            set_option(OI_NOVICE_DELAYS, yes_or_no(!upordown));
            break;

        case 'F':
            set_option(OI_CITED_TEXT_STRING, switch_suffix(s));
            break;

        case 'g':
            set_option(OI_GOTO_LINE_NUM, switch_suffix(s));
            break;

        case 'G':
            set_option(OI_FUZZY_NEWSGROUP_NAMES, yes_or_no(upordown));
            break;

        case 'h':
            if (switch_suffix(s).empty())
            {
                // Free old g_user_htype list
                while (g_user_header_type_count > 1)
                {
                    g_user_header_type[--g_user_header_type_count].name.clear();
                }
                std::memset(g_user_header_type_index,0,26);
            }
            // FALL THROUGH

        case 'H':
            if (g_check_flag)
            {
                break;
            }
            set_header(switch_suffix(s), option == 'h' ? HT_HIDE : HT_MAGIC, upordown);
            break;

        case 'i':
            set_option(OI_INITIAL_ARTICLE_LINES, switch_argument(s));
            break;

        case 'I':
            set_option(OI_APPEND_UNSUBSCRIBED_GROUPS, yes_or_no(upordown));
            break;

        case 'j':
            set_option(OI_FILTER_CONTROL_CHARACTERS, yes_or_no(!upordown));
            break;

        case 'J':
        {
            const std::string_view argument = switch_argument(s);
            set_option(OI_JOIN_SUBJECT_LINES, upordown && !argument.empty() ? argument : yes_or_no(upordown));
            break;
        }

        case 'k':
            set_option(OI_IGNORE_THRU_ON_SELECT, yes_or_no(upordown));
            break;

        case 'K':
            set_option(OI_AUTO_GROW_GROUPS, yes_or_no(!upordown));
            break;

        case 'l':
            set_option(OI_MUCK_UP_CLEAR, yes_or_no(upordown));
            break;

        case 'L':
            set_option(OI_ERASE_EACH_LINE, yes_or_no(upordown));
            break;

        case 'M':
            if (upordown)
            {
                set_option(OI_SAVE_FILE_TYPE, "mail");
            }
            break;

        case 'm':
            set_option(OI_PAGER_LINE_MARKING, switch_suffix(s));
            break;

        case 'N':
            if (upordown)
            {
                set_option(OI_SAVE_FILE_TYPE, "norm");
            }
            break;

        case 'o':
            set_option(OI_OLD_MTHREADS_DATABASE, switch_argument(s));
            break;

        case 'O':
        {
            const std::string_view argument = switch_argument(s);
            set_option(OI_NEWS_SEL_MODE, argument);
            if (argument.size() > 1)
            {
                const char        order_char = argument[1];
                const std::string order =
                    fmt::format("{}{}", std::isupper(static_cast<unsigned char>(order_char)) ? "r " : "", order_char);
                set_option(OI_NEWS_SEL_ORDER, order);
            }
            break;
        }

        case 'p':
        {
            std::string_view       value;
            const std::string_view argument = switch_argument(s);
            if (!upordown)
            {
                value = yes_or_no(false);
            }
            else
            {
                switch (argument.empty() ? '\0' : argument.front())
                {
                case '+':
                    value = "thread";
                    break;

                case 'p':
                    value = "parent";
                    break;

                default:
                    value = "subthread";
                    break;
                }
            }
            set_option(OI_SELECT_MY_POSTS, value);
            break;
        }

        case 'q':
            set_option(OI_NEW_GROUP_CHECK, yes_or_no(!upordown));
            break;

        case 'Q':
            set_option(OI_CHARSET, switch_argument(s));
            break;

        case 'r':
            set_option(OI_RESTART_AT_LAST_GROUP, yes_or_no(upordown));
            break;

        case 's':
        {
            const std::string_view argument = switch_argument(s);
            set_option(OI_INITIAL_GROUP_LIST,
                       !argument.empty() && is_digit(argument.front()) ? argument : yes_or_no(false));
            break;
        }

        case 'S':
            set_option(OI_SCAN_MODE_COUNT, switch_argument(s));
            break;

        case 't':
            set_option(OI_TERSE_OUTPUT, yes_or_no(upordown));
            break;

        case 'T':
            set_option(OI_EAT_TYPEAHEAD, yes_or_no(!upordown));
            break;

        case 'u':
            set_option(OI_COMPRESS_SUBJECTS, yes_or_no(!upordown));
            break;

        case 'U':
            g_unsafe_rc_saves = upordown;
            break;

        case 'v':
            set_option(OI_VERIFY_INPUT, yes_or_no(upordown));
            break;

        case 'V':
            if (g_mode == MM_INITIALIZING)
            {
                g_tc_LINES = 1000;
                g_tc_COLS = 1000;
                g_erase_screen = false;
            }
            trn_version();
            newline();
            if (g_mode == MM_INITIALIZING)
            {
                std::exit(0);
            }
            break;

        case 'x':
        {
            const std::string_view argument = switch_argument(s);
            std::string_view       rest = argument;
            if (!rest.empty() && is_digit(rest.front()))
            {
                set_option(OI_ARTICLE_TREE_LINES, rest);
                rest = skip_digits_view(rest);
            }
            if (!rest.empty())
            {
                set_option(OI_NEWS_SEL_STYLES, rest);
            }
            set_option(OI_USE_THREADS, yes_or_no(upordown));
            break;
        }

        case 'X':
        {
            const std::string_view argument = switch_argument(s);
            std::string_view       rest = argument;
            if (!rest.empty() && is_digit(rest.front()))
            {
                set_option(OI_USE_NEWS_SEL, rest);
                rest = skip_digits_view(rest);
            }
            else
            {
                set_option(OI_USE_NEWS_SEL, yes_or_no(upordown));
            }
            if (!rest.empty())
            {
                set_option(OI_NEWS_SEL_CMDS, rest);
            }
            break;
        }

        case 'z':
        {
            const std::string_view argument = switch_argument(s);
            set_option(OI_DEFAULT_REFETCH_TIME, upordown && !argument.empty() ? argument : yes_or_no(upordown));
            break;
        }

        default:
            if (g_verbose)
            {
                fmt::print("\nIgnoring unrecognized switch: -{}\n", option);
            }
            else
            {
                fmt::print("\nIgnoring -{}\n", option);
            }
            term_down(2);
            break;
        }
    }
}

static void save_init_environment(std::string_view name, std::string_view value)
{
    s_init_environment_strings.push_back({std::string{name}, std::string{value}});
}

void write_init_environment(std::FILE *fp)
{
    for (const InitEnvironmentString &entry : s_init_environment_strings)
    {
        fmt::print(fp, "{}={}\n", entry.name, quote_string(entry.value));
    }
    s_init_environment_strings.clear();
}
