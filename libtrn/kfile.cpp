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
            while (std::fgets(g_buf, sizeof g_buf, fp) != nullptr)
            {
                if (*g_buf == '<')
                {
                    char* split = std::strchr(g_buf,' ');
                    const char *cmd = ",";
                    if (split)
                    {
                        *split++ = '\0';
                        cmd = split;
                    }
                    int age = s_kill_file_day_num - std::atol(cmd + 1);
                    if (age > KF_MAX_DAYS)
                    {
                        g_kf_change_thread_cnt++;
                        continue;
                    }
                    const char *thread_cmd = std::strchr(s_thread_cmd_ltr, *cmd);
                    if (thread_cmd != nullptr)
                    {
                        int auto_flag = s_thread_cmd_flag[thread_cmd - s_thread_cmd_ltr];
                        HashDatum data = hash_fetch(g_msg_id_hash, g_buf);
                        if (!data.dat_ptr)
                        {
                            data = make_pending_msg_id(g_buf, auto_flag | age);
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

static int do_kill_file(std::FILE *kfp, int entering)
{
    bool first_time = (entering && !g_kill_first);
    char last_kill_type = '\0';
    int thread_kill_cnt = 0;
    int thread_select_cnt = 0;
    char* cp;
    char* bp;

    g_art = article_after(g_last_art);
    g_kill_first = g_first_art;
    std::fseek(kfp,0L,0);                    // rewind file
    while (std::fgets(g_buf, LINE_BUF_LEN, kfp) != nullptr)
    {
        if (*(cp = g_buf + std::strlen(g_buf) - 1) == '\n')
        {
            *cp = '\0';
        }
        bp = skip_space(g_buf);
        if (!std::strncmp(bp, "THRU", 4))
        {
            const std::string rc_name = g_newsgroup_ptr->m_rc->name.generic_string();
            const std::size_t  len = rc_name.size();
            cp = skip_space(bp + 4);
            if (std::strncmp(cp, rc_name.c_str(), len) != 0 || !std::isspace(cp[len]))
            {
                continue;
            }
            g_kill_first = ArticleNum{std::atol(cp+len+1)+1};
            g_kill_first = std::max(g_kill_first, g_first_art);
            if (g_kill_first > g_last_art)
            {
                g_kill_first = article_after(g_last_art);
            }
            continue;
        }
        if (*bp == 'I')
        {
            cp = skip_non_space(bp + 1);
            cp = skip_space(cp);
            if (!*cp)
            {
                continue;
            }
            std::string include_name = file_exp(cp);
            if (include_name.empty())
            {
                continue;
            }
            if (!std::strchr(include_name.c_str(), '/'))
            {
                set_newsgroup_name(include_name.c_str());
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
        if (*bp == 'X')                 // exit command?
        {
            if (entering)
            {
                s_exit_cmds = true;
                continue;
            }
            bp++;
        }
        else if (!entering)
        {
            continue;
        }

        if (*bp == '&')
        {
            mention(bp);
            if (bp > g_buf)
            {
                std::strcpy(g_buf, bp);
            }
            switcheroo();
        }
        else if (*bp == '/')
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
            mention(bp);
            s_kill_mentioned = true;
            switch (art_search(bp, (sizeof g_buf) - (bp - g_buf), false))
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
        else if (first_time && *bp == '<')
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
            std::string_view msg_id{bp};
            std::string_view cmd{"T,"};
            char *split = std::strchr(bp,' ');
            if (split)
            {
                msg_id = {bp, static_cast<std::size_t>(split - bp)};
                cmd = split + 1;
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
                    const char *thread_cmd_match =
                        thread_cmd.empty() ? nullptr : std::strchr(s_thread_cmd_ltr, thread_cmd.front());
                    if (thread_cmd_match != nullptr)
                    {
                        ap->m_auto_flags = s_thread_cmd_flag[thread_cmd_match - s_thread_cmd_ltr];
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
        else if (*bp == '<')
        {
            g_kf_state |= KFS_THREAD_LINES;
        }
        else if (*bp == '*')
        {
            int killmask = AF_UNREAD;
            switch (bp[1])
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
        std::sprintf(g_buf,"%ld auto-kill command%s.", (long)thread_kill_cnt,
                plural(thread_kill_cnt));
        mention(g_buf);
        s_kill_mentioned = true;
    }
    if (thread_select_cnt)
    {
        std::sprintf(g_buf,"%ld auto-select command%s.", (long)thread_select_cnt,
                plural(thread_select_cnt));
        mention(g_buf);
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

void kill_unwanted(ArticleNum starting, const char *message, int entering)
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
        if (message && (g_verbose || entering))
        {
            std::fputs(message, stdout);
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
        // The arrays are in priority order, so find highest priority bit.
        for (int i = 0; s_thread_cmd_ltr[i]; i++)
        {
            if (autofl & s_thread_cmd_flag[i])
            {
                ch = s_thread_cmd_ltr[i];
                break;
            }
        }
        fmt::print(s_new_kill_file_fp, "{} T{}\n", ap->msg_id_c_str(), ch);
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
    char* bp;

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
        while (g_local_kfp && std::fgets(g_buf, LINE_BUF_LEN, g_local_kfp) != nullptr)
        {
            if (!std::strncmp(g_buf, "THRU", 4))
            {
                char* cp = g_buf+4;
                const std::string rc_name = g_newsgroup_ptr->m_rc->name.generic_string();
                const std::size_t  len = rc_name.size();
                cp = skip_space(cp);
                if (std::isdigit(*cp))
                {
                    continue;
                }
                if (std::strncmp(cp, rc_name.c_str(), len) != 0 || (cp[len] && !std::isspace(cp[len])))
                {
                    std::fputs(g_buf,s_new_kill_file_fp);
                    needs_newline = !std::strchr(g_buf,'\n');
                }
                continue;
            }
            bp = skip_space(g_buf);
            // Leave out any outdated thread commands
            if (*bp == 'T' || *bp == '<')
            {
                continue;
            }
            // Write star commands after other kill commands
            if (*bp == '*')
            {
                has_star_commands = true;
            }
            else
            {
                std::fputs(g_buf,s_new_kill_file_fp);
                needs_newline = !std::strchr(bp,'\n');
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
            while (std::fgets(g_buf, LINE_BUF_LEN, g_local_kfp) != nullptr)
            {
                bp = skip_space(g_buf);
                if (*bp == '*')
                {
                    std::fputs(g_buf,s_new_kill_file_fp);
                    needs_newline = !std::strchr(bp,'\n');
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
    const char *msgid;
    char        ch;

    if (data->dat_len)
    {
        if (appending)
        {
            return 0;
        }
        autofl = data->dat_len;
        age = autofl & KF_AGE_MASK;
        msgid = hash_msg_id_c_str(*data);
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
        msgid = ap->msg_id_c_str();
    }

    // The arrays are in priority order, so find highest priority bit.
    for (int i = 0; s_thread_cmd_ltr[i]; i++)
    {
        if (autofl & s_thread_cmd_flag[i])
        {
            ch = s_thread_cmd_ltr[i];
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
    char* bp;
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
            while (std::fgets(g_buf, LINE_BUF_LEN, g_local_kfp) != nullptr)
            {
                bp = skip_space(g_buf);
                if (*bp == '/' || *bp == '*')
                {
                    g_kf_state |= KFS_NORMAL_LINES;
                }
                else if (*bp == '<')
                {
                    std::string_view msg_id{bp};
                    std::string_view cmd{","};
                    char *split = std::strchr(bp,' ');
                    if (split)
                    {
                        msg_id = {bp, static_cast<std::size_t>(split - bp)};
                        cmd = split + 1;
                    }
                    Article *ap = get_article(msg_id);
                    if (ap != nullptr)
                    {
                        std::string_view thread_cmd = cmd;
                        if (!thread_cmd.empty() && thread_cmd.front() == 'T')
                        {
                            thread_cmd.remove_prefix(1);
                        }
                        const char *thread_cmd_match =
                            thread_cmd.empty() ? nullptr : std::strchr(s_thread_cmd_ltr, thread_cmd.front());
                        if (thread_cmd_match != nullptr)
                        {
                            ap->m_auto_flags |= s_thread_cmd_flag[thread_cmd_match - s_thread_cmd_ltr];
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

void kill_file_append(const char *cmd, bool local)
{
    const fs::path kill_file{
        file_exp(local ? get_env_var("KILLLOCAL", s_kill_local) : get_env_var("KILLGLOBAL", s_kill_global))};
    if (!make_dir(kill_file.string().c_str(), MD_FILE))
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
