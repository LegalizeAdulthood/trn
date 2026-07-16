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
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::string storage;
    storage.reserve(switches.size() + 2);
    storage.assign(switches);
    storage.resize(storage.size() + 2);
    char *swlist = storage.data();
    char* p;
    char inquote = 0;

    char *s = swlist;
    p = swlist;
    while (*s)                          // "String, or nothing"
    {
        if (!inquote && std::isspace(*s))    // word delimiter?
        {
            while (true)
            {
                s = skip_space(s);
                if (*s != '#')
                {
                    break;
                }
                s = skip_ne(s, '\n');
            }
            if (p != swlist)
            {
                *p++ = '\0';            // chop here
            }
        }
        else if (inquote == *s)
        {
            s++;                        // delete trailing quote
            inquote = 0;                // no longer quoting
        }
        else if (!inquote && (*s == '"' || *s == '\''))
        {
                                        // OK, I know when I am not wanted
            inquote = *s++;             // remember & del single or double
        }
        else if (*s == '\\')            // quoted something?
        {
            if (*++s != '\n')           // newline?
            {
                s = interp_backslash(p, s);
                p++;
            }
            s++;
        }
        else
        {
            *p++ = *s++;                // normal char
        }
    }
    *p++ = '\0';
    *p = '\0';                          // put an extra null on the end
    if (inquote)
    {
        std::printf("Unmatched %c in switch\n",inquote);
        term_down(1);
    }
    for (char *c = swlist; *c; /* p += strlen(p)+1 */)
    {
        decode_switch(c);
        while (*c++)
        {
                                        // point at null + 1
        }
    }
}

// decode a single switch

void decode_switch(const char *s)
{
    s = skip_space(s);          // ignore leading spaces
#ifdef DEBUG
    if (g_debug)
    {
        std::printf("Switch: %s\n",s);
        term_down(1);
    }
#endif
    if (*s != '-' && *s != '+')         // newsgroup pattern
    {
        set_newsgroup_to_do(s);
        if (g_mode == MM_INITIALIZING)
        {
            g_newsgroup_min_to_read = 0;
        }
    }
    else                                // normal switch
    {
        bool upordown = *s == '-';
        char tmpbuf[LINE_BUF_LEN];

        switch (*++s)
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
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_CHECKPOINT_NEWSRC_FREQUENCY, s);
            break;

        case 'd':
        {
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_SAVE_DIR, s);
            break;
        }

        case 'D':
#ifdef DEBUG
            if (*++s == '=')
            {
                s++;
            }
            if (*s)
            {
                if (upordown)
                {
                    g_debug |= std::atoi(s);
                }
                else
                {
                    g_debug &= ~std::atoi(s);
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
            std::printf("Trn was not compiled with -DDEBUG.\n");
            term_down(1);
#endif
            break;

        case 'e':
            set_option(OI_ERASE_SCREEN, yes_or_no(upordown));
            break;

        case 'E':
        {
            if (*++s == '=')
            {
                s++;
            }
            std::strcpy(tmpbuf,s);
            char *value = std::strchr(tmpbuf, '=');
            if (value)
            {
                *value++ = '\0';
                set_env_var(tmpbuf, value);
                if (g_mode == MM_INITIALIZING)
                {
                    save_init_environment(tmpbuf, value);
                }
            }
            else
            {
                set_env_var(tmpbuf, "");
                if (g_mode == MM_INITIALIZING)
                {
                    save_init_environment(tmpbuf, "");
                }
            }
            break;
        }

        case 'f':
            set_option(OI_NOVICE_DELAYS, yes_or_no(!upordown));
            break;

        case 'F':
            set_option(OI_CITED_TEXT_STRING, s+1);
            break;

        case 'g':
            set_option(OI_GOTO_LINE_NUM, s+1);
            break;

        case 'G':
            set_option(OI_FUZZY_NEWSGROUP_NAMES, yes_or_no(upordown));
            break;

        case 'h':
            if (!s[1])
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
            set_header(s+1,*s == 'h'? HT_HIDE : HT_MAGIC, upordown);
            break;

        case 'i':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_INITIAL_ARTICLE_LINES, s);
            break;

        case 'I':
            set_option(OI_APPEND_UNSUBSCRIBED_GROUPS, yes_or_no(upordown));
            break;

        case 'j':
            set_option(OI_FILTER_CONTROL_CHARACTERS, yes_or_no(!upordown));
            break;

        case 'J':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_JOIN_SUBJECT_LINES,
                       upordown && *s? s : yes_or_no(upordown));
            break;

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
            set_option(OI_PAGER_LINE_MARKING, s+1);
            break;

        case 'N':
            if (upordown)
            {
                set_option(OI_SAVE_FILE_TYPE, "norm");
            }
            break;

        case 'o':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_OLD_MTHREADS_DATABASE, s);
            break;

        case 'O':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_NEWS_SEL_MODE, s);
            if (*++s)
            {
                const std::string order = fmt::format("{}{}", std::isupper(*s) ? "r " : "", *s);
                set_option(OI_NEWS_SEL_ORDER, order.c_str());
            }
            break;

        case 'p':
            if (*++s == '=')
            {
                s++;
            }
            if (!upordown)
            {
                s = yes_or_no(false);
            }
            else
            {
                switch (*s)
                {
                case '+':
                    s = "thread";
                    break;

                case 'p':
                    s = "parent";
                    break;

                default:
                    s = "subthread";
                    break;
                }
            }
            set_option(OI_SELECT_MY_POSTS, s);
            break;

        case 'q':
            set_option(OI_NEW_GROUP_CHECK, yes_or_no(!upordown));
            break;

        case 'Q':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_CHARSET, s);
            break;

        case 'r':
            set_option(OI_RESTART_AT_LAST_GROUP, yes_or_no(upordown));
            break;

        case 's':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_INITIAL_GROUP_LIST, std::isdigit(*s)? s : yes_or_no(false));
            break;

        case 'S':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_SCAN_MODE_COUNT, s);
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
            if (*++s == '=')
            {
                s++;
            }
            if (std::isdigit(*s))
            {
                set_option(OI_ARTICLE_TREE_LINES, s);
                s = skip_digits(s);
            }
            if (*s)
            {
                set_option(OI_NEWS_SEL_STYLES, s);
            }
            set_option(OI_USE_THREADS, yes_or_no(upordown));
            break;

        case 'X':
            if (*++s == '=')
            {
                s++;
            }
            if (std::isdigit(*s))
            {
                set_option(OI_USE_NEWS_SEL, s);
                s = skip_digits(s);
            }
            else
            {
                set_option(OI_USE_NEWS_SEL, yes_or_no(upordown));
            }
            if (*s)
            {
                set_option(OI_NEWS_SEL_CMDS, s);
            }
            break;

        case 'z':
            if (*++s == '=')
            {
                s++;
            }
            set_option(OI_DEFAULT_REFETCH_TIME,
                       upordown && *s? s : yes_or_no(upordown));
            break;

        default:
            if (g_verbose)
            {
                std::printf("\nIgnoring unrecognized switch: -%c\n", *s);
            }
            else
            {
                std::printf("\nIgnoring -%c\n", *s);
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
        std::fprintf(fp, "%s=%s\n", entry.name.c_str(), quote_string(entry.value.c_str()));
    }
    s_init_environment_strings.clear();
}
