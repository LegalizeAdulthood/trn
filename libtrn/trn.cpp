// vi: set sw=4 ts=8 ai sm noet :

//  trn -- threaded readnews program based on rn 4.4
//
//  You can request help from:  trn-users@lists.sourceforge.net
//  Send bugs, suggestions, etc. to:  trn-workers@lists.sourceforge.net
//
//  Author/Maintainer of trn: trn@blorf.net (Wayne Davison)
//  Maintainer of rn: sob@bcm.tmc.edu (Stan Barber)
//  Original Author: lwall@sdcrdcf.UUCP (Larry Wall)
//  Copyright (c) 2026, Richard Thomson
//
//  History:
//      01/14/83 - rn begun
//      04/08/83 - rn 1.0
//      09/01/83 - rn 2.0
//      05/01/85 - rn 4.3
//      11/01/89 - rn/rrn integration
//      11/25/89 - trn begun
//      07/21/90 - trn 1.0
//      07/04/91 - rn 4.4
//      11/25/91 - trn 2.0
//      07/25/93 - trn 3.0
//      ??/??/?? - trn 4.0
//
//  strn -- Scan(-mode)/Scoring TRN
//
//  Author/Maintainer of strn: caadams@zynet.com (Clifford A. Adams)
//
//  Strn history:
//      Dec.  90  - "Keyword RN" initial ideas, keyword entry prototype
//      01/16/91  - "Scoring RN" initial design notes
//      Late  91  - cleaned up "Semicolon mode" RN patches from Todd Day
//      Early 92  - major additions to "STRN"
//      Mid   93  - first strn public release (version 0.8)
//      Sep.  94  - last beta release (version 0.9.3).
//      Late  95  - strn code ported to trn 4.0, universal selector started
//      May   96  - strn 1.0 release
//
//
// This software is copyrighted as detailed in the LICENSE file.

#include <trn/trn.h>

#include <config/common.h>
#include <nntp/nntpclient.h>
#include <trn/addng.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/help.h>
#include <trn/init.h>
#include <trn/kfile.h>
#include <trn/last.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngsrch.h>
#include <trn/ngstuff.h>
#include <trn/nntp.h>
#include <trn/only.h>
#include <trn/opt.h>
#include <trn/patchlevel.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/string-algos.h>
#include <trn/smisc.h>
#include <trn/sw.h>
#include <trn/terminal.h>
#include <trn/univ.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

std::string g_newsgroup_name;                             // name of current newsgroup
std::string g_newsgroup_dir;                              // same thing in directory name form
std::string g_patch_level{PATCHLEVEL};             //
int         g_find_last{};                         // -r
bool        g_verbose{true};                      // +t
bool        g_use_univ_selector{};                //
bool        g_use_newsrc_selector{};              //
bool        g_use_newsgroup_selector{true};       //
int         g_use_news_selector{SELECT_INIT - 1}; //

static bool s_restore_old_newsrc{};
static bool s_go_forward{true};

static std::string get_newsgroup_dir(std::string_view newsgroup_name);

void trn_init()
{
    g_newsgroup_dir.clear();
}

int trn_main(int argc, char *argv[])
{
#if !THREAD_INIT
    // Default to threaded operation if our name starts with a 't' or 's'.
    const auto is_threaded_name = [](const char *arg)
    {
        const std::string name{std::filesystem::path{arg}.filename().string()};
        return name[0] == 't' || name[0] == 's';
    };
    if (is_threaded_name(argv[0]))
    {
        g_use_threads = true;
    }
    else
    {
        g_use_news_selector = -1;
    }
#endif
    bool found_any = initialize(argc,argv);

    if (g_use_newsrc_selector)
    {
        multirc_selector();
        finalize(0);
    }

    if (find_new_groups())              // did we add any new groups?
    {
        found_any = true;
        g_start_here = nullptr;          // start ng scan from the top
    }

    if (g_max_newsgroup_to_do)
    {
        g_start_here = nullptr;
    }
    else if (!found_any)                 // nothing to do?
    {
        if (g_verbose)
        {
            std::fputs("No unread news in subscribed-to newsgroups.  To subscribe to a new\n"
                       "newsgroup use the g<newsgroup> command.\n",
                       stdout);
            term_down(2);
        }
        g_start_here = newsgroup_last();
    }

    do_multirc();

    finalize(0);
    // NOT REACHED
    return 0;
}

