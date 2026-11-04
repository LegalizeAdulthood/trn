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

static constexpr int s_perform_failed = -1;
static constexpr int s_perform_aborted = -2;

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

bool escapade_with_shell_runner(const NgstuffShellRunner &shell_runner, std::string_view command)
{
    bool        interactive = command.size() > 1 && command[1] == FINISH_CMD;
    fs::path    where_i_am;
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
    g_mail_count = 0; // force recheck
#endif
    return false;
}

bool escapade(std::string_view command)
{
    return escapade_with_shell_runner(do_shell, command);
}

// process & command

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
        if (g_general_mode != GM_SELECTOR || g_sel.mode != SM_OPTIONS)
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
    const ArticleLocation old_location = g_nav.target;
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

    g_status.init(g_newsgroup_ptr->m_to_read);

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
            min = old_location.num;
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
                max = old_location.num;
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
            g_nav.target = old_location;
            return NN_ASK;
        }
        if (justone)
        {
            g_nav.target.num = min;
            return NN_REREAD;
        }
        for (g_nav.target.num = g_cache.first(min); g_nav.target.num <= max;
             g_nav.target.num = g_cache.next(g_nav.target.num))
        {
            g_nav.target.art = g_cache.ptr(g_nav.target.num);
            const int result = perform(cmdlst, output_level && g_page_line == 1);
            if (result < 0)
            {
                if (result == s_perform_aborted && !g_msg.empty())
                {
                    error_msg(g_msg);
                }
                else if (g_verbose)
                {
                    g_msg = fmt::format("(Interrupted at article {})", g_nav.target.num.value_of());
                    error_msg(g_msg);
                }
                else
                {
                    g_msg = fmt::format("(Intr at {})", g_nav.target.num.value_of());
                    error_msg(g_msg);
                }
                g_nav.target = old_location;
                return NN_ASK;
            }
            if (!output_level)
            {
                g_status.status(g_newsgroup_ptr->m_to_read, 50);
            }
        }
        if (have_range)
        {
            ranges.remove_prefix(comma + 1);
        }
    }
    g_nav.target = old_location;
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
        if (!g_nav.target.art)
        {
            return -1;
        }
        one_thread = true;
        len++;
    }
    std::string cmdstr{command.substr(static_cast<std::size_t>(len))};
    if (cmdstr.empty())
    {
        return -1;
    }
    bool want_unread = !g_sel.rereading && cmdstr[0] != 'm';

    g_status.init(g_newsgroup_ptr->m_to_read);
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
        g_status.performed_article_loop = false;
        if (one_thread)
        {
            sp = (g_sel.mode == SM_THREAD ? g_nav.target.art->m_subj->m_thread->m_subj : g_nav.target.art->m_subj);
        }
        else
        {
            sp = next_subject((Subject *) nullptr, bits);
        }
        for (; sp; sp = next_subject(sp, bits))
        {
            if ((!(sp->m_flags & g_sel.mask) ^ !bits) || !sp->m_misc)
            {
                continue;
            }
            g_nav.target.art = g_sel.mode == SM_SUBJECT ? sp->m_articles : sp->first_art();
            if (g_nav.target.art)
            {
                g_nav.target.num = g_nav.target.art->article_num();
                const int result = perform(cmdstr, 0);
                if (result < 0)
                {
                    error_msg(result == s_perform_aborted && !g_msg.empty() ? g_msg : "Interrupted");
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
        ArticleNum oldart = g_nav.target.num;
        g_nav.target.num = article_after(g_last_art);
        followup("f");
        g_force_grow = true;
        g_nav.target.num = oldart;
        g_page_line++;
    }
    else
    {
        // The rest loop through the articles.
        // Use the explicit article-order if it exists
        if (!g_art_order.ptr_list.empty())
        {
            Article **limit = g_art_order.end();
            if (one_thread)
            {
                sp = (g_sel.mode == SM_THREAD ? g_nav.target.art->m_subj->m_thread->m_subj : g_nav.target.art->m_subj);
            }
            for (Article **app = g_art_order.begin(); app < limit; app++)
            {
                ap = *app;
                if (one_thread && ap->m_subj->m_thread != sp->m_thread)
                {
                    continue;
                }
                if ((!(ap->m_flags & AF_UNREAD) ^ want_unread) //
                    && !(ap->m_flags & g_sel.mask) ^ !!bits)
                {
                    g_nav.target = {ap->article_num(), ap};
                    const int result = perform(cmdstr, output_level && g_page_line == 1);
                    if (result < 0)
                    {
                        error_msg(result == s_perform_aborted && !g_msg.empty() ? g_msg : "Interrupted");
                        goto break_out;
                    }
                }
                if (!output_level)
                {
                    g_status.status(g_newsgroup_ptr->m_to_read, 50);
                }
            }
        }
        else
        {
            if (one_thread)
            {
                sp = (g_sel.mode == SM_THREAD ? g_nav.target.art->m_subj->m_thread->m_subj : g_nav.target.art->m_subj);
            }
            else
            {
                sp = next_subject((Subject *) nullptr, bits);
            }
            for (; sp; sp = next_subject(sp, bits))
            {
                for (ap = g_sel.mode == SM_SUBJECT ? sp->m_articles : sp->first_art(); ap;
                     ap = g_sel.mode == SM_SUBJECT ? ap->m_subj_next : ap->next_article())
                {
                    if ((!(ap->m_flags & AF_UNREAD) ^ want_unread) && !(ap->m_flags & g_sel.mask) ^ !!bits)
                    {
                        g_nav.target = {ap->article_num(), ap};
                        const int result = perform(cmdstr, output_level && g_page_line == 1);
                        if (result < 0)
                        {
                            error_msg(result == s_perform_aborted && !g_msg.empty() ? g_msg : "Interrupted");
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
                    g_status.status(g_newsgroup_ptr->m_to_read, 50);
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
        fmt::print("{:<6} ", g_nav.target.num.value_of());
        std::fflush(stdout);
    }

    g_status.perform_count++;
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
                g_nav.target.art->mark_as_read();
                g_nav.target.art->change_auto_flags(AUTO_KILL_1);
            }
            else if (!g_cache.was_read(g_nav.target.num))
            {
                g_nav.target.art->mark_as_read();
                if (output_level && g_verbose)
                {
                    fmt::print("\tJunked");
                }
            }
            if (g_sel.rereading)
            {
                g_nav.target.art->deselect_article(output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == '+')
        {
            const bool doubled_plus =
                command_pos + 1 < command_list.size() && command_list[command_pos + 1] == '+';
            if (savemode || doubled_plus)
            {
                if (g_sel.mode == SM_THREAD)
                {
                    g_nav.target.art->select_articles_thread(savemode ? AUTO_SEL_THD : AUTO_KILL_NONE);
                }
                else
                {
                    g_nav.target.art->select_articles_subject(savemode ? AUTO_SEL_SBJ : AUTO_KILL_NONE);
                }
                if (doubled_plus)
                {
                    ++command_pos;
                }
            }
            else
            {
                g_nav.target.art->select_article(output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == 'S')
        {
            g_nav.target.art->select_articles_subject(AUTO_SEL_SBJ);
        }
        else if (ch == '.')
        {
            select_sub_thread(g_nav.target.art, savemode ? AUTO_SEL_FOL : AUTO_KILL_NONE);
        }
        else if (ch == '-')
        {
            const bool doubled_minus =
                command_pos + 1 < command_list.size() && command_list[command_pos + 1] == '-';
            if (doubled_minus)
            {
                if (g_sel.mode == SM_THREAD)
                {
                    g_nav.target.art->deselect_articles_thread();
                }
                else
                {
                    g_nav.target.art->deselect_articles_subject();
                }
                ++command_pos;
            }
            else
            {
                g_nav.target.art->deselect_article(output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == ',')
        {
            kill_sub_thread(g_nav.target.art, AFFECT_ALL | (savemode ? AUTO_KILL_FOL : AUTO_KILL_NONE));
        }
        else if (ch == 'J')
        {
            if (g_sel.mode == SM_THREAD)
            {
                g_nav.target.art->kill_articles_thread(AFFECT_ALL | (savemode ? AUTO_KILL_THD : AUTO_KILL_NONE));
            }
            else
            {
                g_nav.target.art->kill_articles_subject(AFFECT_ALL | (savemode ? AUTO_KILL_SBJ : AUTO_KILL_NONE));
            }
        }
        else if (ch == 'K' || ch == 'k')
        {
            g_nav.target.art->kill_articles_subject(AFFECT_ALL | (savemode ? AUTO_KILL_SBJ : AUTO_KILL_NONE));
        }
        else if (ch == 'x')
        {
            if (!g_cache.was_read(g_nav.target.num))
            {
                g_nav.target.art->one_less();
                if (output_level && g_verbose)
                {
                    fmt::print("\tKilled");
                }
            }
            if (g_sel.rereading)
            {
                g_nav.target.art->deselect_article(AUTO_KILL_NONE);
            }
        }
        else if (ch == 't')
        {
            entire_tree(g_nav.target.art);
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
                g_nav.target.art->change_auto_flags(AUTO_SEL_1);
            }
            else if ((g_nav.target.art->m_flags & (AF_UNREAD | AF_EXISTS)) == AF_EXISTS)
            {
                g_nav.target.art->unmark_as_read();
                if (output_level && g_verbose)
                {
                    fmt::print("\tMarked unread");
                }
            }
        }
        else if (ch == 'M')
        {
            g_nav.target.art->delay_unmark();
            g_nav.target.art->one_less();
            if (output_level && g_verbose)
            {
                fmt::print("\tWill return");
            }
        }
        else if (ch == '=')
        {
            carriage_return();
            output_subject(*g_nav.target.art, 0);
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
            g_status.perform_count--;
            const int result = perform(expanded_command, output_level ? 2 : 0);
            if (result < 0)
            {
                return result;
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
                    fmt::print("{:<6} ", g_nav.target.num.value_of());
                }
                if (ch == 'a')
                {
                    view_article();
                }
                else
                {
                    const SaveResult save_result = save_article(command_text);
                    if (save_result == SAVE_ABORT)
                    {
                        if (g_msg.empty())
                        {
                            g_msg = fmt::format("Save aborted at article {}", g_nav.target.num.value_of());
                        }
                        return s_perform_aborted;
                    }
                }
                newline();
                output_level = 0;
            }
        }
        else
        {
            g_msg = fmt::format("Unknown command: {}", command_list.substr(command_pos));
            error_msg(g_msg);
            return s_perform_failed;
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
        return s_perform_failed;
    }
    return 1;
}

int newsgroup_sel_perform(std::string_view command)
{
    NewsgroupFlags bits;
    bool                 dot_scope = false;
    NewsgroupData *const current_group = g_newsgroup_ptr;

    g_status.init(g_newsgroup_to_read.value_of());
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
        dot_scope = true;
        len++;
    }
    std::string cmdstr{command.substr(static_cast<std::size_t>(len))};

    if (dot_scope)
    {
        if (!current_group)
        {
            return -1;
        }
        if (bits != NF_NONE)
        {
            newsgroup_perform(cmdstr, 0);
            goto break_out;
        }
    }

    for (g_newsgroup_ptr = g_ng_order.first(); g_newsgroup_ptr; g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr))
    {
        if (dot_scope && g_newsgroup_ptr == current_group)
        {
            continue;
        }
        if (g_sel.rereading ? g_newsgroup_ptr->m_to_read != TR_NONE
                         : g_newsgroup_ptr->m_to_read < g_newsgroup_min_to_read)
        {
            continue;
        }
        set_newsgroup(g_newsgroup_ptr);
        if (dot_scope || ((g_newsgroup_ptr->m_flags & bits) == bits //
                          && (!(g_newsgroup_ptr->m_flags & static_cast<NewsgroupFlags>(g_sel.mask)) ^ !!bits)))
        {
            if (newsgroup_perform(cmdstr, 0) < 0)
            {
                break;
            }
        }
        g_status.status(g_newsgroup_to_read.value_of(), 50);
    }

break_out:
    if (dot_scope)
    {
        set_newsgroup(current_group);
    }
    return 1;
}

int newsgroup_perform(std::string_view cmdlst, int output_level)
{
    if (output_level == 1)
    {
        fmt::print("{} ", g_newsgroup_name);
        std::fflush(stdout);
    }

    g_status.perform_count++;
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
            if (!(g_newsgroup_ptr->m_flags & static_cast<NewsgroupFlags>(g_sel.mask)))
            {
                g_newsgroup_ptr->m_flags =
                    ((g_newsgroup_ptr->m_flags | static_cast<NewsgroupFlags>(g_sel.mask)) & ~NF_DEL);
                g_selected_count++;
            }
            break;

        case 'c':
            g_newsgroup_ptr->catch_up(0, 0);
            // FALL THROUGH

        case '-':
deselect:
            if (g_newsgroup_ptr->m_flags & static_cast<NewsgroupFlags>(g_sel.mask))
            {
                g_newsgroup_ptr->m_flags &= ~static_cast<NewsgroupFlags>(g_sel.mask);
                if (g_sel.rereading && g_newsgroup_ptr->m_to_read != TR_UNSUB)
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
            if (g_newsgroup_ptr->m_to_read >= g_newsgroup_min_to_read)
            {
                --g_newsgroup_to_read;
            }
            g_newsgroup_ptr->unsubscribe();
            g_newsgroup_ptr->m_to_read = TR_UNSUB;
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
        bits = g_sel.mask;
    }
    if (len < static_cast<int>(command.size()) && command[static_cast<std::size_t>(len)] == '.')
    {
        if (!g_add_groups.empty())
        {
            return -1;
        }
        one_group = true;
        len++;
    }
    std::string cmdstr{command.substr(static_cast<std::size_t>(len))};

    g_status.init(g_newsgroup_to_read.value_of());

    if (one_group)
    {
        goto break_out;
    }

    for (AddGroupIndex index : g_add_groups.indices())
    {
        AddGroup *gp = &g_add_groups[index];
        if (!(gp->m_flags & g_sel.mask) ^ !!bits)
        {
            if (gp->add_group_perform(cmdstr, 0) < 0)
            {
                break;
            }
        }
        g_status.status(g_newsgroup_to_read.value_of(), 50);
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

    g_status.perform_count++;
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
