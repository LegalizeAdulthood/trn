/* scmd.cpp
 *
 * Scan command loop.
 * Does some simple commands, and passes the rest to context-specific routines.
 */
// This file is Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/scmd-internal.h>

#include <config/common.h>
#include <trn/final.h>
#include <trn/help.h>
#include <trn/ng.h>
#include <trn/ngstuff.h>
#include <trn/sacmd.h> // sa_docmd
#include <trn/samain.h>
#include <trn/scan.h>
#include <trn/sdisp.h>
#include <trn/smisc.h>
#include <trn/sorder.h>
#include <trn/spage.h>
#include <trn/terminal.h>
#include <trn/univ.h>

#include <fmt/format.h>

#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

static void             s_look_ahead();
static int              s_do_cmd(std::string_view command);
static char             s_command_char(std::string_view command);
static std::string_view s_command_argument(std::string_view command);
static void             s_set_search_text(std::string_view search_text);
static bool             s_match_description(long ent);
static long             s_forward_search(long ent);
static long             s_backward_search(long ent);
static void             s_search(std::string_view command);
static void             s_jump_num(char_int firstchar);

void s_go_bot()
{
    g_s_ref_bot = true;                 // help uses whole screen
    s_goxy(0,g_tc_LINES-g_s_bot_lines); // go to bottom bar
    erase_eol();                        // erase to end of line
    s_goxy(0,g_tc_LINES-g_s_bot_lines); // go (back?) to bottom bar
}

// finishes a command on the bottom line...
// returns true if command entered, false if wiped out...
std::string s_finish_cmd(std::string_view prompt, std::string_view command)
{
    s_go_bot();
    if (!prompt.empty())
    {
        fmt::print("{}", prompt);
        std::fflush(stdout);
    }
    if (command.empty())
    {
        return {};
    }
    return finish_command(command.substr(0, 1), false); // do not echo newline
}

// returns an entry # selected, S_QUIT, or S_ERR
int s_cmd_loop()
{
    int i;

    // initialization stuff for entry into s_cmdloop
    g_s_ref_all = true;
    eat_typeahead();    // no typeahead before entry
    while (true)
    {
        s_refresh();
        s_place_ptr();          // place article pointer
        g_bos_on_stop = true;
        s_look_ahead();          // do something useful while waiting
        std::string command = get_cmd();
        g_bos_on_stop = false;
        eat_typeahead();        // stay in control.
        // check for window resizing and refresh
        // if window is resized, refill and redraw
        if (g_s_resized)
        {
            i = s_fill_page();
            if (i == -1 || i == 0)      // can't fillpage
            {
                return S_QUIT;
            }
            const char refresh_command = Ctl('l');
            (void)s_do_cmd(std::string_view{&refresh_command, 1});
            g_s_resized = false;                // dealt with
        }
        i = s_do_cmd(command);
        if (i == S_NOT_FOUND)    // command not in common set
        {
            switch (g_s_cur_type)
            {
            case S_ART:
                i = sa_do_cmd(command);
                break;

            default:
                i = 0;  // just keep looping
                break;
            }
        }
        if (i != 0)     // either an entry # or a return code
        {
            return i;
        }
        if (g_s_refill)
        {
            i = s_fill_page();
            if (i == -1 || i == 0)      // can't fillpage
            {
                return S_QUIT;
            }
        }
        // otherwise just keep on looping...
    }
}

static void s_look_ahead()
{
    switch (g_s_cur_type)
    {
    case S_ART:
        sa_lookahead();
        break;

    default:
        break;
    }
}