void do_multirc()
{
    bool special = false;       // allow newsgroup with no unread news?
    MinorMode mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;

    if (g_use_univ_selector)
    {
        univ_startup();         // load startup file
        char ch = universal_selector();
        if (ch != 'Q')
        {
            // section copied from bug_out below
            // now write the newsrc(s) back out
            if (!write_newsrcs(g_multirc))
            {
                s_restore_old_newsrc = true; // TODO: ask to retry!
            }
            if (s_restore_old_newsrc)
            {
                get_old_newsrcs(g_multirc);
            }
            finalize(0);
        }
    }

    if (g_use_newsgroup_selector)
    {
ng_start_sel:
        switch (newsgroup_selector())
        {
        case Ctl('n'):
            g_multirc->use_next_multirc();
            end_only();
            goto ng_start_sel;

        case Ctl('p'):
            g_multirc->use_prev_multirc();
            end_only();
            goto ng_start_sel;

        case 'q':
            goto bug_out;
        }
        g_start_here = g_newsgroup_ptr;
        g_use_newsgroup_selector = false;
    }

    // loop through all unread news
restart:
    g_current_newsgroup = newsgroup_first();
    while (true)
    {
        bool retry = false;
        if (g_find_last > 0)
        {
            g_find_last = -1;
            g_start_here = nullptr;
            if (!g_last_newsgroup_name.empty())
            {
                g_newsgroup_ptr = find_newsgroup(g_last_newsgroup_name);
                if (g_newsgroup_ptr == nullptr)
                {
                    g_newsgroup_ptr = newsgroup_first();
                }
                else
                {
                    set_newsgroup_name(g_last_newsgroup_name);
                    g_newsgroup_ptr->set_to_read(ST_LAX);
                    if (g_newsgroup_ptr->m_to_read <= TR_NONE)
                    {
                        g_newsgroup_ptr = newsgroup_first();
                    }
                }
            }
        }
        else if (g_start_here)
        {
            g_newsgroup_ptr = g_start_here;
            g_start_here = nullptr;
        }
        else
        {
            g_newsgroup_ptr = newsgroup_first();
        }
        while (true) // for each newsgroup
        {
            if (g_newsgroup_ptr == nullptr) // after the last newsgroup?
            {
                set_mode(GM_READ,MM_FINISH_NEWSGROUP_LIST);
                if (g_max_newsgroup_to_do)
                {
                    if (retry)
                    {
                        if (g_verbose)
                        {
                            std::printf("\nRestriction %s%s still in effect.\n",
                                   g_newsgroup_to_do[0].c_str(), g_max_newsgroup_to_do > 1 ? ", etc." : "");
                        }
                        else
                        {
                            std::fputs("\n(\"Only\" mode.)\n",stdout);
                        }
                        term_down(2);
                    }
                    else
                    {
                        if (g_verbose)
                        {
                            std::fputs("\nNo articles under restriction.", stdout);
                        }
                        else
                        {
                            std::fputs("\nNo \"only\" articles.",stdout);
                        }
                        term_down(2);
                        end_only();     // release the restriction
                        std::printf("\n%s\n", g_msg.c_str());
                        term_down(2);
                        retry = true;
                    }
                }
            }
            else
            {
                bool shoe_fits; // newsgroup matches restriction?

                set_mode(GM_READ,MM_NEWSGROUP_LIST);
                if (g_newsgroup_ptr->m_to_read >= TR_NONE) // recalc toread?
                {
                    set_newsgroup_name(g_newsgroup_ptr->rc_line());
                    shoe_fits = in_list(g_newsgroup_name.c_str());
                    if (shoe_fits)
                    {
                        g_newsgroup_ptr->set_to_read(ST_LAX);
                    }
                    if (g_paranoid)
                    {
                        g_recent_newsgroup = g_current_newsgroup;
                        g_current_newsgroup = g_newsgroup_ptr;
                        cleanup_newsrc(g_newsgroup_ptr->m_rc); // this may move newsgroups around
                        set_newsgroup(g_current_newsgroup);
                    }
                }
                else
                {
                    shoe_fits = true;
                }
                if (g_newsgroup_ptr->m_to_read < (special ? TR_NONE : g_newsgroup_min_to_read) //
                    || !shoe_fits)                                                             // unwanted newsgroup?
                {
                    if (s_go_forward)
                    {
                        g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr);
                    }
                    else
                    {
                        g_newsgroup_ptr = newsgroup_prev(g_newsgroup_ptr);
                        if (g_newsgroup_ptr == nullptr)
                        {
                            g_newsgroup_ptr = newsgroup_next(newsgroup_first());
                            s_go_forward = true;
                        }
                    }
                    continue;
                }
            }
            special = false;    // go back to normal mode
            if (g_newsgroup_ptr != g_current_newsgroup)
            {
                g_recent_newsgroup = g_current_newsgroup;     // remember previous newsgroup
                g_current_newsgroup = g_newsgroup_ptr; // remember current newsgroup
            }
reask_newsgroup:
            unflush_output();   // disable any ^O in effect
                if (g_newsgroup_ptr == nullptr)
                {
                g_default_cmd = retry ? "npq" : "qnp";
                if (g_verbose)
                {
                    std::printf("\n****** End of newsgroups -- what next? [%s] ",
                           g_default_cmd.c_str());
                }
                else
                {
                    std::printf("\n**** End -- next? [%s] ", g_default_cmd.c_str());
                }
                term_down(1);
            }
            else
            {
                g_threaded_group = (g_use_threads && !(g_newsgroup_ptr->m_flags&NF_UNTHREADED));
                g_default_cmd =
                    (g_use_news_selector >= 0 && g_newsgroup_ptr->m_to_read >= (ArticleUnread) g_use_news_selector ? "+ynq" : "ynq");
                if (g_verbose)
                {
                    std::printf("\n%s %3ld unread article%s in %s -- read now? [%s] ",
                           g_threaded_group? "======" : "******",
                           (long)g_newsgroup_ptr->m_to_read, plural(g_newsgroup_ptr->m_to_read),
                           g_newsgroup_name.c_str(), g_default_cmd.c_str());
                }
                else
                {
                    std::printf("\n%s %3ld in %s -- read? [%s] ",
                           g_threaded_group? "====" : "****",
                           (long)g_newsgroup_ptr->m_to_read,g_newsgroup_name.c_str(),g_default_cmd.c_str());
                }
                term_down(1);
            }
            std::fflush(stdout);
reinp_newsgroup:
            if (special || (g_newsgroup_ptr && g_newsgroup_ptr->m_to_read > 0))
            {
                retry = true;
            }
            switch (input_newsgroup())
            {
            case ING_ASK:
                goto reask_newsgroup;

            case ING_INPUT:
            case ING_ERASE:
                goto reinp_newsgroup;

            case ING_ERROR:
                std::fputs("\nType h for help.\n", stdout);
                term_down(2);
                settle_down();
                goto reask_newsgroup;

            case ING_QUIT:
                goto bug_out;

            case ING_BREAK:
                goto loop_break;

            case ING_RESTART:
                goto restart;

            case ING_NO_SERVER:
                if (g_multirc)
                {
                    goto restart;
                }
                goto bug_out;

            case ING_SPECIAL:
                special = true;
                break;

            case ING_NORM:
                break;

            case ING_DISPLAY:
                newline();
                break;

            case ING_MESSAGE:
                std::printf("\n%s\n", g_msg.c_str());
                term_down(2);
                break;
            }
        }
    loop_break:;
        check_active_refetch(false);
        }

