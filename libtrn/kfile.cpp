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

namespace fs = std::filesystem;

std::FILE         *g_local_kfp{};             // local (for this newsgroup) file
KillFileStateFlags g_kf_state{};              // the state of our kill files
KillFileStateFlags g_kfs_thread_change_set{}; // bits to set for thread changes
int                g_kf_change_thread_cnt{};  // # entries changed from old to new
ArticleNum         g_kill_first{};            // used as g_firstart when killing

static void mention(std::string_view str);
static bool kill_file_junk(char *ptr, int killmask);
static int  do_kill_file(std::FILE *kfp, int entering);
static void rewrite_kill_file(ArticleNum thru);
static int  write_local_thread_commands(int keylen, HashDatum *data, int extra);
static int  write_global_thread_commands(int keylen, HashDatum *data, int appending);
static int  age_thread_commands(int keylen, HashDatum *data, int elapsed_days);
static std::string_view skip_leading_space(std::string_view text);

static std::FILE         *s_global_kill_file_fp{};                // global article killer file
static KillFileStateFlags s_kill_file_state_local_change_clear{}; // bits to clear local changes
static int                s_kill_file_thread_cnt{};               // # entries in the thread kfile
static long               s_kill_file_day_num{};                  // day number for thread killfile
static bool               s_exit_cmds{};
static constexpr char     s_thread_cmd_ltr[] = "JK,j+S.m";
static AutoKillFlags      s_thread_cmd_flag[]{
    AUTO_KILL_THD, AUTO_KILL_SBJ, AUTO_KILL_FOL, AUTO_KILL_1, AUTO_SEL_THD, AUTO_SEL_SBJ, AUTO_SEL_FOL, AUTO_SEL_1,
};
static constexpr char s_kill_global[] = KILL_GLOBAL;
static constexpr char s_kill_local[] = KILL_LOCAL;
static constexpr char s_kill_threads[] = KILL_THREADS;
static bool       s_kill_mentioned;
static std::FILE *s_new_kill_file_fp{};

inline long kill_file_day_num(long x)
{
    return (long) std::time(nullptr) / 86400 - 10490 - x;
}

void kill_file_init()
{
    std::string kill_threads = get_env_var("KILLTHREADS", s_kill_threads);
    if (!kill_threads.empty() && kill_threads != "none")
    {
        s_kill_file_day_num = kill_file_day_num(0);
        s_kill_file_thread_cnt = 0;
        g_kf_change_thread_cnt = 0;
        std::FILE *fp = std::fopen(file_exp(kill_threads).c_str(), "r");
        if (fp != nullptr)
        {
            g_msg_id_hash = hash_create(1999, msg_id_cmp);
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
                    const int age = static_cast<int>(s_kill_file_day_num - command_day);
                    if (age > KF_MAX_DAYS)
                    {
                        g_kf_change_thread_cnt++;
                        continue;
                    }
                    const std::size_t thread_cmd = std::string_view{s_thread_cmd_ltr}.find(command_char);
                    if (thread_cmd != std::string_view::npos)
                    {
                        int       auto_flag = s_thread_cmd_flag[thread_cmd];
                        HashDatum data = hash_fetch(g_msg_id_hash, message_id);
                        if (!data.dat_ptr)
                        {
                            data = make_pending_msg_id(message_id, auto_flag | age);
                        }
                        else
                        {
                            g_kf_change_thread_cnt++;
                            data.dat_len = auto_flag | age;
                        }
                        hash_store_last(data);
                    }
                    s_kill_file_thread_cnt++;
                }
            }
            std::fclose(fp);
        }
        g_kf_state |= KFS_GLOBAL_THREAD_FILE;
        s_kill_file_state_local_change_clear = KFS_LOCAL_CHANGES;
        g_kfs_thread_change_set = KFS_THREAD_CHANGES;
    }
    else
    {
        s_kill_file_state_local_change_clear = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
        g_kfs_thread_change_set = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
    }
}