// Do some simple, common Scan commands for any mode
// Interprets command, returning 0 to continue looping or
// a condition code (negative #s).  Responsible for setting refresh flags
// if necessary.
//
static int s_do_cmd(std::string_view command)
{
    bool flag; // misc

    long a = g_page_ents[g_s_ptr_page_line].ent_num;
    char command_ch = s_command_char(command);
    if (command_ch == '\f') // map form feed to ^l
    {
        command_ch = Ctl('l');
    }
    switch (command_ch)
    {
    case 'j':         // vi mode
        if (!g_s_mode_vi)
        {
            return S_NOT_FOUND;
        }
        // FALL THROUGH

    case 'n':         // next art
    case ']':
        s_rub_ptr();
        if (g_s_ptr_page_line < g_s_bot_ent)    // more on page...
        {
            g_s_ptr_page_line +=1;
        }
        else
        {
            if (!s_next_elig(g_page_ents[g_s_bot_ent].ent_num))
            {
                s_beep();
                g_s_refill = true;
                break;
            }
            s_go_next_page();   // will jump to top too...
        }
        break;

    case 'k': // vi mode
        if (!g_s_mode_vi)
        {
            return S_NOT_FOUND;
        }
        // FALL THROUGH

    case 'p': // previous art
    case '[':
        s_rub_ptr();
        if (g_s_ptr_page_line > 0)      // more on page...
        {
            g_s_ptr_page_line = g_s_ptr_page_line - 1;
        }
        else
        {
            if (s_prev_elig(g_page_ents[0].ent_num))
            {
                s_go_prev_page();
                g_s_ptr_page_line = g_s_bot_ent; // go to page bot.
            }
            else
            {
                g_s_refill = true;
                s_beep();
            }
        }
        break;

    case 't': // top of page
        s_rub_ptr();
        s_go_top_page();
        break;

    case 'b': // bottom of page
        s_rub_ptr();
        s_go_bot_page();
        break;

    case '>': // next page
        s_rub_ptr();
        a = s_next_elig(g_page_ents[g_s_bot_ent].ent_num);
        if (!a)                 // at end of articles
        {
            s_beep();
            break;
        }
        s_go_next_page();               // will beep if no next page
        break;

    case '<': // previous page
        s_rub_ptr();
        if (!s_prev_elig(g_page_ents[0].ent_num))
        {
            s_beep();
            break;
        }
        s_go_prev_page();               // will beep if no prior page
        break;

    case 'T':         // top of ents
    case '^':
        s_rub_ptr();
        flag = s_go_top_ents();
        if (!flag)              // failure
        {
            return S_QUIT;
        }
        break;

    case 'B': // bottom of ents
    case '$':
        s_rub_ptr();
        flag = s_go_bot_ents();
        if (!flag)
        {
            return S_QUIT;
        }
        break;

    case Ctl('r'):    // refresh screen
    case Ctl('l'):
          g_s_ref_all = true;
        break;

    case Ctl('f'):    // refresh (mail) display
#ifdef MAIL_CALL
        set_mail(true);
#endif
        g_s_ref_bot = true;
        break;

    case 'h': // universal help
        s_go_bot();
        g_s_ref_all = true;
        univ_help(UHELP_SCANART);
        eat_typeahead();
        break;

    case 'H': // help
        s_go_bot();
        g_s_ref_all = true;
        // any commands typed during help are unused. (might change)
        switch (g_s_cur_type)
        {
        case S_ART:
            (void)help_scan_article();
            break;

        default:
            std::printf("No help available for this mode (yet).\n");
            std::printf("Press any key to continue.\n");
            break;
        }
        (void)get_anything();
        eat_typeahead();
        break;

    case '!': // shell command
        s_go_bot();
        g_s_ref_all = true;                     // will need refresh
        if (!escapade())
        {
            (void)get_anything();
        }
        eat_typeahead();
        break;

    case '/':
    case '?':
    case 'g':         // goto (search for) group
        s_search(command);
        break;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        s_jump_num(command_ch);
        break;

    case '#':         // Toggle item numbers
        if (g_s_item_num)
        {
            // turn off item numbers
            g_s_desc_cols += g_s_item_num_cols;
            g_s_item_num_cols = 0;
            g_s_item_num = 0;
        }
        else
        {
            // turn on item numbers
            g_s_item_num_cols = 3;
            g_s_desc_cols -= g_s_item_num_cols;
            g_s_item_num = 1;
        }
        g_s_ref_all = true;
        break;

    default:
        return S_NOT_FOUND;              // not one of the simple commands
    } // switch
    return 0;           // keep on looping!
}

static std::string s_search_text;
static bool        s_search_init{};

static char s_command_char(std::string_view command)
{
    return command.empty() ? '\0' : command.front();
}

static std::string_view s_command_argument(std::string_view command)
{
    if (command.size() < 2)
    {
        return {};
    }
    return command.substr(1);
}

bool scmd_match_description_for_test(long ent, std::string_view search_text)
{
    s_set_search_text(search_text);
    return s_match_description(ent);
}

void scmd_jump_num_for_test(char_int firstchar)
{
    s_jump_num(firstchar);
}

static void s_set_search_text(std::string_view search_text)
{
    s_search_text = search_text;
    for (char &ch : s_search_text)
    {
        const unsigned char text_ch = static_cast<unsigned char>(ch);
        if (std::isupper(text_ch))
        {
            ch = static_cast<char>(std::tolower(text_ch)); // convert to lower case
        }
    }
}