bug_out:
    // now write the newsrc(s) back out
    if (!write_newsrcs(g_multirc))
    {
        s_restore_old_newsrc = true; // TODO: ask to retry!
    }
    if (s_restore_old_newsrc)
    {
        get_old_newsrcs(g_multirc);
    }

    set_mode(gmode_save,mode_save);
}

static char newsgroup_command_char(std::string_view command)
{
    return command.empty() ? '\0' : command.front();
}

static bool is_default_newsgroup_command(char ch)
{
    if (ch == ' ')
    {
        return true;
    }
#ifndef STRICT_CR
    if (ch == '\n' || ch == '\r')
    {
        return true;
    }
#endif
    return false;
}

static std::string apply_newsgroup_default_command(std::string command)
{
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    if (is_default_newsgroup_command(newsgroup_command_char(command)))
    {
        g_s_default_cmd = true;
        g_univ_default_cmd = true;
        if (g_default_cmd.size() > 1 && g_default_cmd.front() == '^' &&
            std::isupper(static_cast<unsigned char>(g_default_cmd[1])))
        {
            push_char(Ctl(g_default_cmd[1]));
        }
        else
        {
            push_char(g_default_cmd.empty() ? '\0' : g_default_cmd.front());
        }
        command = get_cmd();
    }
    return command;
}

