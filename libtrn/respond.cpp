/* respond.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/respond.h>

#include <config/common.h>
#include <config/env.h>
#include <config/string_case_compare.h>
#include <config/user_id.h>
#include <nntp/nntpclient.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/change_dir.h>
#include <trn/charsubst.h>
#include <trn/datasrc.h>
#include <trn/decode.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/intrp.h>
#include <trn/mime.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/respond-internal.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <trn/uudecode.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

std::string     g_save_dest;          // value of %b
std::string     g_extract_dest;       // value of %E
std::string     g_extract_prog;       // value of %e
ArticlePosition g_save_from{};        // value of %B
bool            g_mbox_always{};      // -M
bool            g_norm_always{};      // -N
std::string     g_priv_dir;           // private news directory
std::string     g_indent_string{">"}; // indent for old article embedded in followup

static constexpr char s_empty_article[] = "\nEmpty article.\n";
static std::FILE *s_tmp_fp{};

static void follow_it_up();
static int  invoke(const char *cmd, const char *dir);

static char response_command_char(std::string_view command)
{
    return command.empty() ? '\0' : command.front();
}

static bool extract_option_digit(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

static std::string_view skip_response_spaces(std::string_view text)
{
    const std::size_t non_space = text.find_first_not_of(' ');
    if (non_space == std::string_view::npos)
    {
        return {};
    }
    return text.substr(non_space);
}

static std::string_view skip_response_whitespace(std::string_view text)
{
    const std::string_view::const_iterator first = std::find_if_not(
        text.begin(), text.end(), [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
    text.remove_prefix(static_cast<std::size_t>(first - text.begin()));
    return text;
}

static std::string_view parse_extract_number(std::string_view text, int &value)
{
    const char            *first = text.data();
    std::from_chars_result result = std::from_chars(first, first + text.size(), value);
    return text.substr(static_cast<std::string_view::size_type>(result.ptr - first));
}

static std::string_view parse_extract_options(std::string_view command_text, int &part_opt, int &total_opt)
{
    std::string_view destination = skip_response_spaces(command_text);
    if (destination.size() >= 2 && destination.front() == '-' && extract_option_digit(destination[1]))
    {
        destination.remove_prefix(1);
        destination = parse_extract_number(destination, part_opt);
        if (!destination.empty() && destination.front() == '/')
        {
            destination.remove_prefix(1);
            total_opt = 0;
            destination = parse_extract_number(destination, total_opt);
            destination = skip_response_spaces(destination);
        }
        else
        {
            total_opt = part_opt;
        }
    }
    return destination;
}

std::string_view respond_parse_extract_options_for_test(std::string_view command_text, int &part_opt, int &total_opt)
{
    return parse_extract_options(command_text, part_opt, total_opt);
}

void respond_init()
{
    g_save_dest.clear();
    g_extract_dest.clear();
}

SaveResult save_article(std::string_view command)
{
    std::string completed_command;
    bool        interactive = command.size() > 1 && command[1] == FINISH_CMD;
    char        cmd = response_command_char(command);

    if (interactive)
    {
        completed_command = finish_command(command.substr(0, 1), true);
        if (completed_command.empty())
        {
            return SAVE_ABORT;
        }
        command = completed_command;
    }
    std::string_view command_text = command.size() > 1 ? command.substr(1) : std::string_view{};
    bool             use_pref = std::isupper(static_cast<unsigned char>(cmd)) != 0;
    if (use_pref != 0)
    {
        cmd = static_cast<char>(std::tolower(static_cast<unsigned char>(cmd)));
    }
    parse_header(g_art);
    mime_set_article();
    clear_art_buf();
    g_save_from = (cmd == 'w' || cmd == 'e') ? g_header_type[PAST_HEADER].min_pos : ArticlePosition{};
    if (art_open(g_art, g_save_from) == nullptr)
    {
        if (g_verbose)
        {
            fmt::print("\nCan't save an empty article.\n");
        }
        else
        {
            fmt::print("{}", s_empty_article);
        }
        term_down(2);
        return SAVE_DONE;
    }
    if (change_dir(g_priv_dir))
    {
        fmt::print("Can't chdir to directory {}\n", g_priv_dir);
        sig_catcher(0);
    }
    if (cmd == 'e')             // is this an extract command?
    {
        static bool custom_extract = false;
        int         partOpt = 0;
        int         totalOpt = 0;

        std::string destination = file_exp(parse_extract_options(command_text, partOpt, totalOpt));
        destination.reserve(CMD_BUF_LEN);
        bool has_extract_command = custom_extract;
        if (!destination.empty())
        {
            std::size_t command_separator = destination.find('|');
            while (command_separator != std::string::npos && command_separator > 0 &&
                   destination[command_separator - 1] == '\\')
            {
                destination.erase(command_separator - 1, 1);
                command_separator = destination.find('|', command_separator);
            }
            if (command_separator != std::string::npos)
            {
                std::size_t command_start = command_separator + 1;
                while (command_start < destination.size() && destination[command_start] == ' ')
                {
                    ++command_start;
                }
                if (command_start < destination.size())
                {
                    g_extract_prog.assign(destination, command_start, std::string::npos);
                }
                destination.erase(command_separator);
            }
            while (!destination.empty() && destination.back() == ' ')
            {
                destination.pop_back();
            }
            has_extract_command = command_separator != std::string::npos;
        }
        else
        {
            destination = g_extract_dest;
        }
        custom_extract = has_extract_command;

        if (!file_ref(destination)) // relative path?
        {
            std::string save_directory = file_exp(get_env_var("SAVEDIR", SAVEDIR));
            save_directory.reserve(LINE_BUF_LEN);
            if (make_dir(save_directory, MD_DIR)) // ensure directory exists
            {
                save_directory = g_priv_dir;
            }
            if (!destination.empty())
            {
                destination = (fs::path{save_directory} / destination).generic_string();
            }
            else
            {
                destination = save_directory;
            }
        }
        if (!file_ref(destination)) // path still relative?
        {
            destination = (fs::path{g_priv_dir} / destination).generic_string();
        }
        g_extract_dest = destination;                 // make it handy for %E
        if (make_dir(g_extract_dest, MD_DIR)) // ensure directory exists
        {
            g_int_count++;
            return SAVE_DONE;
        }
        if (change_dir(g_extract_dest))
        {
            fmt::print("Can't chdir to directory {}\n", g_extract_dest);
            sig_catcher(0);
        }
        if (custom_extract)
        {
            fmt::print("Extracting article into {} using {}\n", fs::current_path().generic_string(), g_extract_prog);
            term_down(1);
            const std::string saver_command = do_interp(get_env_var("CUSTOMSAVER", CUSTOM_SAVER));
            invoke(saver_command.c_str(), nullptr);
        }
        else if (g_is_mime)
        {
            fmt::print("Extracting MIME article into {}:\n", fs::current_path().generic_string());
            term_down(1);
            mime_decode_article(false);
        }
        else
        {
            int part;
            int total;
            int decode_type = 0;
            int cnt = 0;

            // Scan subject for filename and part number information
            std::string filename = decode_subject(g_art, &part, &total);
            if (partOpt)
            {
                part = partOpt;
            }
            if (totalOpt)
            {
                total = totalOpt;
            }
            std::string article_line;
            article_line.reserve(LINE_BUF_LEN);
            for (g_art_pos = g_save_from; read_art(article_line); g_art_pos = tell_art())
            {
                const std::string_view line_text{article_line};
                if (line_text.empty() || line_text.front() <= ' ')
                {
                    continue; // Ignore empty or initially-whitespace lines
                }
                if ((line_text.front() == '#' || line_text.front() == ':')                  //
                    && (line_text.substr(1, 9) == "! /bin/sh"                               //
                        || line_text.substr(1, 8) == "!/bin/sh"                             //
                        || (line_text.size() > 1 && line_text.substr(2, 8) == "This is "))) //
                {
                    g_save_from = g_art_pos;
                    decode_type = 1;
                    break;
                }
                if (uue_prescan(line_text, filename, &part, &total))
                {
                    g_save_from = g_art_pos;
                    seek_art(g_save_from);
                    decode_type = 2;
                    break;
                }
                if (++cnt == 300)
                {
                    break;
                }
            } // for
            switch (decode_type)
            {
            case 1:
                fmt::print("Extracting shar into {}:\n", fs::current_path().generic_string());
                term_down(1);
                {
                    const std::string saver_command = do_interp(get_env_var("SHARSAVER", SHAR_SAVER));
                    invoke(saver_command.c_str(), nullptr);
                }
                break;

            case 2:
                fmt::print("Extracting uuencoded file into {}:\n", fs::current_path().generic_string());
                term_down(1);
                g_mime_section->m_type = IMAGE_MIME;
                if (!filename.empty())
                {
                    g_mime_section->m_filename = filename;
                }
                else
                {
                    g_mime_section->m_filename.reset();
                }
                g_mime_section->m_encoding = MENCODE_UUE;
                g_mime_section->m_part = part;
                g_mime_section->m_total = total;
                if (!decode_piece(nullptr, {}) && !g_msg.empty())
                {
                    newline();
                    fmt::print("{}", g_msg);
                }
                newline();
                break;

            default:
                fmt::print("Unable to determine type of file.\n");
                term_down(1);
                break;
            }
        } // if
    }
    else if (const std::string_view::size_type pipe_separator = command.find('|');
             pipe_separator != std::string_view::npos) // is it a pipe command?
    {
        g_save_dest = file_exp(skip_response_spaces(command.substr(pipe_separator + 1)));
        if (g_data_source->m_flags & DF_REMOTE)
        {
            nntp_finish_body(FB_SILENT);
        }
        const std::string saver_command = do_interp(get_env_var("PIPESAVER", PIPE_SAVER));
        // then set up for command
        termlib_reset();
        reset_tty();              // restore tty state
        if (use_pref)           // use preferred shell?
        {
            do_shell({}, saver_command);
                                // do command with it
        }
        else
        {
            do_shell(SH, saver_command); // do command with sh
        }
        no_echo();               // and stop echoing
        cr_mode();               // and start cbreaking
        termlib_init();
    }
    else                        // normal save
    {
        bool  there;
        bool  mailbox;
        const std::string savename = get_env_var("SAVENAME", SAVENAME);

        if (!command_text.empty() && command_text.front() == '-') // if they are confused, skip - also
        {
            if (g_verbose)
            {
                fmt::print("Warning: '-' ignored.  This isn't readnews.\n");
            }
            else
            {
                fmt::print("'-' ignored.\n");
            }
            term_down(1);
            command_text.remove_prefix(1);
        }
        std::string destination = file_exp(skip_response_spaces(command_text));
        if (!file_ref(destination))
        {
            std::string save_directory = file_exp(get_env_var("SAVEDIR", SAVEDIR));
            if (make_dir(save_directory, MD_DIR)) // ensure directory exists
            {
                save_directory = g_priv_dir;
            }
            if (!destination.empty())
            {
                destination = (fs::path{save_directory} / destination).generic_string();
            }
            else
            {
                destination = save_directory;
            }
        }
        stat_t save_dir_stat{};
        for (int i = 0; (there = stat(destination.c_str(), &save_dir_stat) >= 0) && S_ISDIR(save_dir_stat.st_mode);
             i++) // is it a directory?
        {
            destination = (fs::path{destination} / file_exp(i ? "News" : savename)).generic_string();
        }
        make_dir(destination, MD_FILE);
        if (!file_ref(destination)) // relative path?
        {
            destination = (fs::path{g_priv_dir} / destination).generic_string();
        }
        g_save_dest = destination; // make it handy for %b
        s_tmp_fp = nullptr;
        if (!there)
        {
            if (g_mbox_always)
            {
                mailbox = true;
            }
            else if (g_norm_always)
            {
                mailbox = false;
            }
            else
            {
                const std::string_view dflt = (in_string(savename, "%a", true) ? "nyq" : "ynq");

reask_save:
                const std::string command =
                    in_char(fmt::format("\nFile {} doesn't exist--\n        use mailbox format?", destination),
                            MM_USE_MAILBOX_FORMAT_PROMPT, dflt);
                newline();
                print_cmd(command);
                if (*g_buf == 'h')
                {
                    if (g_verbose)
                    {
                        fmt::print("\n"
                                   "Type y to create {} as a mailbox.\n"
                                   "Type n to create it as a normal file.\n"
                                   "Type q to abort the save.\n",
                                   destination);
                    }
                    else
                    {
                        fmt::print("\n"
                                   "y to create mailbox.\n"
                                   "n to create normal file.\n"
                                   "q to abort.\n");
                    }
                    term_down(4);
                    goto reask_save;
                }
                else if (*g_buf == 'n')
                {
                    mailbox = false;
                }
                else if (*g_buf == 'y')
                {
                    mailbox = true;
                }
                else if (*g_buf == 'q')
                {
                    goto s_bomb;
                }
                else
                {
                    fmt::print("Type h for help.\n");
                    term_down(1);
                    settle_down();
                    goto reask_save;
                }
            }
        }
        else if (S_ISCHR(save_dir_stat.st_mode))
        {
            mailbox = false;
        }
        else
        {
            s_tmp_fp = std::fopen(destination.c_str(), "r+");
            if (!s_tmp_fp)
            {
                mailbox = false;
            }
            else
            {
                std::string first_line(LINE_BUF_LEN, '\0');
                const std::size_t bytes_read = std::fread(first_line.data(), 1, first_line.size(), s_tmp_fp);
                if (bytes_read != 0)
                {
                    first_line.resize(bytes_read);
                    std::string_view first = first_line;
                    if (!std::isspace(static_cast<unsigned char>(MBOX_CHAR))) // if non-zero,
                    {
                        first = skip_response_whitespace(first); // check the first character
                    }
                    mailbox = (!first.empty() && first.front() == MBOX_CHAR);
                }
                else
                {
                    mailbox = g_mbox_always;    // if zero length, recheck -M
                }
            }
        }

        const std::string saver = get_env_var(mailbox ? "MBOXSAVER" : "NORMSAVER");
        int i;
        if (!saver.empty())
        {
            if (s_tmp_fp)
            {
                std::fclose(s_tmp_fp);
            }
            if (g_data_source->m_flags & DF_REMOTE)
            {
                nntp_finish_body(FB_SILENT);
            }
            termlib_reset();
            reset_tty();          // make terminal behave
            i = do_shell(use_pref ? "" : SH, file_exp(saver));
            termlib_init();
            no_echo();           // make terminal do what we want
            cr_mode();
        }
        else if (s_tmp_fp != nullptr || (s_tmp_fp = std::fopen(g_save_dest.c_str(), "a")) != nullptr)
        {
            bool quote_From = false;
            std::fseek(s_tmp_fp,0,2);
            if (mailbox)
            {
#if MBOX_CHAR == '\001'
                std::fprintf(s_tmpfp,"\001\001\001\001\n");
#else
                const std::string from_line = do_interp("From %t %`LANG= date`\n");
                fmt::print(s_tmp_fp, "{}", from_line);
                quote_From = true;
#endif
            }
            if (g_save_from == 0 && g_art != 0)
            {
                fmt::print(s_tmp_fp, "Article: {} of {}\n", g_art.value_of(), g_newsgroup_name);
            }
            seek_art(g_save_from);
            std::string article_line;
            article_line.reserve(LINE_BUF_LEN);
            while (read_art(article_line))
            {
                const std::string_view line_text{article_line};
                if (quote_From && string_case_equal(line_text.substr(0, 5), "from "))
                {
                    fmt::print(s_tmp_fp, ">");
                }
                fmt::print(s_tmp_fp, "{}", line_text);
            }
            fmt::print(s_tmp_fp, "\n\n");
#if MBOX_CHAR == '\001'
            if (mailbox)
            {
                std::fprintf(s_tmpfp,"\001\001\001\001\n");
            }
#endif
            std::fclose(s_tmp_fp);
            i = 0; // TODO: set non-zero on write error
        }
        else
        {
            i = 1;
        }
        if (i)
        {
            fmt::print("Not saved");
        }
        else
        {
            fmt::print("{} to {} {}", there ? "Appended" : "Saved", mailbox ? "mailbox" : "file", g_save_dest);
        }
        if (interactive)
        {
            newline();
        }
    }
s_bomb:
    chdir_news_dir();
    return SAVE_DONE;
}

SaveResult view_article()
{
    parse_header(g_art);
    mime_set_article();
    clear_art_buf();
    g_save_from = g_header_type[PAST_HEADER].min_pos;
    if (art_open(g_art, g_save_from) == nullptr)
    {
        if (g_verbose)
        {
            fmt::print("\nNo attatchments on an empty article.\n");
        }
        else
        {
            fmt::print("{}", s_empty_article);
        }
        term_down(2);
        return SAVE_DONE;
    }
    fmt::print("Processing attachments...\n");
    term_down(1);
    if (g_is_mime)
    {
        mime_decode_article(true);
    }
    else
    {
        int part;
        int total;
        int cnt = 0;

        // Scan subject for filename and part number information
        std::string filename = decode_subject(g_art, &part, &total);
        for (g_art_pos = g_save_from; read_art(g_art_line); g_art_pos = tell_art())
        {
            if (g_art_line.empty() || g_art_line.front() <= ' ')
            {
                continue; // Ignore empty or initially-whitespace lines
            }
            if (uue_prescan(g_art_line, filename, &part, &total))
            {
                MimeCapEntry *mc = mime_find_mimecap_entry("image/jpeg", MCF_NONE); // TODO: refine this
                g_save_from = g_art_pos;
                seek_art(g_save_from);
                g_mime_section->m_type = UNHANDLED_MIME;
                if (!filename.empty())
                {
                    g_mime_section->m_filename = filename;
                }
                else
                {
                    g_mime_section->m_filename.reset();
                }
                g_mime_section->m_encoding = MENCODE_UUE;
                g_mime_section->m_part = part;
                g_mime_section->m_total = total;
                if (mc && !decode_piece(mc, {}) && !g_msg.empty())
                {
                    newline();
                    fmt::print("{}", g_msg);
                }
                newline();
                cnt = 0;
            }
            else if (++cnt == 300)
            {
                break;
            }
        } // for
        if (cnt)
        {
            fmt::print("Unable to determine type of file.\n");
            term_down(1);
        }
    }
    chdir_news_dir();
    return SAVE_DONE;
}

int cancel_article()
{
    int  myuid = current_user_id();
    int  r = -1;

    if (art_open(g_art, (ArticlePosition) 0) == nullptr)
    {
        if (g_verbose)
        {
            fmt::print("\nCan't cancel an empty article.\n");
        }
        else
        {
            fmt::print("{}", s_empty_article);
        }
        term_down(2);
        return r;
    }
    std::string reply_buf = fetch_lines(g_art, REPLY_LINE);
    std::string from_buf = fetch_lines(g_art, FROM_LINE);
    if (!string_case_equal(get_env_var("FROM"), from_buf)                    //
        && (!in_string(from_buf, g_host_name, false)                         //
            || (!in_string(from_buf, g_login_name, true)                     //
                && !in_string(reply_buf, g_login_name, true)                 //
#ifdef HAS_NEWS_ADMIN
                && myuid != g_news_uid //
#endif
                && myuid != ROOT_UID)))
    {
#ifdef DEBUG
        if (g_debug)
        {
            fmt::print("\n{}@{} != {}\n", g_login_name, g_host_name, from_buf);
            fmt::print("{} != {}\n", get_env_var("FROM"), from_buf);
            term_down(3);
        }
#endif
        if (g_verbose)
        {
            fmt::print("\nYou can't cancel someone else's article\n");
        }
        else
        {
            fmt::print("\nNot your article\n");
        }
        term_down(2);
    }
    else
    {
        std::FILE *header = std::fopen(g_head_name.c_str(), "w"); // open header file
        if (header == nullptr)
        {
            fmt::print("Can't create {}\n", g_head_name);
            term_down(1);
            goto done;
        }
        fmt::print(header, "{}", do_interp(get_env_var("CANCELHEADER", CANCEL_HEADER)));
        std::fclose(header);
        fmt::print("\nCanceling...\n");
        term_down(2);
        r = do_shell(SH, file_exp(get_env_var("CANCEL", CALL_INEWS)));
    }
done:
    return r;
}

int supersede_article(std::string_view command) // Supersedes:
{
    int  myuid = current_user_id();
    int  r = -1;
    bool incl_body = (response_command_char(command) == 'Z');

    if (art_open(g_art, (ArticlePosition) 0) == nullptr)
    {
        if (g_verbose)
        {
            fmt::print("\nCan't supersede an empty article.\n");
        }
        else
        {
            fmt::print("{}", s_empty_article);
        }
        term_down(2);
        return r;
    }
    std::string reply_buf = fetch_lines(g_art, REPLY_LINE);
    std::string from_buf = fetch_lines(g_art, FROM_LINE);
    if (!string_case_equal(get_env_var("FROM"), from_buf)    //
        && (!in_string(from_buf, g_host_name, false)         //
            || (!in_string(from_buf, g_login_name, true)     //
                && !in_string(reply_buf, g_login_name, true) //
#ifdef HAS_NEWS_ADMIN                                        //
                && myuid != g_news_uid                       //
#endif
                && myuid != ROOT_UID)))
    {
#ifdef DEBUG
        if (g_debug)
        {
            fmt::print("\n{}@{} != {}\n", g_login_name, g_host_name, from_buf);
            fmt::print("{} != {}\n", get_env_var("FROM"), from_buf);
            term_down(3);
        }
#endif
        if (g_verbose)
        {
            fmt::print("\nYou can't supersede someone else's article\n");
        }
        else
        {
            fmt::print("\nNot your article\n");
        }
        term_down(2);
    }
    else
    {
        std::FILE *header = std::fopen(g_head_name.c_str(), "w"); // open header file
        if (header == nullptr)
        {
            fmt::print("Can't create {}\n", g_head_name);
            term_down(1);
            goto done;
        }
        fmt::print(header, "{}", do_interp(get_env_var("SUPERSEDEHEADER", SUPERSEDE_HEADER)));
        if (incl_body && g_art_fp != nullptr)
        {
            parse_header(g_art);
            seek_art(g_header_type[PAST_HEADER].min_pos);
            std::string article_line;
            article_line.reserve(LINE_BUF_LEN);
            while (read_art(article_line))
            {
                fmt::print(header, "{}", article_line);
            }
        }
        std::fclose(header);
        follow_it_up();
        r = 0;
    }
done:
    return r;
}

static int nntp_date()
{
    return nntp_command("DATE");
}

static void follow_it_up()
{
    if (invoke(file_exp(get_env_var("NEWSPOSTER", NEWS_POSTER)).c_str(), //
               g_orig_dir.c_str()) == 42)
    {
        int ret;
        if ((g_data_source->m_flags & DF_REMOTE) &&
            (nntp_date() <= 0 || (nntp_check() < 0 && nntp_response_code(g_ser_line) != NNTP_BAD_COMMAND_VAL)))
        {
            ret = 1;
        }
        else
        {
            ret = invoke(file_exp(CALL_INEWS).c_str(), g_orig_dir.c_str());
        }
        if (ret)
        {
            int               appended = 0;
            const std::string deadart = file_exp("%./dead.article");
            std::ofstream     fp_out{deadart, std::ios::app};
            if (fp_out)
            {
                std::ifstream fp_in{g_head_name};
                if (fp_in)
                {
                    std::string line;
                    line.reserve(CMD_BUF_LEN);
                    while (std::getline(fp_in, line))
                    {
                        fp_out << line;
                        if (!fp_in.eof())
                        {
                            fp_out << '\n';
                        }
                    }
                    appended = 1;
                }
            }
            if (appended)
            {
                fmt::print("Article appended to {}\n", deadart);
            }
            else
            {
                fmt::print("Unable to append article to {}\n", deadart);
            }
        }
    }
}

void reply(std::string_view command)
{
    bool incl_body = (response_command_char(command) == 'R' && g_art);
    const std::string mail_doer = get_env_var("MAILPOSTER", MAIL_POSTER);

    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");       // open header file
    if (header == nullptr)
    {
        fmt::print("Can't create {}\n", g_head_name);
        term_down(1);
        return;
    }
    const std::string header_text = do_interp(get_env_var("MAILHEADER", MAIL_HEADER));
    fmt::print(header, "{}", header_text);
    if (!in_string(mail_doer, "%h", true))
    {
        if (g_verbose)
        {
            fmt::print("\n{}\n(Above lines saved in file {})\n", header_text, g_head_name);
        }
        else
        {
            fmt::print("\n{}\n(Header in {})\n", header_text, g_head_name);
        }
        term_down(3);
    }
    if (incl_body && g_art_fp != nullptr)
    {
        std::string article_line;
        article_line.reserve(LINE_BUF_LEN);
        const std::string introduction = do_interp(get_env_var("YOUSAID", YOU_SAID));
        fmt::print(header, "{}\n", introduction);
        parse_header(g_art);
        mime_set_article();
        clear_art_buf();
        seek_art(g_header_type[PAST_HEADER].min_pos);
        g_wrapped_nl = '\n';
        while (read_art_buf(article_line, false))
        {
            if (!article_line.empty() && article_line.back() == '\n')
            {
                article_line.pop_back();
            }
            fmt::print(header, "{}{}\n", g_indent_string, str_char_subst(article_line, *g_char_subst));
        }
        fmt::print(header, "\n");
        g_wrapped_nl = WRAPPED_NL;
    }
    std::fclose(header);
    invoke(file_exp(mail_doer).c_str(), g_orig_dir.c_str());
}

void forward()
{
    std::string       header_text;
    const std::string mail_doer = get_env_var("FORWARDPOSTER", FORWARD_POSTER);
#ifdef REGEX_WORKS_RIGHT
    COMPEX mime_compex;
#else
    constexpr std::string_view content_type_prefix{"Content-Type: multipart/"};
    constexpr std::string_view boundary_prefix{"boundary=\""};
    std::string                mime_boundary_storage;
#endif
    const char *mime_boundary;

#ifdef REGEX_WORKS_RIGHT
    mime_compex.init_compex();
#endif
    art_open(g_art, (ArticlePosition) 0);
    std::FILE *header = std::fopen(g_head_name.c_str(), "w"); // open header file
    if (header == nullptr)
    {
        fmt::print("Can't create {}\n", g_head_name);
        term_down(1);
        goto done;
    }
    header_text = do_interp(get_env_var("FORWARDHEADER", FORWARD_HEADER));
    fmt::print(header, "{}", header_text);
#ifdef REGEX_WORKS_RIGHT
    if (!mime_compex.compile("Content-Type: multipart/.*; *boundary=\"\\([^\"]*\\)\"",true,true)
        && mime_compex.execute(header_text.c_str()) != nullptr)
    {
        mime_boundary = mime_compex.get_bracket(1);
    }
    else
    {
        mime_boundary = nullptr;
    }
#else
    mime_boundary = nullptr;
    for (std::string_view header_lines{header_text}; !header_lines.empty();)
    {
        const std::size_t      eol = header_lines.find('\n');
        const std::string_view line = header_lines.substr(0, eol == std::string_view::npos ? header_lines.size() : eol);
        if (!line.empty() && line.front() == 'C' && line.size() >= content_type_prefix.size() &&
            string_case_equal(line.substr(0, content_type_prefix.size()), content_type_prefix))
        {
            std::string_view content_type = header_lines.substr(content_type_prefix.size());
            std::size_t      content_type_end = 0;
            while (content_type_end < content_type.size())
            {
                const std::size_t newline = content_type.find('\n', content_type_end);
                if (newline == std::string_view::npos)
                {
                    content_type_end = content_type.size();
                    break;
                }
                if (newline + 1 == content_type.size() ||
                    !std::isspace(static_cast<unsigned char>(content_type[newline + 1])))
                {
                    content_type_end = newline;
                    break;
                }
                content_type_end = newline + 1;
            }
            content_type = content_type.substr(0, content_type_end);
            for (std::size_t param_start = content_type.find(';'); param_start != std::string_view::npos;
                 param_start = content_type.find(';', param_start))
            {
                ++param_start;
                while (param_start < content_type.size() && content_type[param_start] == ' ')
                {
                    ++param_start;
                }
                const std::string_view param = content_type.substr(param_start);
                if (param.size() >= boundary_prefix.size() &&
                    string_case_equal(param.substr(0, boundary_prefix.size()), boundary_prefix))
                {
                    const std::string_view boundary = param.substr(boundary_prefix.size());
                    const std::size_t      boundary_end = boundary.find('"');
                    if (boundary_end != std::string_view::npos)
                    {
                        mime_boundary_storage = boundary.substr(0, boundary_end);
                        mime_boundary = mime_boundary_storage.c_str();
                    }
                    break;
                }
            }
            break;
        }
        if (eol == std::string_view::npos)
        {
            break;
        }
        header_lines.remove_prefix(eol + 1);
    }
#endif
    if (!in_string(mail_doer, "%h", true))
    {
        if (g_verbose)
        {
            fmt::print("\n{}\n(Above lines saved in file {})\n", header_text, g_head_name);
        }
        else
        {
            fmt::print("\n{}\n(Header in {})\n", header_text, g_head_name);
        }
        term_down(3);
    }
    if (g_art_fp != nullptr)
    {
        std::string forward_message = do_interp(get_env_var("FORWARDMSG", FORWARD_MSG));
        if (mime_boundary)
        {
            if (!forward_message.empty() &&
                string_case_compare(std::string_view{forward_message}.substr(0, 8), "Content-") != 0)
            {
                forward_message = "Content-Type: text/plain\n";
            }
            fmt::print(header, "--{}\n{}\n[Replace this with your comments.]\n\n--{}\nContent-Type: message/rfc822\n\n",
                       mime_boundary, forward_message, mime_boundary);
        }
        else if (!forward_message.empty())
        {
            fmt::print(header, "{}\n", forward_message);
        }
        parse_header(g_art);
        seek_art((ArticlePosition)0);
        std::string article_line;
        article_line.reserve(LINE_BUF_LEN + 1);
        while (read_art(article_line))
        {
            const std::string_view line_text{article_line};
            if (!mime_boundary && !line_text.empty() && line_text.front() == '-')
            {
                fmt::print(stdout, "- ");
            }
            fmt::print(header, "{}", line_text);
        }
        if (mime_boundary)
        {
            fmt::print(header, "\n--{}--\n", mime_boundary);
        }
        else
        {
            const std::string forward_message_end = do_interp(get_env_var("FORWARDMSGEND", FORWARD_MSG_END));
            if (!forward_message_end.empty())
            {
                fmt::print(header, "{}\n", forward_message_end);
            }
        }
    }
    std::fclose(header);
    invoke(file_exp(mail_doer).c_str(), g_orig_dir.c_str());
done:
#ifdef REGEX_WORKS_RIGHT
    mime_compex.free_compex();
#endif
    return;
}

void followup(std::string_view command)
{
    bool       incl_body = (response_command_char(command) == 'F' && g_art);
    ArticleNum oldart = g_art;

    if (!incl_body && g_art <= g_last_art)
    {
        term_down(2);
        in_answer("\n\nAre you starting an unrelated topic? [ynq] ", MM_FOLLOWUP_NEW_TOPIC_PROMPT);
        const std::string answer = set_def(std::string_view{g_buf}, "y");
        const char        answer_char = answer.empty() ? '\0' : answer.front();
        if (answer_char == 'q') // TODO: need to add 'h' also
        {
            return;
        }
        if (answer_char != 'n')
        {
            g_art = article_after(g_last_art);
        }
    }
    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");
    if (header == nullptr)
    {
        fmt::print("Can't create {}\n", g_head_name);
        term_down(1);
        g_art = oldart;
        return;
    }
    fmt::print(header, "{}", do_interp(get_env_var("NEWSHEADER", NEWS_HEADER)));
    if (incl_body && g_art_fp != nullptr)
    {
        std::string article_line;
        article_line.reserve(LINE_BUF_LEN);
        if (g_verbose)
        {
            fmt::print("\n"
                       "(Be sure to double-check the attribution against the signature, and\n"
                       "trim the quoted article down as much as possible.)\n");
        }
        const std::string attribution = do_interp(get_env_var("ATTRIBUTION", ATTRIBUTION));
        fmt::print(header, "{}\n", attribution);
        parse_header(g_art);
        mime_set_article();
        clear_art_buf();
        seek_art(g_header_type[PAST_HEADER].min_pos);
        g_wrapped_nl = '\n';
        while (read_art_buf(article_line, false))
        {
            if (!article_line.empty() && article_line.back() == '\n')
            {
                article_line.pop_back();
            }
            fmt::print(header, "{}{}\n", g_indent_string, str_char_subst(article_line, *g_char_subst));
        }
        fmt::print(header, "\n");
        g_wrapped_nl = WRAPPED_NL;
    }
    std::fclose(header);
    follow_it_up();
    g_art = oldart;
}

static int invoke(const char *cmd, const char *dir)
{
    const MinorMode old_mode = g_mode;
    int ret = -1;

    if (g_data_source->m_flags & DF_REMOTE)
    {
        nntp_finish_body(FB_SILENT);
    }
#ifdef DEBUG
    if (g_debug)
    {
        fmt::print("\nInvoking command: {}\n", cmd);
    }
#endif
    if (dir)
    {
        if (change_dir(dir))
        {
            fmt::print("Can't chdir to directory {}\n", dir);
            return ret;
        }
    }
    set_mode(g_general_mode,MM_EXECUTE);
    termlib_reset();
    reset_tty();                  // make terminal well-behaved
    ret = do_shell(SH, cmd);      // do the command
    no_echo();                   // set no echo
    cr_mode();                   // and cbreak mode
    termlib_init();
    set_mode(g_general_mode,old_mode);
    if (dir)
    {
        chdir_news_dir();
    }
    return ret;
}