static bool s_match_description(long ent)
{
    int lines = s_ent_lines(ent);
    for (int i = 1; i <= lines; i++)
    {
        std::string description = s_get_desc(ent, i, false);
        for (char &ch : description)
        {
            if (std::isupper(ch))
            {
                ch = static_cast<char>(std::tolower(ch)); // convert to lower case
            }
        }
        if (description.find(s_search_text) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

static long s_forward_search(long ent)
{
    if (ent)
    {
        ent = s_next_elig(ent);
    }
    else
    {
        ent = s_first();
    }
    for ( ; ent; ent = s_next_elig(ent))
    {
        if (s_match_description(ent))
        {
            break;
        }
    }
    return ent;
}

static long s_backward_search(long ent)
{
    if (ent)
    {
        ent = s_prev_elig(ent);
    }
    else
    {
        ent = s_last();
    }
    for ( ; ent; ent = s_prev_elig(ent))
    {
        if (s_match_description(ent))
        {
            break;
        }
    }
    return ent;
}

// perhaps later have a wraparound search?
static void s_search(std::string_view command)
{
    int         fill_type; // 0: forward, 1: backward
    const char *error_msg;
    const char  command_ch = s_command_char(command);

    if (!s_search_init)
    {
        s_search_init = true;
        s_search_text.clear();
    }
    s_rub_ptr();
    const std::string full_command = s_finish_cmd("", std::string_view{&command_ch, 1});
    if (full_command.empty())
    {
        return;
    }
    const std::string_view argument = s_command_argument(full_command);
    if (!argument.empty()) // new text
    {
        // make leading space skip an option later?
        // (it isn't too important because substring matching is used)
        std::string_view search_text = argument;
        while (!search_text.empty() && search_text.front() == ' ')
        {
            search_text.remove_prefix(1);
        }
        s_set_search_text(search_text);
    }
    if (s_search_text.empty())
    {
        s_beep();
        std::printf("\nNo previous search string.\n");
        (void)get_anything();
        g_s_ref_all = true;
        return;
    }
    s_go_bot();
    std::printf("Searching for %s",s_search_text.c_str());
    std::fflush(stdout);
    long ent = g_page_ents[g_s_ptr_page_line].ent_num;
    switch (command_ch)
    {
    case '/':
        error_msg = "No matches forward from current point.";
        ent = s_forward_search(ent);
        fill_type = 0;          // forwards fill
        break;

    case '?':
        error_msg = "No matches backward from current point.";
        ent = s_backward_search(ent);
        fill_type = 1;          // backwards fill
        break;

    case 'g':
        ent = s_forward_search(ent);
        if (!ent)
        {
            ent = s_forward_search(0);  // from top
            // did we just loop around?
            if (ent == g_page_ents[g_s_ptr_page_line].ent_num)
            {
                ent = 0;
                error_msg = "No other entry matches.";
            }
            else
            {
                error_msg = "No matches.";
            }
        }
        fill_type = 0;          // forwards fill
        break;

    default:
        fill_type = 0;
        error_msg = "Internal error in s_search()";
        break;
    }
    if (!ent)
    {
        s_beep();
        std::printf("\n%s\n", error_msg);
        (void)get_anything();
        g_s_ref_all = true;
        return;
    }
    for (int i = 0; i <= g_s_bot_ent; i++)
    {
        if (g_page_ents[i].ent_num == ent)               // entry is on same page
        {
            g_s_ptr_page_line = i;
            return;
        }
    }
    // entry is not on page...
    if (fill_type == 1)
    {
        (void)s_fill_page_backward(ent);
        s_go_bot_page();
        g_s_refill = true;
        g_s_ref_all = true;
    }
    else
    {
        (void)s_fill_page_forward(ent);
        s_go_top_page();
        g_s_ref_all = true;
    }
}

static void s_jump_num(char_int firstchar)
{
    bool jump_verbose = true;
    int  value = firstchar - '0';

    s_rub_ptr();
    if (jump_verbose)
    {
        s_go_bot();
        g_s_ref_bot = true;
        fmt::print("Jump to item: {}", static_cast<char>(firstchar));
        std::fflush(stdout);
    }
    const std::string command = get_cmd();
    const char        command_char = command.empty() ? '\0' : command.front();
    if (command_char == g_erase_char)
    {
        return;
    }
    switch (command_char)
    {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        if (jump_verbose)
        {
            fmt::print("{}", command_char);
            std::fflush(stdout);
        }
        value = value*10 + (command_char - '0');
        break;

    default:
        push_char(command_char);
        break;
    }
    if (value == 0 || value > g_s_bot_ent+1)
    {
        s_beep();
        return;
    }
    g_s_ptr_page_line = value-1;
}