static void set_newsgroup_command_char(std::string &command, char ch)
{
    if (command.empty())
    {
        command += ch;
    }
    else
    {
        command[0] = ch;
    }
}

static void stage_legacy_newsgroup_command(std::string_view command)
{
    const std::size_t command_size = std::min(command.size(), static_cast<std::size_t>(LINE_BUF_LEN));
    std::copy_n(command.data(), command_size, g_buf);
    g_buf[command_size] = '\0';
}

static std::string finish_newsgroup_command(std::string_view command, bool donewline)
{
    if (command.size() <= 1 || command[1] != FINISH_CMD)
    {
        return std::string{command};
    }
    return finish_command(command.substr(0, 1), donewline);
}

static std::string command_with_typeahead(char ch)
{
    std::string command(LINE_BUF_LEN + 1, '\0');
    command[0] = ch;
    save_typeahead(command.data() + 1, LINE_BUF_LEN);
    const std::size_t end = command.find('\0');
    command.resize(end == std::string::npos ? command.size() : end);
    return command;
}

static std::string_view command_argument(std::string_view command)
{
    if (command.empty())
    {
        return {};
    }
    command.remove_prefix(1);
    return command;
}

static std::string_view trim_leading_spaces(std::string_view text)
{
    const std::size_t start = text.find_first_not_of(' ');
    text.remove_prefix(start == std::string_view::npos ? text.size() : start);
    return text;
}

