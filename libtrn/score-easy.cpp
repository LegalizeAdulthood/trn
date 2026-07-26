/* score-easy.cpp
 *
 * Simple interactive menus for scorefile tasks.
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/score-easy.h>

#include <config/common.h>
#include <trn/terminal.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <string>
#include <string_view>
#include <system_error>

// returns new string or empty string to abort.
std::string sc_easy_append()
{
    char ch;

    char  filechar = '\0'; // GCC warning avoidance
    std::string line;
    fmt::print("\nScorefile easy append mode.\n");
    bool q_done = false;
    while (!q_done)
    {
        fmt::print("0) Exit.\n");
        fmt::print("1) List the current scorefile abbreviations.\n");
        fmt::print("2) Add an entry to the global scorefile.\n");
        fmt::print("3) Add an entry to this newsgroup's scorefile.\n");
        fmt::print("4) Add an entry to another scorefile.\n");
        fmt::print("5) Use a temporary scoring rule.\n");
        ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return {};

        case '1':
            return "?";

        case '2':
            filechar = '*';
            break;

        case '3':
            filechar = '"';
            break;

        case '4':
            filechar = '\0';
            break;

        case '5':
            filechar = '!';
            break;

        case 'h':
            fmt::print("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    while (filechar == '\0')    // choose one
    {
        fmt::print("Type the (single character) abbreviation of the scorefile:");
        std::fflush(stdout);
        eat_typeahead();
        const std::string command = get_cmd();
        filechar = command.empty() ? '\0' : command.front();
        fmt::print("{}\n", filechar);
        // If error checking is done later, then an error should set
        // filechar to '\0' and continue the while loop.
    }
    line += filechar;
    line += ' ';
    q_done = false;
    while (!q_done)
    {
        fmt::print("What type of line do you want to add?\n");
        fmt::print("0) Exit.\n");
        fmt::print("1) A scoring rule line.\n");
        fmt::print("   (for the current article's author/subject)\n");
        fmt::print("2) A command, comment, or other kind of line.\n");
        fmt::print("   (use this for any other kind of line)\n");
        fmt::print("\n[Other line formats will be supported later.]\n");
        ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return {};

        case '1':
            break;

        case '2':
        {
            fmt::print("Enter the line below:\n");
            std::fflush(stdout);
            const std::string command = finish_command(">", true);
            if (!command.empty())
            {
                std::string_view command_text{command};
                command_text.remove_prefix(1);
                line += command_text;
                return line;
            }
            fmt::print("\n");
            q_done = false;
            break;
        }

        case 'h':
            fmt::print("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    q_done = false;
    while (!q_done)
    {
        fmt::print("Enter a score amount (like 10 or -6):");
        std::fflush(stdout);
        const std::string command = finish_command(" ", true);
        if (!command.empty())
        {
            std::string_view score_text{command};
            score_text.remove_prefix(1);
            score_text.remove_prefix(std::min(score_text.find_first_not_of(" \t"), score_text.size()));
            if (score_text.empty())
            {
                continue; // the while loop
            }
            std::string_view parse_text{score_text};
            if (parse_text.front() == '+')
            {
                parse_text.remove_prefix(1);
                if (parse_text.empty())
                {
                    continue; // the while loop
                }
            }
            long                         score{};
            const std::from_chars_result result =
                std::from_chars(parse_text.data(), parse_text.data() + parse_text.size(), score);
            if (result.ec != std::errc{} || (score == 0 && score_text.front() != '0'))
            {
                continue; // the while loop
            }
            line += std::to_string(score);
            line += ' ';
            q_done = true;
        }
        else
        {
            fmt::print("\n");
        }
    }
    q_done = false;
    while (!q_done)
    {
        fmt::print("Do you want to:\n");
        fmt::print("0) Exit.\n");
        fmt::print("1) Give the score to the current subject.\n");
        fmt::print("2) Give the score to the current author.\n");
// add some more options here later
// perhaps fold regular-expression question here?
        ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return {};

        case '1':
            line += 'S';
            return line;

        case '2':
            line += 'F';
            return line;

        case 'h':
            fmt::print("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    // later ask for headers, pattern-matching, etc...
    return {};
}

// returns new string or empty string to abort.
std::string sc_easy_command()
{
    fmt::print("\nScoring easy command mode.\n");
    bool q_done = false;
    while (!q_done)
    {
        fmt::print("0) Exit.\n");
        fmt::print("1) Add something to a scorefile.\n");
        fmt::print("2) Rescore the articles in the current newsgroup.\n");
        fmt::print("3) Explain the current article's score.\n");
        fmt::print("   (show the rules that matched this article)\n");
        fmt::print("4) Edit this newsgroup's scoring rule file.\n");
        // later add an option to edit an arbitrary file
        fmt::print("5) Continue scoring unscored articles.\n");
        char ch = menu_get_char();
        q_done = true;
        switch (ch)
        {
        case '0':
            return {};

        case '1':
            return "\"";        // do an append command

        case '2':
            return "r";

        case '3':
            return "s";

        case '4':
            // add more later
            return "e";

        case '5':
            return "f";

        case 'h':
            fmt::print("No help available (yet).\n");
            q_done = false;
            break;

        default:
            q_done = false;
            break;
        }
    }
    return {};
}
