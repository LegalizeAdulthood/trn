/* kfile.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/kfile.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/artsrch.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/color.h>
#include <trn/hash.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngstuff.h>
#include <trn/rcstuff.h>
#include <trn/rt-process.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

KillFileProcessor g_kill{};

KillFilePersistence g_kill_store{};

static void mention(std::string_view str);
static bool kill_file_junk(Article &article, int killmask);
static int  write_local_thread_commands(int keylen, HashDatum *data, int extra);
static int  write_global_thread_commands(int keylen, HashDatum *data, int appending);
static int  age_thread_commands(int keylen, HashDatum *data, int elapsed_days);
static std::string_view skip_leading_space(std::string_view text);

static constexpr std::string_view s_thread_cmd_ltr{"JK,j+S.m"};
static AutoKillFlags              s_thread_cmd_flag[]{
    AUTO_KILL_THD, AUTO_KILL_SBJ, AUTO_KILL_FOL, AUTO_KILL_1, AUTO_SEL_THD, AUTO_SEL_SBJ, AUTO_SEL_FOL, AUTO_SEL_1,
};
static constexpr std::string_view s_kill_global{KILL_GLOBAL};
static constexpr std::string_view s_kill_local{KILL_LOCAL};
static constexpr std::string_view s_kill_threads{KILL_THREADS};

inline long kill_file_day_num(long x)
{
    return (long) std::time(nullptr) / 86400 - 10490 - x;
}

void KillFilePersistence::init()
{
    std::string kill_threads = get_env_var("KILLTHREADS", s_kill_threads);
    if (!kill_threads.empty() && kill_threads != "none")
    {
        day_num = kill_file_day_num(0);
        thread_cnt = 0;
        change_thread_cnt = 0;
        std::FILE *fp = std::fopen(file_exp(kill_threads).c_str(), "r");
        if (fp != nullptr)
        {
            g_msg_id_hash = new HashTable(1999, msg_id_cmp);
            for (std::string thread_line = get_a_line(fp); !thread_line.empty(); thread_line = get_a_line(fp))
            {
                const std::string_view line{thread_line};
                if (line.front() == '<')
                {
                    std::string_view  message_id = line;
                    std::string_view  command{","};
                    const std::size_t split = line.find(' ');
                    if (split != std::string_view::npos)
                    {
                        message_id = line.substr(0, split);
                        command = line.substr(split + 1);
                    }
                    const char       command_char = command.empty() ? '\0' : command.front();
                    std::string_view age_text = command.empty() ? std::string_view{} : command.substr(1);
                    age_text = skip_leading_space(age_text);
                    long command_day{};
                    (void) std::from_chars(age_text.data(), age_text.data() + age_text.size(), command_day);
                    const int age = static_cast<int>(day_num - command_day);
                    if (age > KF_MAX_DAYS)
                    {
                        change_thread_cnt++;
                        continue;
                    }
                    const std::size_t thread_cmd = s_thread_cmd_ltr.find(command_char);
                    if (thread_cmd != std::string_view::npos)
                    {
                        int       auto_flag = s_thread_cmd_flag[thread_cmd];
                        HashDatum data = g_hash_cursor.fetch(g_msg_id_hash, message_id);
                        if (!data.dat_ptr)
                        {
                            data = make_pending_msg_id(message_id, auto_flag | age);
                        }
                        else
                        {
                            change_thread_cnt++;
                            data.dat_len = auto_flag | age;
                        }
                        g_hash_cursor.store_last(data);
                    }
                    thread_cnt++;
                }
            }
            std::fclose(fp);
        }
        state |= KFS_GLOBAL_THREAD_FILE;
        local_change_clear = KFS_LOCAL_CHANGES;
        thread_change_set = KFS_THREAD_CHANGES;
    }
    else
    {
        local_change_clear = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
        thread_change_set = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
    }
}

static void mention(std::string_view str)
{
    if (g_verbose)
    {
        g_color.print(COLOR_NOTICE, str);
        newline();
    }
    else
    {
        std::putchar('.');
    }
    std::fflush(stdout);
}

static bool is_space(char ch)
{
    return std::isspace(static_cast<unsigned char>(ch));
}

static std::string_view skip_leading_space(std::string_view text)
{
    const std::string_view::const_iterator first = std::find_if_not(text.begin(), text.end(), is_space);
    text.remove_prefix(static_cast<std::size_t>(first - text.begin()));
    return text;
}

static std::string_view skip_non_space_prefix(std::string_view text)
{
    const std::string_view::const_iterator first = std::find_if(text.begin(), text.end(), is_space);
    text.remove_prefix(static_cast<std::size_t>(first - text.begin()));
    return text;
}

int KillFileProcessor::do_file(std::FILE *kfp, int entering)
{
    bool first_time = (entering && !first);
    char last_kill_type = '\0';
    int  thread_kill_cnt = 0;
    int  thread_select_cnt = 0;

    g_nav.target.num = article_after(g_last_art);
    first = g_first_art;
    std::fseek(kfp,0L,0);                    // rewind file
    for (std::string kill_line = get_a_line(kfp); !kill_line.empty(); kill_line = get_a_line(kfp))
    {
        if (kill_line.back() == '\n')
        {
            kill_line.pop_back();
        }
        const std::string_view line = skip_leading_space(kill_line);
        if (line.size() >= 4 && line.substr(0, 4) == "THRU")
        {
            const std::string      rc_name = g_newsgroup_ptr->m_rc->name.generic_string();
            const std::size_t      len = rc_name.size();
            const std::string_view thru_args = skip_leading_space(line.substr(4));
            if (thru_args.size() <= len || thru_args.substr(0, len) != rc_name ||
                !std::isspace(static_cast<unsigned char>(thru_args[len])))
            {
                continue;
            }
            const std::string_view article_text = skip_leading_space(thru_args.substr(len));
            long                   article_num{};
            (void) std::from_chars(article_text.data(), article_text.data() + article_text.size(), article_num);
            first = ArticleNum{article_num + 1};
            first = std::max(first, g_first_art);
            if (first > g_last_art)
            {
                first = article_after(g_last_art);
            }
            continue;
        }
        if (!line.empty() && line.front() == 'I')
        {
            const std::string_view include_text = skip_leading_space(skip_non_space_prefix(line.substr(1)));
            if (include_text.empty())
            {
                continue;
            }
            std::string include_name = file_exp(include_text);
            if (include_name.empty())
            {
                continue;
            }
            if (include_name.find('/') == std::string::npos)
            {
                set_newsgroup_name(include_name);
                include_name = file_exp(get_env_var("KILLLOCAL", s_kill_local));
                set_newsgroup_name(g_newsgroup_ptr->rc_line());
            }
            std::FILE *incfile = std::fopen(include_name.c_str(), "r");
            if (incfile != nullptr)
            {
                int ret = do_file(incfile, entering);
                std::fclose(incfile);
                if (ret)
                {
                    return ret;
                }
            }
            continue;
        }
        std::string_view command_view = line;
        if (!command_view.empty() && command_view.front() == 'X') // exit command?
        {
            if (entering)
            {
                exit_cmds = true;
                continue;
            }
            command_view.remove_prefix(1);
        }
        else if (!entering)
        {
            continue;
        }
        std::string command{command_view};

        if (!command.empty() && command.front() == '&')
        {
            mention(command);
            switcheroo(command);
        }
        else if (!command.empty() && command.front() == '/')
        {
            g_kill_store.state |= KFS_NORMAL_LINES;
            if (g_first_art > g_last_art)
            {
                continue;
            }
            if (last_kill_type)
            {
                if (g_status.end(g_newsgroup_ptr->m_to_read, "article"))
                {
                    mentioned = true;
                    carriage_return();
                    fmt::print("{}", g_msg);
                    newline();
                }
            }
            g_status.init(g_newsgroup_ptr->m_to_read);
            last_kill_type = '/';
            mention(command);
            mentioned = true;
            switch (art_search(command, false))
            {
            case SRCH_ABORT:
                continue;

            case SRCH_INTR:
                if (g_verbose)
                {
                    fmt::print("\n(Interrupted at article {})\n", g_nav.target.num.value_of());
                }
                else
                {
                    fmt::print("\n(Intr at {})\n", g_nav.target.num.value_of());
                }
                term_down(2);
                return -1;

            case SRCH_DONE:
                break;

            case SRCH_SUBJ_DONE:
                // std::fputs("\tsubject not found (?)\n",stdout);
                break;

            case SRCH_NOT_FOUND:
                // std::fputs("\tnot found\n",stdout);
                break;

            case SRCH_FOUND:
                // std::fputs("\tfound\n",stdout);
                break;

            case SRCH_ERROR:
                break;
            }
        }
        else if (first_time && !command.empty() && command.front() == '<')
        {
            if (last_kill_type != '<')
            {
                if (last_kill_type)
                {
                    if (g_status.end(g_newsgroup_ptr->m_to_read, "article"))
                    {
                        mentioned = true;
                        carriage_return();
                        fmt::print("{}", g_msg);
                        newline();
                    }
                }
                g_status.init(g_newsgroup_ptr->m_to_read);
                last_kill_type = '<';
            }
            const std::string_view command_text{command};
            std::string_view       msg_id{command_text};
            std::string_view       cmd{"T,"};
            const std::size_t      split = command_text.find(' ');
            if (split != std::string_view::npos)
            {
                msg_id = command_text.substr(0, split);
                cmd = command_text.substr(split + 1);
            }
            Article *ap = get_article(msg_id);
            if (ap != nullptr)
            {
                if ((ap->m_flags & AF_FAKE) && !ap->m_child1)
                {
                    std::string_view thread_cmd = cmd;
                    if (!thread_cmd.empty() && thread_cmd.front() == 'T')
                    {
                        thread_cmd.remove_prefix(1);
                    }
                    const std::size_t thread_cmd_index =
                        thread_cmd.empty() ? std::string_view::npos : s_thread_cmd_ltr.find(thread_cmd.front());
                    if (thread_cmd_index != std::string_view::npos)
                    {
                        ap->m_auto_flags = s_thread_cmd_flag[thread_cmd_index];
                        if (ap->m_auto_flags & AUTO_KILL_MASK)
                        {
                            thread_kill_cnt++;
                        }
                        else
                        {
                            thread_select_cnt++;
                        }
                    }
                }
                else
                {
                    g_nav.target = {ap->article_num(), ap};
                    perform(cmd,false);
                    if (ap->m_auto_flags & AUTO_SEL_MASK)
                    {
                        thread_select_cnt++;
                    }
                    else if (ap->m_auto_flags & AUTO_KILL_MASK)
                    {
                        thread_kill_cnt++;
                    }
                }
            }
            g_nav.target.num = article_after(g_last_art);
            g_kill_store.state |= KFS_THREAD_LINES;
        }
        else if (!command.empty() && command.front() == '<')
        {
            g_kill_store.state |= KFS_THREAD_LINES;
        }
        else if (!command.empty() && command.front() == '*')
        {
            int killmask = AF_UNREAD;
            switch (command.size() > 1 ? command[1] : '\0')
            {
            case 'X':
                killmask |= g_sel.mask; // don't kill selected articles
                // FALL THROUGH

            case 'j':
                g_cache.walk(kill_file_junk, killmask);
                break;
            }
            g_kill_store.state |= KFS_NORMAL_LINES;
        }
    }
    if (thread_kill_cnt)
    {
        mention(fmt::format("{} auto-kill command{}.", thread_kill_cnt, plural(thread_kill_cnt)));
        mentioned = true;
    }
    if (thread_select_cnt)
    {
        mention(fmt::format("{} auto-select command{}.", thread_select_cnt, plural(thread_select_cnt)));
        mentioned = true;
    }
    if (last_kill_type)
    {
        if (g_status.end(g_newsgroup_ptr->m_to_read, "article"))
        {
            mentioned = true;
            carriage_return();
            fmt::print("{}", g_msg);
            newline();
        }
    }
    return 0;
}

static bool kill_file_junk(Article &article, int killmask)
{
    if ((article.m_flags & killmask) == AF_UNREAD)
    {
        article.set_read();
    }
    else if (article.m_flags & g_sel.mask)
    {
        article.m_flags &= ~static_cast<ArticleFlags>(g_sel.mask);
        if (!g_selected_count--)
        {
            g_selected_count = 0;
        }
    }
    return false;
}

void KillFileProcessor::kill_unwanted(ArticleNum starting, std::string_view message, int entering)
{
    bool intr = false;                  // did we get an interrupt?
    MinorMode oldmode = g_mode;
    bool anytokill = (g_newsgroup_ptr->m_to_read > 0);

    set_mode(GM_READ,MM_PROCESSING_KILL);
    if ((entering || exit_cmds) && (local_kfp || global_kfp))
    {
        exit_cmds = false;
        ArticleNum oldfirst = g_first_art;
        g_first_art = starting;
        clear();
        if (!message.empty() && (g_verbose || entering))
        {
            fmt::print("{}", message);
        }

        mentioned = false;
        if (local_kfp)
        {
            if (entering)
            {
                g_kill_store.state |= KFS_LOCAL_CHANGES;
            }
            intr = do_file(local_kfp, entering);
        }
        open(KF_GLOBAL); // Just in case the name changed
        if (global_kfp && !intr)
        {
            intr = do_file(global_kfp, entering);
        }
        newline();
        if (entering && mentioned && g_novice_delays)
        {
            if (g_verbose)
            {
                get_anything();
            }
            else
            {
                pad(g_just_a_sec);
            }
        }
        if (anytokill)                  // if there was anything to kill
        {
            g_force_last = false;        // allow for having killed it all
        }
        g_first_art = oldfirst;
    }
    if (!entering && (g_kill_store.state & KFS_LOCAL_CHANGES) && !intr)
    {
        g_kill_store.rewrite_kill_file(g_last_art);
    }
    set_mode(g_general_mode,oldmode);
}

static int write_local_thread_commands(int keylen, HashDatum *data, int extra)
{
    return g_kill_store.write_local_thread_commands(keylen, data, extra);
}

int KillFilePersistence::write_local_thread_commands(int keylen, HashDatum *data, int extra)
{
    Article *ap = (Article *) data->dat_ptr;
    int      autofl = ap->m_auto_flags;
    char     ch;

    if (autofl && ((ap->m_flags & AF_EXISTS) || ap->m_child1))
    {
        // The arrays are in priority order, so find highest priority bit.
        for (std::size_t i = 0; i < s_thread_cmd_ltr.size(); i++)
        {
            if (autofl & s_thread_cmd_flag[i])
            {
                ch = s_thread_cmd_ltr[i];
                break;
            }
        }
        fmt::print(new_fp, "{} T{}\n", ap->msg_id_view(), ch);
    }
    return 0;
}

void KillFilePersistence::rewrite_kill_file(ArticleNum thru)
{
    bool                     has_content = (state & (KFS_THREAD_LINES | KFS_GLOBAL_THREAD_FILE)) == KFS_THREAD_LINES;
    bool has_star_commands = false;
    bool needs_newline = false;
    const fs::path killname{file_exp(get_env_var("KILLLOCAL", s_kill_local))};
    std::error_code error;
    std::vector<std::string> kill_lines;

    if (g_kill.local_kfp)
    {
        std::fseek(g_kill.local_kfp, 0L, 0); // rewind current file
        for (std::string line = get_a_line(g_kill.local_kfp); !line.empty(); line = get_a_line(g_kill.local_kfp))
        {
            kill_lines.push_back(line);
        }
        // Close the reader before removing its file on Windows.
        g_kill.close(KF_LOCAL);
    }
    else
    {
        fs::create_directories(killname.parent_path(), error);
    }
    fs::remove(killname, error);        // to prevent file reuse
    state &= ~(local_change_clear | KFS_NORMAL_LINES);
    new_fp = std::fopen(killname.string().c_str(), "w");
    if (new_fp != nullptr)
    {
        fmt::print(new_fp, "THRU {} {}\n", g_newsgroup_ptr->m_rc->name.generic_string(), thru.value_of());
        for (const std::string &kill_line : kill_lines)
        {
            const std::string_view line{kill_line};
            if (line.substr(0, 4) == "THRU")
            {
                const std::string rc_name = g_newsgroup_ptr->m_rc->name.generic_string();
                const std::size_t len = rc_name.size();
                const std::string_view thru_args = skip_leading_space(line.substr(4));
                if (!thru_args.empty() && std::isdigit(static_cast<unsigned char>(thru_args.front())))
                {
                    continue;
                }
                if (thru_args.size() < len || thru_args.substr(0, len) != rc_name ||
                    (thru_args.size() > len && !std::isspace(static_cast<unsigned char>(thru_args[len]))))
                {
                    fmt::print(new_fp, "{}", kill_line);
                    needs_newline = kill_line.back() != '\n';
                }
                continue;
            }
            const std::string_view line_text = skip_leading_space(kill_line);
            // Leave out any outdated thread commands
            if (!line_text.empty() && (line_text.front() == 'T' || line_text.front() == '<'))
            {
                continue;
            }
            // Write star commands after other kill commands
            if (!line_text.empty() && line_text.front() == '*')
            {
                has_star_commands = true;
            }
            else
            {
                fmt::print(new_fp, "{}", kill_line);
                needs_newline = kill_line.back() != '\n';
            }
            has_content = true;
        }
        if (needs_newline)
        {
            std::putc('\n', new_fp);
        }
        if (has_star_commands)
        {
            for (const std::string &kill_line : kill_lines)
            {
                const std::string_view line_text = skip_leading_space(kill_line);
                if (!line_text.empty() && line_text.front() == '*')
                {
                    fmt::print(new_fp, "{}", kill_line);
                    needs_newline = kill_line.back() != '\n';
                }
            }
            if (needs_newline)
            {
                std::putc('\n', new_fp);
            }
        }
        if (!(state & KFS_GLOBAL_THREAD_FILE))
        {
            // Append all the still-valid thread commands
            g_hash_cursor.walk(g_msg_id_hash, &::write_local_thread_commands, 0);
        }
        std::fclose(new_fp);
        if (!has_content)
        {
            fs::remove(killname, error);
        }
        g_kill.open(KF_LOCAL); // and reopen local file
    }
    else
    {
        fmt::print("Can't create {}\n", killname.string());
    }
}

static int write_global_thread_commands(int keylen, HashDatum *data, int appending)
{
    int autofl;
    int age;
    std::string_view msgid;
    char             ch;

    if (data->dat_len)
    {
        if (appending)
        {
            return 0;
        }
        autofl = data->dat_len;
        age = autofl & KF_AGE_MASK;
        msgid = hash_msg_id_view(*data);
    }
    else
    {
        Article *ap = (Article *) data->dat_ptr;
        autofl = ap->m_auto_flags;
        if (!autofl || (appending && (autofl & AUTO_OLD)))
        {
            return 0;
        }
        ap->m_auto_flags |= AUTO_OLD;
        age = 0;
        msgid = ap->msg_id_view();
    }

    // The arrays are in priority order, so find highest priority bit.
    for (std::size_t i = 0; i < s_thread_cmd_ltr.size(); i++)
    {
        if (autofl & s_thread_cmd_flag[i])
        {
            ch = s_thread_cmd_ltr[i];
            break;
        }
    }
    fmt::print(g_kill_store.new_fp, "{} {} {}\n", msgid, ch, g_kill_store.day_num - age);
    g_kill_store.thread_cnt++;

    return 0;
}

static int age_thread_commands(int keylen, HashDatum *data, int elapsed_days)
{
    return g_kill_store.age_thread_commands(keylen, data, elapsed_days);
}

int KillFilePersistence::age_thread_commands(int keylen, HashDatum *data, int elapsed_days)
{
    if (data->dat_len)
    {
        int age = (data->dat_len & KF_AGE_MASK) + elapsed_days;
        if (age > KF_MAX_DAYS)
        {
            free_pending_msg_id(data);
            change_thread_cnt++;
            return -1;
        }
        data->dat_len += elapsed_days;
    }
    else
    {
        Article* ap = (Article*)data->dat_ptr;
        if (ap->m_auto_flags & AUTO_OLD)
        {
            ap->m_auto_flags &= ~AUTO_OLD;
            change_thread_cnt++;
            state |= KFS_THREAD_CHANGES;
        }
    }
    return 0;
}

void update_thread_kill_file()
{
    if (!(g_kill_store.state & KFS_GLOBAL_THREAD_FILE))
    {
        return;
    }

    int elapsed_days = kill_file_day_num(g_kill_store.day_num);
    if (elapsed_days)
    {
        g_hash_cursor.walk(g_msg_id_hash, &age_thread_commands, elapsed_days);
        g_kill_store.day_num += elapsed_days;
    }

    if (!(g_kill_store.state & KFS_THREAD_CHANGES))
    {
        return;
    }

    const fs::path kname{file_exp(get_env_var("KILLTHREADS", s_kill_threads))};
    std::error_code error;
    fs::create_directories(kname.parent_path(), error);
    if (g_kill_store.change_thread_cnt * 5 > g_kill_store.thread_cnt)
    {
        fs::remove(kname, error);       // to prevent file reuse
        g_kill_store.new_fp = std::fopen(kname.string().c_str(), "w");
        if (g_kill_store.new_fp == nullptr)
        {
            return; // Yikes!
        }
        g_kill_store.thread_cnt = 0;
        g_kill_store.change_thread_cnt = 0;
        g_hash_cursor.walk(g_msg_id_hash, &write_global_thread_commands, 0); // Rewrite
    }
    else
    {
        g_kill_store.new_fp = std::fopen(kname.string().c_str(), "a");
        if (g_kill_store.new_fp == nullptr)
        {
            return; // Yikes!
        }
        g_hash_cursor.walk(g_msg_id_hash, &write_global_thread_commands, 1); // Append
    }
    std::fclose(g_kill_store.new_fp);

    g_kill_store.state &= ~KFS_THREAD_CHANGES;
}

// edit KILL file for newsgroup

void edit_kill_file()
{
    fs::path kill_file;

    if (g_in_ng)
    {
        if (g_kill_store.state & KFS_LOCAL_CHANGES)
        {
            g_kill_store.rewrite_kill_file(g_last_art);
        }
        if (!(g_kill_store.state & KFS_GLOBAL_THREAD_FILE))
        {
            for (Subject *sp = g_subj.first; sp; sp = sp->m_next)
            {
                sp->clear_subject();
            }
        }
        kill_file = file_exp(get_env_var("KILLLOCAL", s_kill_local));
    }
    else
    {
        kill_file = file_exp(get_env_var("KILLGLOBAL", s_kill_global));
    }
    std::error_code error;
    fs::create_directories(kill_file.parent_path(), error);
    if (!error)
    {
        const std::string command = editor_command(kill_file.string());
        fmt::print("\nEditing {} KILL file:\n{}\n", g_in_ng ? "local" : "global", command);
        term_down(3);
        reset_tty();                      // make sure tty is friendly
        do_shell(SH, command);            // invoke the shell
        no_echo();                       // and make terminal
        cr_mode();                       // unfriendly again
        g_kill.open(g_in_ng);
        if (g_kill.local_kfp)
        {
            std::fseek(g_kill.local_kfp, 0L, 0); // rewind file
            g_kill_store.state &= ~KFS_NORMAL_LINES;
            for (std::string kill_line = get_a_line(g_kill.local_kfp); !kill_line.empty();
                 kill_line = get_a_line(g_kill.local_kfp))
            {
                const std::string_view line = skip_leading_space(kill_line);
                if (!line.empty() && (line.front() == '/' || line.front() == '*'))
                {
                    g_kill_store.state |= KFS_NORMAL_LINES;
                }
                else if (!line.empty() && line.front() == '<')
                {
                    std::string_view  msg_id{line};
                    std::string_view  cmd{","};
                    const std::size_t split = line.find(' ');
                    if (split != std::string_view::npos)
                    {
                        msg_id = line.substr(0, split);
                        cmd = line.substr(split + 1);
                    }
                    Article *ap = get_article(msg_id);
                    if (ap != nullptr)
                    {
                        std::string_view thread_cmd = cmd;
                        if (!thread_cmd.empty() && thread_cmd.front() == 'T')
                        {
                            thread_cmd.remove_prefix(1);
                        }
                        const std::size_t thread_cmd_index =
                            thread_cmd.empty() ? std::string_view::npos
                                               : s_thread_cmd_ltr.find(thread_cmd.front());
                        if (thread_cmd_index != std::string_view::npos)
                        {
                            ap->m_auto_flags |= s_thread_cmd_flag[thread_cmd_index];
                        }
                    }
                }
            }
        }
    }
    else
    {
        fmt::print("Can't make {}\n", kill_file.string());
        term_down(1);
    }
}

void KillFileProcessor::open(int local)
{
    const fs::path kname{
        file_exp(local ? get_env_var("KILLLOCAL", s_kill_local) : get_env_var("KILLGLOBAL", s_kill_global))};

    // delete the file if it is empty
    if (fs::exists(kname) && fs::file_size(kname) == 0)
    {
        fs::remove(kname);
    }
    close(local);
    if (local)
    {
        local_kfp = std::fopen(kname.string().c_str(), "r");
    }
    else
    {
        global_kfp = std::fopen(kname.string().c_str(), "r");
    }
}

void KillFileProcessor::close(int local)
{
    std::FILE *&reader = local ? local_kfp : global_kfp;
    if (reader)
    {
        std::fclose(reader);
        reader = nullptr;
    }
}

void kill_file_append(std::string_view cmd, bool local)
{
    const fs::path kill_file{
        file_exp(local ? get_env_var("KILLLOCAL", s_kill_local) : get_env_var("KILLGLOBAL", s_kill_global))};
    if (!make_dir(kill_file, MD_FILE))
    {
        if (g_verbose)
        {
            fmt::print("\nDepositing command in {}...", kill_file.string());
        }
        else
        {
            fmt::print("\n--> {}...", kill_file.string());
        }
        std::fflush(stdout);
        if (g_novice_delays)
        {
            sleep(2);
        }
        std::error_code      error;
        const std::uintmax_t file_size = fs::file_size(kill_file, error);
        bool                 needs_newline = !error && file_size > 0;
        if (needs_newline)
        {
            std::ifstream input{kill_file, std::ios::binary};
            input.seekg(-1, std::ios::end);
            char last_char{};
            input.get(last_char);
            needs_newline = input && last_char != '\n';
        }

        std::ofstream output{kill_file, std::ios::app};
        if (output)
        {
            if (needs_newline)
            {
                output.put('\n');
            }
            output << cmd << '\n';
            output.close();
            if (local && !g_kill.local_kfp)
            {
                g_kill.open(KF_LOCAL);
            }
            fmt::print("done\n");
        }
        else
        {
            fmt::print("Can't open {}\n", kill_file.string());
        }
        term_down(2);
    }
    g_kill_store.state |= KFS_NORMAL_LINES;
}