InputNewsgroupResult input_newsgroup()
{
    std::optional<std::string> start_command;
    char                       command_ch;

    eat_typeahead();
    std::string command = get_cmd();
    if (errno || newsgroup_command_char(command) == '\f')
    {
        newline();              // if return from stop signal
        return ING_ASK;
    }
    const char original_command_ch = newsgroup_command_char(command);
    command = apply_newsgroup_default_command(command);
    stage_legacy_newsgroup_command(command);
    print_cmd();
    if (g_newsgroup_ptr != nullptr)
    {
        set_newsgroup_command_char(command, original_command_ch);
        stage_legacy_newsgroup_command(command);
    }

do_command:
    command_ch = newsgroup_command_char(command);
    s_go_forward = true;                // default to forward motion
    switch (command_ch)
    {
    case 'P':                           // goto previous newsgroup
    case 'p':                           // find previous unread newsgroup
        if (!g_newsgroup_ptr)
        {
            g_newsgroup_ptr = newsgroup_last();
        }
        else if (g_newsgroup_ptr != newsgroup_first())
        {
            g_newsgroup_ptr = newsgroup_prev(g_newsgroup_ptr);
        }
        s_go_forward = false;           // go backward in the newsrc
        if (command_ch == 'P')
        {
            return ING_SPECIAL;
        }
        break;

    case '-':
        g_newsgroup_ptr = g_recent_newsgroup;          // recall previous newsgroup
        if (g_newsgroup_ptr)
        {
            if (!get_newsgroup(g_newsgroup_ptr->rc_line(), GNG_NONE))
            {
                set_newsgroup(g_current_newsgroup);
            }
            g_add_new_by_default = ADDNEW_ASK;
        }
        return ING_SPECIAL;

    case 'x':
        newline();
        in_char("Confirm: exit and abandon newsrc changes?", MM_ADD_NEWSGROUP_PROMPT, "yn");
        newline();
        if (*g_buf != 'y')
        {
            break;
        }
        std::printf("\nThe abandoned changes are in %s.new.\n",
               g_multirc->multirc_name().c_str());
        term_down(2);
        s_restore_old_newsrc = true;
        return ING_QUIT;

    case 'q': case 'Q':       // quit?
        newline();
        return ING_QUIT;

    case '^':
        if (g_general_mode != GM_SELECTOR)
        {
            newline();
        }
        g_newsgroup_ptr = newsgroup_first();
        break;

    case 'N': // goto next newsgroup
    case 'n': // find next unread newsgroup
        if (g_newsgroup_ptr == nullptr)
        {
            newline();
            return ING_BREAK;
        }
        g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr);
        if (command_ch == 'N')
        {
            return ING_SPECIAL;
        }
        break;

    case '1': // goto 1st newsgroup
        g_newsgroup_ptr = newsgroup_first();
        return ING_SPECIAL;

    case '$':
        g_newsgroup_ptr = nullptr;              // go past last newsgroup
        break;

    case 'L':
        list_newsgroups();
        return ING_ASK;

    case '/': case '?':       // scan for newsgroup pattern
        command = finish_newsgroup_command(command, false);
        if (command.empty())
        {
            set_newsgroup(g_current_newsgroup);
            return ING_INPUT;
        }
        switch (newsgroup_search(command, false))
        {
        case NGS_ERROR:
            set_newsgroup(g_current_newsgroup);
            return ING_ASK;

        case NGS_ABORT:
            set_newsgroup(g_current_newsgroup);
            return ING_INPUT;

        case NGS_INTR:
            if (g_verbose)
            {
                std::fputs("\n(Interrupted)\n",stdout);
            }
            else
            {
                std::fputs("\n(Intr)\n",stdout);
            }
            term_down(2);
            set_newsgroup(g_current_newsgroup);
            return ING_ASK;

        case NGS_FOUND:
            return ING_SPECIAL;

        case NGS_NOT_FOUND:
            if (g_verbose)
            {
                std::fputs("\n\nNot found -- use a or g to add newsgroups\n",
                      stdout);
            }
            else
            {
                std::fputs("\n\nNot found\n",stdout);
            }
            term_down(3);
            return ING_ASK;

        case NGS_DONE:
            return ING_ASK;
        }
        break;

    case 'm':
        not_incl("m");
        break;

    case 'g': // goto named newsgroup
        command = finish_newsgroup_command(command, false);
        if (command.empty())
        {
            return ING_INPUT;
        }
        {
            std::string_view target = trim_leading_spaces(command_argument(command));
            if (target.empty() && command_ch == 'm' && !g_newsgroup_name.empty() && g_newsgroup_ptr)
            {
                target = g_newsgroup_name;
            }
            if (target.find_first_not_of("0123456789") != std::string_view::npos)
            {
                // found non-digit before hitting end
                set_newsgroup_name(target);
            }
            else
            {
                int rcnum{};
                std::from_chars(target.data(), target.data() + target.size(), rcnum);
                for (g_newsgroup_ptr = newsgroup_first(); g_newsgroup_ptr;
                     g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr))
                {
                    if (g_newsgroup_ptr->m_num.value_of() == rcnum)
                    {
                        break;
                    }
                }
                if (!g_newsgroup_ptr)
                {
                    g_newsgroup_ptr = g_current_newsgroup;
                    std::printf("\nOnly %d groups. Try again.\n", g_newsgroup_count.value_of());
                    term_down(2);
                    return ING_ASK;
                }
                set_newsgroup_name(g_newsgroup_ptr->rc_line());
            }
        }
        // try to find newsgroup
        if (!get_newsgroup(g_newsgroup_name, (command_ch == 'm' ? GNG_RELOC : GNG_NONE) | GNG_FUZZY))
        {
            g_newsgroup_ptr = g_current_newsgroup;     // if not found, go nowhere
        }
        g_add_new_by_default = ADDNEW_ASK;
        return ING_SPECIAL;

#ifdef DEBUG
    case 'D':
        return ING_ASK;
#endif

    case '!':                 // shell escape
        stage_legacy_newsgroup_command(command);
        if (escapade())         // do command
        {
            return ING_INPUT;
        }
        return ING_ASK;

    case Ctl('k'):            // edit global KILL file
        edit_kill_file();
        return ING_ASK;

    case Ctl('n'):            // next newsrc list
        end_only();
        newline();
        g_multirc->use_next_multirc();
        goto display_multirc;

    case Ctl('p'):            // prev newsrc list
        end_only();
        newline();
        g_multirc->use_next_multirc();