static void mention(std::string_view str)
{
    if (g_verbose)
    {
        color_string(COLOR_NOTICE,str);
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

static int do_kill_file(std::FILE *kfp, int entering)
{
    bool first_time = (entering && !g_kill_first);
    char last_kill_type = '\0';
    int  thread_kill_cnt = 0;
    int  thread_select_cnt = 0;

    g_art = article_after(g_last_art);
    g_kill_first = g_first_art;
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
            g_kill_first = ArticleNum{std::atol(thru_args.data() + len + 1) + 1};
            g_kill_first = std::max(g_kill_first, g_first_art);
            if (g_kill_first > g_last_art)
            {
                g_kill_first = article_after(g_last_art);
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
                set_newsgroup_name(g_newsgroup_ptr->rc_line_c_str());
            }
            std::FILE *incfile = std::fopen(include_name.c_str(), "r");
            if (incfile != nullptr)
            {
                int ret = do_kill_file(incfile, entering);
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
                s_exit_cmds = true;
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
            std::copy(command.begin(), command.end(), g_buf);
            g_buf[command.size()] = '\0';
            switcheroo();
        }
        else if (!command.empty() && command.front() == '/')
        {
            g_kf_state |= KFS_NORMAL_LINES;
            if (g_first_art > g_last_art)
            {
                continue;
            }
            if (last_kill_type)
            {
                if (perform_status_end(g_newsgroup_ptr->m_to_read, "article"))
                {
                    s_kill_mentioned = true;
                    carriage_return();
                    std::fputs(g_msg.c_str(), stdout);
                    newline();
                }
            }
            perform_status_init(g_newsgroup_ptr->m_to_read);
            last_kill_type = '/';
            mention(command);
            s_kill_mentioned = true;
            switch (art_search(command.data(), static_cast<int>(command.size() + 1), false))
            {
            case SRCH_ABORT:
                continue;

            case SRCH_INTR:
                if (g_verbose)
                {
                    std::printf("\n(Interrupted at article %ld)\n", g_art.value_of());
                }
                else
                {
                    std::printf("\n(Intr at %ld)\n", g_art.value_of());
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
                    if (perform_status_end(g_newsgroup_ptr->m_to_read, "article"))
                    {
                        s_kill_mentioned = true;
                        carriage_return();
                        std::fputs(g_msg.c_str(), stdout);
                        newline();
                    }
                }
                perform_status_init(g_newsgroup_ptr->m_to_read);
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
                        thread_cmd.empty() ? std::string_view::npos
                                           : std::string_view{s_thread_cmd_ltr}.find(thread_cmd.front());
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
                    g_art = ap->article_num();
                    g_artp = ap;
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
            g_art = article_after(g_last_art);
            g_kf_state |= KFS_THREAD_LINES;
        }
        else if (!command.empty() && command.front() == '<')
        {
            g_kf_state |= KFS_THREAD_LINES;
        }
        else if (!command.empty() && command.front() == '*')
        {
            int killmask = AF_UNREAD;
            switch (command.size() > 1 ? command[1] : '\0')
            {
            case 'X':
                killmask |= g_sel_mask; // don't kill selected articles
                // FALL THROUGH

            case 'j':
                article_walk(kill_file_junk, killmask);
                break;
            }
            g_kf_state |= KFS_NORMAL_LINES;
        }
    }
    if (thread_kill_cnt)
    {
        mention(fmt::format("{} auto-kill command{}.", thread_kill_cnt, plural(thread_kill_cnt)));
        s_kill_mentioned = true;
    }
    if (thread_select_cnt)
    {
        mention(fmt::format("{} auto-select command{}.", thread_select_cnt, plural(thread_select_cnt)));
        s_kill_mentioned = true;
    }
    if (last_kill_type)
    {
        if (perform_status_end(g_newsgroup_ptr->m_to_read, "article"))
        {
            s_kill_mentioned = true;
            carriage_return();
            std::fputs(g_msg.c_str(), stdout);
            newline();
        }
    }
    return 0;
}

static bool kill_file_junk(char *ptr, int killmask)
{
    Article* ap = (Article*)ptr;
    if ((ap->m_flags & killmask) == AF_UNREAD)
    {
        ap->set_read();
    }
    else if (ap->m_flags & g_sel_mask)
    {
        ap->m_flags &= ~static_cast<ArticleFlags>(g_sel_mask);
        if (!g_selected_count--)
        {
            g_selected_count = 0;
        }
    }
    return false;
}

void kill_unwanted(ArticleNum starting, std::string_view message, int entering)
{
    bool intr = false;                  // did we get an interrupt?
    MinorMode oldmode = g_mode;
    bool anytokill = (g_newsgroup_ptr->m_to_read > 0);

    set_mode(GM_READ,MM_PROCESSING_KILL);
    if ((entering || s_exit_cmds) && (g_local_kfp || s_global_kill_file_fp))
    {
        s_exit_cmds = false;
        ArticleNum oldfirst = g_first_art;
        g_first_art = starting;
        clear();
        if (!message.empty() && (g_verbose || entering))
        {
            fmt::print("{}", message);
        }

        s_kill_mentioned = false;
        if (g_local_kfp)
        {
            if (entering)
            {
                g_kf_state |= KFS_LOCAL_CHANGES;
            }
            intr = do_kill_file(g_local_kfp, entering);
        }
        open_kill_file(KF_GLOBAL);          // Just in case the name changed
        if (s_global_kill_file_fp && !intr)
        {
            intr = do_kill_file(s_global_kill_file_fp, entering);
        }
        newline();
        if (entering && s_kill_mentioned && g_novice_delays)
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
    if (!entering && (g_kf_state & KFS_LOCAL_CHANGES) && !intr)
    {
        rewrite_kill_file(g_last_art);
    }
    set_mode(g_general_mode,oldmode);
}

static int write_local_thread_commands(int keylen, HashDatum *data, int extra)
{
    Article* ap = (Article*)data->dat_ptr;
    int autofl = ap->m_auto_flags;
    char ch;

    if (autofl && ((ap->m_flags & AF_EXISTS) || ap->m_child1))
    {
        const std::string_view thread_cmd_letters{s_thread_cmd_ltr, sizeof(s_thread_cmd_ltr) - 1};

        // The arrays are in priority order, so find highest priority bit.
        for (std::size_t i = 0; i < thread_cmd_letters.size(); i++)
        {
            if (autofl & s_thread_cmd_flag[i])
            {
                ch = thread_cmd_letters[i];
                break;
            }
        }
        fmt::print(s_new_kill_file_fp, "{} T{}\n", ap->msg_id_view(), ch);
    }
    return 0;
}

static void rewrite_kill_file(ArticleNum thru)
{
    bool has_content = (g_kf_state & (KFS_THREAD_LINES|KFS_GLOBAL_THREAD_FILE))
                                 == KFS_THREAD_LINES;
    bool has_star_commands = false;
    bool needs_newline = false;
    const fs::path killname{file_exp(get_env_var("KILLLOCAL", s_kill_local))};
    std::error_code error;

    if (g_local_kfp)
    {
        std::fseek(g_local_kfp, 0L, 0);       // rewind current file
    }
    else
    {
        fs::create_directories(killname.parent_path(), error);
    }
    fs::remove(killname, error);        // to prevent file reuse
    g_kf_state &= ~(s_kill_file_state_local_change_clear | KFS_NORMAL_LINES);
    s_new_kill_file_fp = std::fopen(killname.string().c_str(), "w");
    if (s_new_kill_file_fp != nullptr)
    {
        fmt::print(s_new_kill_file_fp, "THRU {} {}\n", g_newsgroup_ptr->m_rc->name.generic_string(), thru.value_of());
        for (std::string kill_line = g_local_kfp ? get_a_line(g_local_kfp) : std::string{}; !kill_line.empty();
             kill_line = get_a_line(g_local_kfp))
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
                    fmt::print(s_new_kill_file_fp, "{}", kill_line);
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
                fmt::print(s_new_kill_file_fp, "{}", kill_line);
                needs_newline = kill_line.back() != '\n';
            }
            has_content = true;
        }
        if (needs_newline)
        {
            std::putc('\n', s_new_kill_file_fp);
        }
        if (has_star_commands)
        {
            std::fseek(g_local_kfp,0L,0);                     // rewind file
            while (true)
            {
                const std::string kill_line = get_a_line(g_local_kfp);
                if (kill_line.empty())
                {
                    break;
                }
                const std::string_view line_text = skip_leading_space(kill_line);
                if (!line_text.empty() && line_text.front() == '*')
                {
                    fmt::print(s_new_kill_file_fp, "{}", kill_line);
                    needs_newline = kill_line.back() != '\n';
                }
            }
            if (needs_newline)
            {
                std::putc('\n', s_new_kill_file_fp);
            }
        }
        if (!(g_kf_state & KFS_GLOBAL_THREAD_FILE))
        {
            // Append all the still-valid thread commands
            hash_walk(g_msg_id_hash, write_local_thread_commands, 0);
        }
        std::fclose(s_new_kill_file_fp);
        if (!has_content)
        {
            fs::remove(killname, error);
        }
        open_kill_file(KF_LOCAL);           // and reopen local file
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

    const std::string_view thread_cmd_letters{s_thread_cmd_ltr, sizeof(s_thread_cmd_ltr) - 1};

    // The arrays are in priority order, so find highest priority bit.
    for (std::size_t i = 0; i < thread_cmd_letters.size(); i++)
    {
        if (autofl & s_thread_cmd_flag[i])
        {
            ch = thread_cmd_letters[i];
            break;
        }
    }
    fmt::print(s_new_kill_file_fp, "{} {} {}\n", msgid, ch, s_kill_file_day_num - age);
    s_kill_file_thread_cnt++;

    return 0;
}

static int age_thread_commands(int keylen, HashDatum *data, int elapsed_days)
{
    if (data->dat_len)
    {
        int age = (data->dat_len & KF_AGE_MASK) + elapsed_days;
        if (age > KF_MAX_DAYS)
        {
            free_pending_msg_id(data);
            g_kf_change_thread_cnt++;
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
            g_kf_change_thread_cnt++;
            g_kf_state |= KFS_THREAD_CHANGES;
        }
    }
    return 0;
}

void update_thread_kill_file()
{
    if (!(g_kf_state & KFS_GLOBAL_THREAD_FILE))
    {
        return;
    }

    int elapsed_days = kill_file_day_num(s_kill_file_day_num);
    if (elapsed_days)
    {
        hash_walk(g_msg_id_hash, age_thread_commands, elapsed_days);
        s_kill_file_day_num += elapsed_days;
    }

    if (!(g_kf_state & KFS_THREAD_CHANGES))
    {
        return;
    }

    const fs::path kname{file_exp(get_env_var("KILLTHREADS", s_kill_threads))};
    std::error_code error;
    fs::create_directories(kname.parent_path(), error);
    if (g_kf_change_thread_cnt * 5 > s_kill_file_thread_cnt)
    {
        fs::remove(kname, error);       // to prevent file reuse
        s_new_kill_file_fp = std::fopen(kname.string().c_str(), "w");
        if (s_new_kill_file_fp == nullptr)
        {
            return; // Yikes!
        }
        s_kill_file_thread_cnt = 0;
        g_kf_change_thread_cnt = 0;
        hash_walk(g_msg_id_hash, write_global_thread_commands, 0); // Rewrite
    }
    else
    {
        s_new_kill_file_fp = std::fopen(kname.string().c_str(), "a");
        if (s_new_kill_file_fp == nullptr)
        {
            return; // Yikes!
        }
        hash_walk(g_msg_id_hash, write_global_thread_commands, 1); // Append
    }
    std::fclose(s_new_kill_file_fp);

    g_kf_state &= ~KFS_THREAD_CHANGES;
}

// edit KILL file for newsgroup

void edit_kill_file()
{
    fs::path kill_file;

    if (g_in_ng)
    {
        if (g_kf_state & KFS_LOCAL_CHANGES)
        {
            rewrite_kill_file(g_last_art);
        }
        if (!(g_kf_state & KFS_GLOBAL_THREAD_FILE))
        {
            for (Subject *sp = g_first_subject; sp; sp = sp->m_next)
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
        const std::string command =
            fmt::format("{} {}", file_exp(get_env_var("VISUAL", get_env_var("EDITOR", DEFAULT_EDITOR))),
                        kill_file.string());
        fmt::print("\nEditing {} KILL file:\n{}\n", g_in_ng ? "local" : "global", command);
        term_down(3);
        reset_tty();                      // make sure tty is friendly
        do_shell(SH, command.c_str());   // invoke the shell
        no_echo();                       // and make terminal
        cr_mode();                       // unfriendly again
        open_kill_file(g_in_ng);
        if (g_local_kfp)
        {
            std::fseek(g_local_kfp,0L,0);     // rewind file
            g_kf_state &= ~KFS_NORMAL_LINES;
            for (std::string kill_line = get_a_line(g_local_kfp); !kill_line.empty();
                 kill_line = get_a_line(g_local_kfp))
            {
                const std::string_view line = skip_leading_space(kill_line);
                if (!line.empty() && (line.front() == '/' || line.front() == '*'))
                {
                    g_kf_state |= KFS_NORMAL_LINES;
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
                                               : std::string_view{s_thread_cmd_ltr, sizeof(s_thread_cmd_ltr) - 1}.find(
                                                     thread_cmd.front());
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

void open_kill_file(int local)
{
    const fs::path kname{
        file_exp(local ? get_env_var("KILLLOCAL", s_kill_local) : get_env_var("KILLGLOBAL", s_kill_global))};

    // delete the file if it is empty
    if (fs::exists(kname) && fs::file_size(kname) == 0)
    {
        fs::remove(kname);
    }
    if (local)
    {
        if (g_local_kfp)
        {
            std::fclose(g_local_kfp);
        }
        g_local_kfp = std::fopen(kname.string().c_str(), "r");
    }
    else
    {
        if (s_global_kill_file_fp)
        {
            std::fclose(s_global_kill_file_fp);
        }
        s_global_kill_file_fp = std::fopen(kname.string().c_str(), "r");
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
            if (local && !g_local_kfp)
            {
                open_kill_file(KF_LOCAL);
            }
            fmt::print("done\n");
        }
        else
        {
            fmt::print("Can't open {}\n", kill_file.string());
        }
        term_down(2);
    }
    g_kf_state |= KFS_NORMAL_LINES;
}
