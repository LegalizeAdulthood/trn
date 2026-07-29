/* ngstuff.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngstuff-internal.h>

#include <config/common.h>
#include <trn/addng.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/change_dir.h>
#include <trn/final.h>
#include <trn/IniDocument.h>
#include <trn/IniSectionValues.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/opt.h>
#include <trn/OptionApplier.h>
#include <trn/OptionCatalog.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/respond.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rt-wumpus.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/sw.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

bool        g_one_command{}; // no ':' processing in perform()
std::string g_save_dir;      // -d

// given the new and complex universal/help possibilities,
// the following interlock variable may save some trouble.
// (if true, we are currently processing options)
//
static bool s_option_sel_lock{};

static std::string_view trim_command_spaces(std::string_view text)
{
    const std::string_view::const_iterator first = std::find_if_not(
        text.begin(), text.end(), [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
    text.remove_prefix(static_cast<std::size_t>(first - text.begin()));
    return text;
}

void newsgroup_stuff_init()
{
    s_option_sel_lock = false;
}

// do a shell escape

bool escapade_with_shell_runner(const NgstuffShellRunner &shell_runner)
{
    return escapade_with_shell_runner(shell_runner, std::string_view{g_buf});
}

bool escapade_with_shell_runner(const NgstuffShellRunner &shell_runner, std::string_view command)
{
    bool interactive = command.size() > 1 && command[1] == FINISH_CMD;
    fs::path where_i_am;
    std::string completed_command;

    if (interactive)
    {
        completed_command = finish_command(command.substr(0, 1), true);
        if (completed_command.empty())
        {
            return true;
        }
        command = completed_command;
    }
    std::string_view command_text = command.size() > 1 ? command.substr(1) : std::string_view{};
    bool             do_cd = command_text.empty() || command_text.front() != '!';
    if (!do_cd)
    {
        command_text.remove_prefix(1);
    }
    else
    {
        std::error_code error;
        where_i_am = fs::current_path(error);
        if (error)
        {
            fmt::print("Cannot determine current working directory!\n");
            finalize(1);
        }
        if (change_dir(g_priv_dir))
        {
            fmt::print("Can't chdir to directory {}\n", g_priv_dir);
            sig_catcher(0);
        }
    }
    command_text = trim_command_spaces(command_text);
    const std::string shell_command = do_interp(command_text);
    reset_tty();                          // make sure tty is friendly
    shell_runner({}, shell_command);      // invoke the shell
    no_echo();                           // and make terminal
    cr_mode();                           // unfriendly again
    if (do_cd)
    {
        if (change_dir(where_i_am))
        {
            fmt::print("Can't chdir to directory {}\n", where_i_am.generic_string());
            sig_catcher(0);
        }
    }
#ifdef MAIL_CALL
    g_mail_count = 0;                    // force recheck
#endif
    return false;
}

bool escapade()
{
    return escapade(std::string_view{g_buf});
}

bool escapade(std::string_view command)
{
    return escapade_with_shell_runner(do_shell, command);
}

// process & command

bool switcheroo()
{
    return switcheroo(std::string_view{g_buf});
}

bool switcheroo(std::string_view command)
{
    std::string completed_command;
    if (command.size() > 1 && command[1] == FINISH_CMD)
    {
        completed_command = finish_command(command.substr(0, 1), true);
        if (completed_command.empty())
        {
            return true;      // if rubbed out, try something else
        }
        command = completed_command;
    }
    std::string_view command_text = command.size() > 1 ? command.substr(1) : std::string_view{};
    if (command_text.empty())
    {
        const std::string prior_save_dir = g_save_dir;
        if (s_option_sel_lock)
        {
            return false;
        }
        s_option_sel_lock = true;
        if (g_general_mode != GM_SELECTOR || g_sel_mode != SM_OPTIONS)
        {
            option_selector();
        }
        s_option_sel_lock = false;
        if (g_save_dir != prior_save_dir)
        {
            cwd_check();
        }
    }
    else if (command_text.front() == '&')
    {
        command_text.remove_prefix(1);
        if (command_text.empty())
        {
            page_start();
            show_macros();
        }
        else
        {
            mac_line(trim_command_spaces(command_text));
        }
    }
    else
    {
        bool     do_cd = in_string(command, "-d", true);
        fs::path where_am_i;

        if (do_cd)
        {
            std::error_code error;
            where_am_i = fs::current_path(error);
            if (error)
            {
                fmt::print("Cannot determine current working directory!\n");
                finalize(1);
            }
        }
        if (command_text.front() == '-' || command_text.front() == '+')
        {
            sw_list(command_text);
        }
        else
        {
            IniDocument document{fmt::format("[options]\n{}\n", command_text), "'&' input"};
            for (const IniSection section : document)
            {
                IniSectionValues values;
                parse_ini_section(section, OptionCatalog().schema(), values);
                OptionApplier{}.apply(values);
                break;
            }
        }
        if (do_cd)
        {
            cwd_check();
            if (change_dir(where_am_i))                // -d does chdirs
            {
                fmt::print("Can't chdir to directory {}\n", where_am_i.generic_string());
                sig_catcher(0);
            }
        }
    }
    return false;
}

// process range commands

NumNumResult num_num(std::string_view command)
{
    ArticleNum       min;
    ArticleNum       max;
    std::string      cmdlst;
    std::string_view ranges;
    ArticleNum       oldart = g_art;
    bool             output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    bool             justone = true; // assume only one article
    const auto       parse_article_num = [](std::string_view text)
    {
        std::size_t index{};
        while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])))
        {
            ++index;
        }
        const bool negative = index < text.size() && text[index] == '-';
        if (index < text.size() && (text[index] == '-' || text[index] == '+'))
        {
            ++index;
        }
        long value{};
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])))
        {
            value = value * 10 + text[index] - '0';
            ++index;
        }
        return ArticleNum{negative ? -value : value};
    };

    if (g_last_art < 1)
    {
        error_msg("No articles");
        return NN_ASK;
    }
    if (g_search_ahead)
    {
        g_search_ahead = ArticleNum{-1};
    }

    perform_status_init(g_newsgroup_ptr->m_to_read);

    std::size_t range_end{};
    for (; range_end < command.size() && (std::isdigit(static_cast<unsigned char>(command[range_end])) ||
                                          std::string_view{" ,-.$"}.find(command[range_end]) != std::string_view::npos);
         ++range_end)
    {
        if (!std::isdigit(static_cast<unsigned char>(command[range_end])))
        {
            justone = false;
        }
    }
    if (range_end < command.size())
    {
        cmdlst = command.substr(range_end);
        justone = false;
    }
    else if (!justone)
    {
        cmdlst = "m";
    }
    ranges = command.substr(0, range_end);
    if (!output_level && !justone)
    {
        fmt::print("Processing...");
        std::fflush(stdout);
    }
    for (bool have_range = true; have_range;)
    {
        const std::size_t      comma = ranges.find(',');
        const std::string_view range = comma == std::string_view::npos ? ranges : ranges.substr(0, comma);
        have_range = comma != std::string_view::npos;

        const std::size_t dash = range.find('-');
        const std::string_view min_text = dash == std::string_view::npos ? range : range.substr(0, dash);
        if (!min_text.empty() && min_text.front() == '.')
        {
            min = oldart;
        }
        else
        {
            min = parse_article_num(min_text);
        }
        if (min < g_abs_first)
        {
            min = g_abs_first;
            g_msg = fmt::format("(First article is {})", g_abs_first.value_of());
            warn_msg(g_msg);
        }
        if (dash != std::string_view::npos)
        {
            const std::string_view max_text = range.substr(dash + 1);
            if (!max_text.empty() && max_text.front() == '$')
            {
                max = g_last_art;
            }
            else if (!max_text.empty() && max_text.front() == '.')
            {
                max = oldart;
            }
            else
            {
                max = parse_article_num(max_text);
            }
        }
        else
        {
            max = min;
        }
        if (max > g_last_art)
        {
            max = g_last_art;
            min = std::min(min, max);
            g_msg = fmt::format("(Last article is {})", g_last_art.value_of());
            warn_msg(g_msg);
        }
        if (max < min)
        {
            error_msg("Bad range");
            return NN_ASK;
        }
        if (justone)
        {
            g_art = min;
            return NN_REREAD;
        }
        for (g_art = article_first(min); g_art <= max; g_art = article_next(g_art))
        {
            g_artp = article_ptr(g_art);
            if (perform(cmdlst, output_level && g_page_line == 1) < 0)
            {
                if (g_verbose)
                {
                    g_msg = fmt::format("(Interrupted at article {})", g_art.value_of());
                }
                else
                {
                    g_msg = fmt::format("(Intr at {})", g_art.value_of());
                }
                error_msg(g_msg);
                return NN_ASK;
            }
            if (!output_level)
            {
                perform_status(g_newsgroup_ptr->m_to_read, 50);
            }
        }
        if (have_range)
        {
            ranges.remove_prefix(comma + 1);
        }
    }
    g_art = oldart;
    return NN_NORM;
}

int thread_perform(std::string_view command)
{
    Subject*sp;
    Article*ap;
    int     bits;
    bool    output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    bool    one_thread = false;

    if (command.size() <= 1)
    {
        return -1;
    }
    int len = 1;
    if (command[1] == ':')
    {
        bits = 0;
        len++;
    }
    else
    {
        bits = SF_VISIT;
    }
    if (len < static_cast<int>(command.size()) && command[static_cast<std::size_t>(len)] == '.')
    {
        if (!g_artp)
        {
            return -1;
        }
        one_thread = true;
        len++;
    }
    std::string cmdstr{command.substr(static_cast<std::size_t>(len))};
    bool        want_unread = !g_sel_rereading && (cmdstr.empty() || cmdstr[0] != 'm');

    perform_status_init(g_newsgroup_ptr->m_to_read);
    len = static_cast<int>(cmdstr.size());

    if (!output_level && !one_thread)
    {
        fmt::print("Processing...");
        std::fflush(stdout);
    }
    // A few commands can just loop through the subjects.
    if ((len == 1 && (cmdstr[0] == 't' || cmdstr[0] == 'J'))                       //
        || (len == 2                                                               //
            && (((cmdstr[0] == '+' || cmdstr[0] == '-') && cmdstr[0] == cmdstr[1]) //
                || cmdstr[0] == 'T' || cmdstr[0] == 'A')))
    {
        g_performed_article_loop = false;
        if (one_thread)
        {
            sp = (g_sel_mode == SM_THREAD ? g_artp->m_subj->m_thread->m_subj : g_artp->m_subj);
        }
        else
        {
            sp = next_subject((Subject *) nullptr, bits);
        }
        for (; sp; sp = next_subject(sp, bits))
        {
            if ((!(sp->m_flags & g_sel_mask) ^ !bits) || !sp->m_misc)
            {
                continue;
            }
            g_artp = sp->first_art();
            if (g_artp)
            {
                g_art = g_artp->article_num();
                if (perform(cmdstr, 0) < 0)
                {
                    error_msg("Interrupted");
                    goto break_out;
                }
            }
            if (one_thread)
            {
                break;
            }
        }
    }
    else if (!cmdstr.empty() && cmdstr[0] == 'p')
    {
        ArticleNum oldart = g_art;
        g_art = article_after(g_last_art);
        followup("f");
        g_force_grow = true;
        g_art = oldart;
        g_page_line++;
    }
    else
    {
        // The rest loop through the articles.
        // Use the explicit article-order if it exists
        if (!g_art_ptr_list.empty())
        {
            Article **limit = article_ptr_list_end();
            sp = (g_sel_mode==SM_THREAD? g_artp->m_subj->m_thread->m_subj : g_artp->m_subj);
            for (Article **app = article_ptr_list_begin(); app < limit; app++)
            {
                ap = *app;
                if (one_thread && ap->m_subj->m_thread != sp->m_thread)
                {
                    continue;
                }
                if ((!(ap->m_flags & AF_UNREAD) ^ want_unread) //
                    && !(ap->m_flags & g_sel_mask) ^ !!bits)
                {
                    g_art = ap->article_num();
                    g_artp = ap;
                    if (perform(cmdstr, output_level && g_page_line == 1) < 0)
                    {
                        error_msg("Interrupted");
                        goto break_out;
                    }
                }
                if (!output_level)
                {
                    perform_status(g_newsgroup_ptr->m_to_read, 50);
                }
            }
        }
        else
        {
            if (one_thread)
            {
                sp = (g_sel_mode == SM_THREAD ? g_artp->m_subj->m_thread->m_subj : g_artp->m_subj);
            }
            else
            {
                sp = next_subject((Subject *) nullptr, bits);
            }
            for (; sp; sp = next_subject(sp, bits))
            {
                for (ap = sp->first_art(); ap; ap = ap->next_article())
                {
                    if ((!(ap->m_flags & AF_UNREAD) ^ want_unread)
                        && !(ap->m_flags & g_sel_mask) ^ !!bits)
                    {
                        g_art = ap->article_num();
                        g_artp = ap;
                        if (perform(cmdstr, output_level && g_page_line == 1) < 0)
                        {
                            error_msg("Interrupted");
                            goto break_out;
                        }
                    }
                }
                if (one_thread)
                {
                    break;
                }
                if (!output_level)
                {
                    perform_status(g_newsgroup_ptr->m_to_read, 50);
                }
            }
        }
    }
break_out:
    return 1;
}

int perform(std::string_view command_list, int output_level)
{
    int savemode = 0;

    if (output_level == 1)
    {
        fmt::print("{:<6} ", g_art.value_of());
        std::fflush(stdout);
    }

    g_perform_count++;
    for (std::size_t command_pos{}; command_pos < command_list.size(); ++command_pos)
    {
        const char ch = command_list[command_pos];
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == ':')
        {
            continue;
        }
        if (ch == 'j')
        {
            if (savemode)
            {
                g_artp->mark_as_read();
                g_artp->change_auto_flags(AUTO_KILL_1);
            }
            else if (!was_read(g_art))
            {
                g_artp->mark_as_read();
                if (output_level && g_verbose)
                {
                    fmt::print("\tJunked");
                }
            }
            if (g_sel_rereading)
            {
                g_artp->deselect_article(output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == '+')
        {
            const bool doubled_plus =
                command_pos + 1 < command_list.size() && command_list[command_pos + 1] == '+';
            if (savemode || doubled_plus)
            {
                if (g_sel_mode == SM_THREAD)
                {
                    g_artp->select_articles_thread(savemode ? AUTO_SEL_THD : AUTO_KILL_NONE);
                }
                else
                {
                    g_artp->select_articles_subject(savemode ? AUTO_SEL_SBJ : AUTO_KILL_NONE);
                }
                if (doubled_plus)
                {
                    ++command_pos;
                }
            }
            else
            {
                g_artp->select_article(output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == 'S')
        {
            g_artp->select_articles_subject(AUTO_SEL_SBJ);
        }
        else if (ch == '.')
        {
            select_sub_thread(g_artp, savemode? AUTO_SEL_FOL : AUTO_KILL_NONE);
        }
        else if (ch == '-')
        {
            const bool doubled_minus =
                command_pos + 1 < command_list.size() && command_list[command_pos + 1] == '-';
            if (doubled_minus)
            {
                if (g_sel_mode == SM_THREAD)
                {
                    g_artp->deselect_articles_thread();
                }
                else
                {
                    g_artp->deselect_articles_subject();
                }
                ++command_pos;
            }
            else
            {
                g_artp->deselect_article(output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == ',')
        {
            kill_sub_thread(g_artp, AFFECT_ALL | (savemode? AUTO_KILL_FOL : AUTO_KILL_NONE));
        }
        else if (ch == 'J')
        {
            if (g_sel_mode == SM_THREAD)
            {
                g_artp->kill_articles_thread(AFFECT_ALL | (savemode ? AUTO_KILL_THD : AUTO_KILL_NONE));
            }
            else
            {
                g_artp->kill_articles_subject(AFFECT_ALL | (savemode ? AUTO_KILL_SBJ : AUTO_KILL_NONE));
            }
        }
        else if (ch == 'K' || ch == 'k')
        {
            g_artp->kill_articles_subject(AFFECT_ALL | (savemode ? AUTO_KILL_SBJ : AUTO_KILL_NONE));
        }
        else if (ch == 'x')
        {
            if (!was_read(g_art))
            {
                g_artp->one_less();
                if (output_level && g_verbose)
                {
                    fmt::print("\tKilled");
                }
            }
            if (g_sel_rereading)
            {
                g_artp->deselect_article(AUTO_KILL_NONE);
            }
        }
        else if (ch == 't')
        {
            entire_tree(g_artp);
        }
        else if (ch == 'T')
        {
            savemode = 1;
        }
        else if (ch == 'A')
        {
            savemode = 2;
        }
        else if (ch == 'm')
        {
            if (savemode)
            {
                g_artp->change_auto_flags(AUTO_SEL_1);
            }
            else if ((g_artp->m_flags & (AF_UNREAD | AF_EXISTS)) == AF_EXISTS)
            {
                g_artp->unmark_as_read();
                if (output_level && g_verbose)
                {
                    fmt::print("\tMarked unread");
                }
            }
        }
        else if (ch == 'M')
        {
            g_artp->delay_unmark();
            g_artp->one_less();
            if (output_level && g_verbose)
            {
                fmt::print("\tWill return");
            }
        }
        else if (ch == '=')
        {
            carriage_return();
            output_subject((char*)g_artp,0);
            output_level = 0;
        }
        else if (ch == 'C')
        {
            int ret = cancel_article();
            if (output_level && g_verbose)
            {
                fmt::print("\t{}anceled", ret ? "Not c" : "C");
            }
        }
        else if (ch == '%')
        {
            std::string expanded_command;

            if (g_one_command)
            {
                expanded_command = do_interp(command_list.substr(command_pos));
            }
            else
            {
                const std::string_view command_text = command_list.substr(command_pos);
                const std::size_t      command_size = skip_interp(command_text, ":");
                expanded_command = do_interp(command_text.substr(0, command_size));
                command_pos += command_size - 1;
            }
            g_perform_count--;
            if (perform(expanded_command, output_level ? 2 : 0) < 0)
            {
                return -1;
            }
        }
        else if (std::string_view{"!&sSwWae|"}.find(ch) != std::string_view::npos)
        {
            std::string_view command_text = command_list.substr(command_pos);
            std::string      command_storage;
            if (!g_one_command)
            {
                command_storage.reserve(command_text.size());
                std::size_t command_size{};
                while (command_size < command_text.size())
                {
                    if (command_text[command_size] == '\\' && command_size + 1 < command_text.size() &&
                        command_text[command_size + 1] == ':')
                    {
                        ++command_size;
                    }
                    else if (command_text[command_size] == ':')
                    {
                        break;
                    }
                    command_storage += command_text[command_size];
                    ++command_size;
                }
                command_text = command_storage;
                if (command_size != 0)
                {
                    command_pos += command_size - 1;
                }
            }
            if (ch == '!')
            {
                escapade(command_text);
                if (output_level && g_verbose)
                {
                    fmt::print("\tShell escaped");
                }
            }
            else if (ch == '&')
            {
                switcheroo(command_text);
                if (output_level && g_verbose)
                {
                    if (command_text.size() > 1 && command_text[1] != '&')
                    {
                        fmt::print("\tSwitched");
                    }
                }
            }
            else
            {
                if (output_level != 1)
                {
                    erase_line(false);
                    fmt::print("{:<6} ", g_art.value_of());
                }
                if (ch == 'a')
                {
                    view_article();
                }
                else
                {
                    save_article(command_text);
                }
                newline();
                output_level = 0;
            }
        }
        else
        {
            g_msg = fmt::format("Unknown command: {}", command_list.substr(command_pos));
            error_msg(g_msg);
            return -1;
        }
        if (output_level && g_verbose)
        {
            std::fflush(stdout);
        }
        if (g_one_command)
        {
            break;
        }
    }
    if (output_level && g_verbose)
    {
        newline();
    }
    if (g_int_count)
    {
        g_int_count = 0;
        return -1;
    }
    return 1;
}

int newsgroup_sel_perform(std::string_view command)
{
    NewsgroupFlags bits;
    bool one_group = false;

    if (command.size() <= 1)
    {
        return -1;
    }
    int len = 1;
    if (command[1] == ':')
    {
        bits = NF_NONE;
        len++;
    }
    else
    {
        bits = NF_INCLUDED;
    }
    if (len < static_cast<int>(command.size()) && command[static_cast<std::size_t>(len)] == '.')
    {
        if (!g_newsgroup_ptr)
        {
            return -1;
        }
        one_group = true;
        len++;
    }
    std::string cmdstr{command.substr(static_cast<std::size_t>(len))};

    perform_status_init(g_newsgroup_to_read.value_of());

    if (one_group)
    {
        newsgroup_perform(cmdstr, 0);
        goto break_out;
    }

    for (g_newsgroup_ptr = newsgroup_first(); g_newsgroup_ptr; g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr))
    {
        if (g_sel_rereading? g_newsgroup_ptr->m_to_read != TR_NONE
                         : g_newsgroup_ptr->m_to_read < g_newsgroup_min_to_read)
        {
            continue;
        }
        set_newsgroup(g_newsgroup_ptr);
        if ((g_newsgroup_ptr->m_flags & bits) == bits //
            && (!(g_newsgroup_ptr->m_flags & static_cast<NewsgroupFlags>(g_sel_mask)) ^ !!bits))
        {
            if (newsgroup_perform(cmdstr, 0) < 0)
            {
                break;
            }
        }
        perform_status(g_newsgroup_to_read.value_of(), 50);
    }

break_out:
    return 1;
}

int newsgroup_perform(std::string_view cmdlst, int output_level)
{
    if (output_level == 1)
    {
        fmt::print("{} ", g_newsgroup_name);
        std::fflush(stdout);
    }

    g_perform_count++;
    for (; !cmdlst.empty(); cmdlst.remove_prefix(1))
    {
        const int ch = cmdlst.front();
        if (std::isspace(ch) || ch == ':')
        {
            continue;
        }
        switch (ch)
        {
        case '+':
            if (!(g_newsgroup_ptr->m_flags & static_cast<NewsgroupFlags>(g_sel_mask)))
            {
                g_newsgroup_ptr->m_flags = ((g_newsgroup_ptr->m_flags | static_cast<NewsgroupFlags>(g_sel_mask)) & ~NF_DEL);
                g_selected_count++;
            }
            break;

        case 'c':
            g_newsgroup_ptr->catch_up(0, 0);
            // FALL THROUGH

        case '-':
deselect:
            if (g_newsgroup_ptr->m_flags & static_cast<NewsgroupFlags>(g_sel_mask))
            {
                g_newsgroup_ptr->m_flags &= ~static_cast<NewsgroupFlags>(g_sel_mask);
                if (g_sel_rereading)
                {
                    g_newsgroup_ptr->m_flags |= NF_DEL;
                }
                g_selected_count--;
            }
            break;

        case 'u':
            if (output_level && g_verbose)
            {
                fmt::print("Unsubscribed to newsgroup {}\n", g_newsgroup_ptr->rc_name());
                term_down(1);
            }
            g_newsgroup_ptr->m_subscribe_char = UNSUBSCRIBED_CHAR;
            g_newsgroup_ptr->m_to_read = TR_UNSUB;
            g_newsgroup_ptr->m_rc->flags |= RF_RC_CHANGED;
            g_newsgroup_ptr->m_flags &= ~static_cast<NewsgroupFlags>(g_sel_mask);
            --g_newsgroup_to_read;
            goto deselect;

        default:
            g_msg = fmt::format("Unknown command: {}", cmdlst);
            error_msg(g_msg);
            return -1;
        }
        if (output_level && g_verbose)
        {
            std::fflush(stdout);
        }
        if (g_one_command)
        {
            break;
        }
    }
    if (output_level && g_verbose)
    {
        newline();
    }
    if (g_int_count)
    {
        g_int_count = 0;
        return -1;
    }
    return 1;
}

int add_group_sel_perform(std::string_view command)
{
    int bits;
    bool one_group = false;

    if (command.size() <= 1)
    {
        return -1;
    }
    int len = 1;
    if (command[1] == ':')
    {
        bits = 0;
        len++;
    }
    else
    {
        bits = g_sel_mask;
    }
    if (len < static_cast<int>(command.size()) && command[static_cast<std::size_t>(len)] == '.')
    {
        if (g_first_add_group)
        {
            return -1;
        }
        one_group = true;
        len++;
    }
    std::string cmdstr{command.substr(static_cast<std::size_t>(len))};

    perform_status_init(g_newsgroup_to_read.value_of());

    if (one_group)
    {
        goto break_out;
    }

    for (AddGroup *gp = g_first_add_group; gp; gp = gp->m_next)
    {
        if (!(gp->m_flags & g_sel_mask) ^ !!bits)
        {
            if (gp->add_group_perform(cmdstr, 0) < 0)
            {
                break;
            }
        }
        perform_status(g_newsgroup_to_read.value_of(), 50);
    }

break_out:
    return 1;
}

int AddGroup::add_group_perform(std::string_view cmdlst, int output_level)
{
    if (output_level == 1)
    {
        fmt::print("{} ", m_name);
        std::fflush(stdout);
    }

    g_perform_count++;
    for (; !cmdlst.empty(); cmdlst.remove_prefix(1))
    {
        const int ch = cmdlst.front();
        if (std::isspace(ch) || ch == ':')
        {
            continue;
        }
        if (ch == '+')
        {
            m_flags |= AGF_SEL;
            g_selected_count++;
        }
        else if (ch == '-')
        {
            m_flags &= ~AGF_SEL;
            g_selected_count--;
        }
        else
        {
            g_msg = fmt::format("Unknown command: {}", cmdlst);
            error_msg(g_msg);
            return -1;
        }
        if (output_level && g_verbose)
        {
            std::fflush(stdout);
        }
        if (g_one_command)
        {
            break;
        }
    }
    if (output_level && g_verbose)
    {
        newline();
    }
    if (g_int_count)
    {
        g_int_count = 0;
        return -1;
    }
    return 1;
}