display_multirc:
        {
            Newsrc* rp;
            std::string news_sources;
            news_sources.reserve(66);
            std::size_t len = 0;
            for (rp = g_multirc->m_first; rp && len < 66; rp = rp->next)
            {
                if (rp->flags & RF_ACTIVE)
                {
                    fmt::format_to(std::back_inserter(news_sources), "{}{}", news_sources.empty() ? "" : ", ",
                                   rp->data_source->m_name);
                    len = news_sources.size() + 2;
                }
            }
            if (rp)
            {
                news_sources += ", ...";
            }
            fmt::print("\nUsing newsrc group #{}: {}.\n", g_multirc->m_num, news_sources);
            term_down(3);
            return ING_RESTART;
        }

    case 'c':                 // catch up
        if (g_newsgroup_ptr)
        {
            ask_catchup();
            if (g_newsgroup_ptr->m_to_read == TR_NONE)
            {
                g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr);
            }
        }
        break;

    case 't':
        if (!g_use_threads)
        {
            std::printf("\n\nNot running in thread mode.\n");
        }
        else if (g_newsgroup_ptr && g_newsgroup_ptr->m_to_read >= TR_NONE)
        {
            bool read_unthreaded = !(g_newsgroup_ptr->m_flags & NF_UNTHREADED);
            g_newsgroup_ptr->m_flags ^= NF_UNTHREADED;
            fmt::print("\n\n{} will be read {}threaded.\n", g_newsgroup_ptr->rc_line(), read_unthreaded ? "un" : "");
            g_newsgroup_ptr->set_to_read(ST_LAX);
        }
        term_down(3);
        return ING_SPECIAL;

    case 'u':                                                         // unsubscribe
        if (g_newsgroup_ptr && g_newsgroup_ptr->m_to_read >= TR_NONE) // unsubscribable?
        {
            newline();
            fmt::print("Unsubscribed to newsgroup {}\n", g_newsgroup_ptr->rc_name());
            term_down(1);
            g_newsgroup_ptr->m_subscribe_char = UNSUBSCRIBED_CHAR; // unsubscribe it
            g_newsgroup_ptr->m_to_read = TR_UNSUB;                 // and make line invisible
            g_newsgroup_ptr->m_rc->flags |= RF_RC_CHANGED;
            g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr); // do an automatic 'n'
            --g_newsgroup_to_read;
        }
        break;

    case 'h':
        univ_help(UHELP_NG);
        return ING_ASK;

    case 'H':                 // help
        help_newsgroup();
        return ING_ASK;

    case 'A':
        if (!g_newsgroup_ptr)
        {
            break;
        }
reask_abandon:
        if (g_verbose)
        {
            in_char("\nAbandon changes to current newsgroup?", MM_CONFIRM_ABANDON_PROMPT, "yn");
        }
        else
        {
            in_char("\nAbandon?", MM_CONFIRM_ABANDON_PROMPT, "ynh");
        }
        print_cmd();
        newline();
        if (*g_buf == 'h')
        {
            std::printf("Type y or SP to abandon the changes to this group since you started trn.\n");
            std::printf("Type n to leave the group as it is.\n");
            term_down(2);
            goto reask_abandon;
        }
        else if (*g_buf != 'y' && *g_buf != 'n' && *g_buf != 'q')
        {
            std::fputs("Type h for help.\n", stdout);
            term_down(1);
            settle_down();
            goto reask_abandon;
        }
        else if (*g_buf == 'y')
        {
            g_newsgroup_ptr->abandon_newsgroup();
        }
        return ING_SPECIAL;

    case 'a':
        // FALL THROUGH

    case 'o':
    case 'O':
    {
        bool doscan = (command_ch == 'a');
        command = finish_newsgroup_command(command, true);
        if (command.empty()) // get rest of command
        {
            return ING_INPUT;
        }
        g_msg.clear();
        end_only();
        std::string_view switches = command_argument(command);
        if (!switches.empty())
        {
            bool minusd = in_string(switches, "-d", true);
            sw_list(switches);
            if (minusd)
            {
                cwd_check();
            }
            if (doscan && g_max_newsgroup_to_do)
            {
                scan_active(true);
            }
            g_newsgroup_min_to_read = command_ch == g_empty_only_char && g_max_newsgroup_to_do ? TR_NONE : TR_ONE;
        }
        g_newsgroup_ptr = newsgroup_first(); // simulate ^
        if (!g_msg.empty() && !g_max_newsgroup_to_do)
        {
            return ING_MESSAGE;
        }
        return ING_DISPLAY;
    }

    case '&':
        stage_legacy_newsgroup_command(command);
        if (switcheroo())       // get rest of command
        {
            return ING_INPUT;   // if rubbed out, try something else
        }
        return ING_ASK;

    case 'l':                 // list other newsgroups
    {
        command = finish_newsgroup_command(command, true);
        if (command.empty()) // get rest of command
        {
            return ING_INPUT;   // if rubbed out, try something else
        }
        std::string_view switches = trim_leading_spaces(command_argument(command));
        push_only();
        if (!switches.empty())
        {
            sw_list(switches);
        }
        page_start();
        scan_active(false);
        pop_only();
        return ING_ASK;
    }

    case '`':
    case '\\':
        if (g_general_mode == GM_SELECTOR)
        {
            return ING_ERASE;
        }
ng_start_sel:
        g_use_newsgroup_selector = true;
        switch (newsgroup_selector())
        {
        case Ctl('n'):
            end_only();
            g_multirc->use_next_multirc();
            goto ng_start_sel;

        case Ctl('p'):
            end_only();
            g_multirc->use_prev_multirc();
            goto ng_start_sel;

        case 'q':
             return ING_QUIT;
        }
        g_use_newsgroup_selector = false;
        return ING_ASK;

    case ';':
    case 'U': case '+':
    case '.': case '=':
    case 'y': case 'Y': case '\t': // do normal thing
    case ' ': case '\r': case '\n':
        if (!g_newsgroup_ptr)
        {
            std::fputs("\nNot on a newsgroup.",stdout);
            term_down(1);
            return ING_ASK;
        }
        // *once*, the char* s was set to an illegal value
        // (it seemed to miss all the if statements below)
        // Just to be safe, make sure it is legal.
        //
        start_command = std::string{};
        if (command_ch == '.') // start command?
        {
            command = finish_newsgroup_command(command, false);
            if (command.empty()) // get rest of command
            {
                return ING_INPUT;
            }
            start_command = command_argument(command);
        }
        else if (command_ch == '+' || command_ch == 'U' || command_ch == '=' || command_ch == ';')
        {
            start_command = command_with_typeahead(static_cast<char>(g_last_char));
        }
        else if (command_ch == ' ' || command_ch == '\r' || command_ch == '\n')
        {
            start_command = std::string{};
        }
        else
        {
            start_command.reset();
        }

redo_newsgroup:
        switch (do_newsgroup(start_command))
        {
        case NG_NORM:
        case NG_NEXT:
        case NG_ERROR:
            g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr);
            break;

        case NG_ASK:
            return ING_ASK;

        case NG_SEL_PRIOR:
            command = "p";
            goto do_command;

        case NG_SEL_NEXT:
            command = "n";
            goto do_command;

        case NG_MINUS:
            g_newsgroup_ptr = g_recent_newsgroup; // recall previous newsgroup
            return ING_SPECIAL;

        case NG_NO_SERVER:
            g_newsgroup_ptr->m_rc->data_source->nntp_server_died();
            return ING_NO_SERVER;

        // extensions
        case NG_GO_ARTICLE:
            g_newsgroup_ptr = g_ng_go_newsgroup_ptr;
            start_command = "y"; // enter with minimal fuss
            goto redo_newsgroup;
          // later: possible go-to-newsgroup
        }
        break;

    case ':':         // execute command on selected groups
    {
        const std::string full_command = finish_newsgroup_command(command, true);
        if (full_command.empty() || !newsgroup_sel_perform(full_command))
        {
            return ING_INPUT;
        }
        g_page_line = 1;
        newline();
        set_newsgroup(g_current_newsgroup);
        return ING_ASK;
    }

    case 'v':
        newline();
        trn_version();
        return ING_ASK;

    default:
        if (command_ch == g_erase_char || command_ch == g_kill_char)
        {
            return ING_ERASE;
        }
        return ING_ERROR;
    }
    return ING_NORM;
}

void check_active_refetch(bool force)
{
    std::time_t now = std::time(nullptr);

    for (DataSource *dp = data_source_first(); dp; dp = data_source_next(dp))
    {
        if (!all_bits(dp->m_flags, DF_OPEN | DF_ACTIVE))
        {
            continue;
        }
        if (dp->m_act_sf.m_fp && dp->m_act_sf.m_refetch_secs &&
            (force || now - dp->m_act_sf.m_last_fetch > dp->m_act_sf.m_refetch_secs))
        {
            dp->active_file_hash();
        }
    }
}

void trn_version()
{
    page_start();
    print_lines(fmt::format("Trn version: {}.\nConfigured for "
#ifdef HAS_LOCAL_SPOOL
                            "both NNTP and local news access.\n",
#else
                            "NNTP (plus individual local access).\n",
#endif
                            g_patch_level),
                NO_MARKING);

    if (g_multirc)
    {
        newline();
        print_lines(fmt::format("News source group #{}:\n\n", g_multirc->m_num), NO_MARKING);
        for (Newsrc *rp = g_multirc->m_first; rp; rp = rp->next)
        {
            if (!(rp->flags & RF_ACTIVE))
            {
                continue;
            }
            const DataSource &data_source = *rp->data_source;
            print_lines(fmt::format("ID {}:\nNewsrc {}.\n", data_source.m_name, rp->name.generic_string()), NO_MARKING);
            if (data_source.m_flags & DF_REMOTE)
            {
                print_lines(fmt::format("News from server {}.\n", data_source.m_news_id), NO_MARKING);
                std::string active_file;
                active_file.reserve(CMD_BUF_LEN);
                if (data_source.m_act_sf.m_fp)
                {
                    if (data_source.m_flags & DF_TMP_ACTIVE_FILE)
                    {
                        active_file = "Copy of remote active file";
                    }
                    else
                    {
                        fmt::format_to(std::back_inserter(active_file), "Local active file: {}",
                                       data_source.m_extra_name);
                    }
                }
                else
                {
                    active_file = "Dynamic active file";
                }
                if (data_source.m_act_sf.m_refetch_secs)
                {
                    const std::string refetch_text = secs_to_text(data_source.m_act_sf.m_refetch_secs);
                    if (refetch_text != "never")
                    {
                        fmt::format_to(std::back_inserter(active_file), " (refetch{} {})",
                                       refetch_text == "missing" ? " if" : ":", refetch_text);
                    }
                }
                active_file += ".\n";
                print_lines(active_file, NO_MARKING);
            }
            else
            {
                print_lines(fmt::format("News from {}.\nLocal active file {}.\n", data_source.m_spool_dir,
                                        data_source.m_news_id),
                            NO_MARKING);
            }
            if (!data_source.m_group_desc.empty())
            {
                std::string group_desc;
                group_desc.reserve(CMD_BUF_LEN);
                if (!data_source.m_desc_sf.m_fp && data_source.m_desc_sf.m_hp)
                {
                    group_desc = "Dynamic group desc. file";
                }
                else if (data_source.m_flags & DF_TMP_GROUP_DESC)
                {
                    group_desc = "Copy of remote group desc. file";
                }
                else
                {
                    fmt::format_to(std::back_inserter(group_desc), "Group desc. file: {}", data_source.m_group_desc);
                }
                if (data_source.m_desc_sf.m_refetch_secs)
                {
                    const std::string refetch_text = secs_to_text(data_source.m_desc_sf.m_refetch_secs);
                    if (refetch_text != "never")
                    {
                        fmt::format_to(std::back_inserter(group_desc), " (refetch{} {})",
                                       refetch_text == "missing" ? " if" : ":", refetch_text);
                    }
                }
                group_desc += ".\n";
                print_lines(group_desc, NO_MARKING);
            }
            if (data_source.m_flags & DF_TRY_OVERVIEW)
            {
                print_lines(fmt::format("Overview files from {}.\n",
                                        data_source.m_over_dir.empty() ? "the server" : data_source.m_over_dir),
                            NO_MARKING);
            }
            print_lines("\n", NO_MARKING);
        }
    }

    print_lines("You can request help from:  trn-users@lists.sourceforge.net\n"
                "Send bug reports, suggestions, etc. to:  trn-workers@lists.sourceforge.net\n",
                NO_MARKING);
}

void set_newsgroup_name(std::string_view what)
{
    if (what.empty())
    {
        g_newsgroup_name.clear();
    }
    else if (std::string_view{g_newsgroup_name} != what)
    {
        g_newsgroup_name.assign(what);
    }

    g_newsgroup_dir = get_newsgroup_dir(g_newsgroup_name);
}

static std::string get_newsgroup_dir(std::string_view newsgroup_name)
{
    std::string dir{newsgroup_name};
    for (char &c : dir)
    {
        if (c == '.')
        {
            c = '/';
        }
    }
    return dir;
}
