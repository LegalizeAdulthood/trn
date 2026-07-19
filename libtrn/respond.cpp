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
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <trn/uudecode.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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

void respond_init()
{
    g_save_dest.clear();
    g_extract_dest.clear();
}

SaveResult save_article()
{
    char* s;
    bool interactive = (g_buf[1] == FINISH_CMD);
    char cmd = *g_buf;

    if (!finish_command(interactive))   // get rest of command
    {
        return SAVE_ABORT;
    }
    bool use_pref = std::isupper(cmd);
    if (use_pref != 0)
    {
        cmd = std::tolower(cmd);
    }
    parse_header(g_art);
    mime_set_article();
    clear_art_buf();
    g_save_from = (cmd == 'w' || cmd == 'e')? g_header_type[PAST_HEADER].min_pos : ArticlePosition{};
    if (art_open(g_art, g_save_from) == nullptr)
    {
        if (g_verbose)
        {
            std::fputs("\nCan't save an empty article.\n", stdout);
        }
        else
        {
            std::fputs(s_empty_article, stdout);
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

        s = g_buf+1;            // skip e
        s = skip_eq(s, ' ');    // skip leading spaces
        if (*s == '-' && std::isdigit(s[1]))
        {
            partOpt = std::atoi(s+1);
            do
            {
                s++;
            } while (std::isdigit(*s));
            if (*s == '/')
            {
                ++s;
                totalOpt = std::atoi(s);
                s = skip_digits(s);
                s = skip_eq(s, ' ');
            }
            else
            {
                totalOpt = partOpt;
            }
        }
        std::string destination = file_exp(s);
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

        if (!FILE_REF(destination.c_str())) // relative path?
        {
            std::string save_directory = file_exp(get_env_var("SAVEDIR", SAVEDIR));
            save_directory.reserve(LINE_BUF_LEN);
            if (make_dir(save_directory.c_str(), MD_DIR)) // ensure directory exists
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
        if (!FILE_REF(destination.c_str())) // path still relative?
        {
            destination = (fs::path{g_priv_dir} / destination).generic_string();
        }
        g_extract_dest = destination;                 // make it handy for %E
        if (make_dir(g_extract_dest.c_str(), MD_DIR)) // ensure directory exists
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
            const std::string command = do_interp(get_env_var("CUSTOMSAVER", CUSTOM_SAVER));
            invoke(command.c_str(), nullptr);
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
            for (g_art_pos = g_save_from; read_art(g_art_line, sizeof g_art_line) != nullptr; g_art_pos = tell_art())
            {
                if (*g_art_line <= ' ')
                {
                    continue; // Ignore empty or initially-whitespace lines
                }
                if ((*g_art_line == '#' || *g_art_line == ':')            //
                    && (!std::strncmp(g_art_line + 1, "! /bin/sh", 9)     //
                        || !std::strncmp(g_art_line + 1, "!/bin/sh", 8)   //
                        || !std::strncmp(g_art_line + 2, "This is ", 8))) //
                {
                    g_save_from = g_art_pos;
                    decode_type = 1;
                    break;
                }
                if (uue_prescan(g_art_line, filename, &part, &total))
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
                    const std::string command = do_interp(get_env_var("SHARSAVER", SHAR_SAVER));
                    invoke(command.c_str(), nullptr);
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
                if (!decode_piece(nullptr, nullptr) && !g_msg.empty())
                {
                    newline();
                    std::fputs(g_msg.c_str(),stdout);
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
    else if ((s = std::strchr(g_buf,'|')) != nullptr)   // is it a pipe command?
    {
        s++;                    // skip the |
        s = skip_eq(s, ' ');
        g_save_dest = file_exp(s);
        if (g_data_source->m_flags & DF_REMOTE)
        {
            nntp_finish_body(FB_SILENT);
        }
        const std::string command = do_interp(get_env_var("PIPESAVER", PIPE_SAVER));
        // then set up for command
        termlib_reset();
        reset_tty();              // restore tty state
        if (use_pref)           // use preferred shell?
        {
            do_shell(nullptr, command.c_str());
                                // do command with it
        }
        else
        {
            do_shell(SH, command.c_str());  // do command with sh
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

        s = g_buf+1;            // skip s or S
        if (*s == '-')          // if they are confused, skip - also
        {
            if (g_verbose)
            {
                std::fputs("Warning: '-' ignored.  This isn't readnews.\n", stdout);
            }
            else
            {
                std::fputs("'-' ignored.\n", stdout);
            }
            term_down(1);
            s++;
        }
        for (; *s == ' '; s++)
        {
            // skip spaces
        }
        std::string destination = file_exp(s);
        if (!FILE_REF(destination.c_str()))
        {
            std::string save_directory = file_exp(get_env_var("SAVEDIR", SAVEDIR));
            if (make_dir(save_directory.c_str(), MD_DIR)) // ensure directory exists
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
        make_dir(destination.c_str(), MD_FILE);
        if (!FILE_REF(destination.c_str())) // relative path?
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
                const char *dflt = (in_string(savename, "%a", true) ? "nyq" : "ynq");

reask_save:
                in_char(fmt::format("\nFile {} doesn't exist--\n        use mailbox format?", destination).c_str(),
                        MM_USE_MAILBOX_FORMAT_PROMPT, dflt);
                newline();
                print_cmd();
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
                        std::fputs("\n"
                              "y to create mailbox.\n"
                              "n to create normal file.\n"
                              "q to abort.\n",
                              stdout);
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
                    std::fputs("Type h for help.\n", stdout);
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
                if (std::fread(g_buf, 1, LINE_BUF_LEN, s_tmp_fp))
                {
                    char *first = g_buf;
                    if (!std::isspace(MBOX_CHAR))   // if non-zero,
                    {
                        first = skip_space(first); // check the first character
                    }
                    mailbox = (*first == MBOX_CHAR);
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
            i = do_shell(use_pref ? nullptr : SH, file_exp(saver).c_str());
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
                std::fputs(from_line.c_str(), s_tmp_fp);
                quote_From = true;
#endif
            }
            if (g_save_from == 0 && g_art != 0)
            {
                fmt::print(s_tmp_fp, "Article: {} of {}\n", g_art.value_of(), g_newsgroup_name);
            }
            seek_art(g_save_from);
            while (read_art(g_buf, LINE_BUF_LEN) != nullptr)
            {
                if (quote_From && string_case_equal(g_buf, "from ",5))
                {
                    std::putc('>', s_tmp_fp);
                }
                std::fputs(g_buf, s_tmp_fp);
            }
            std::fputs("\n\n", s_tmp_fp);
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
            std::fputs("Not saved", stdout);
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
            std::fputs("\nNo attatchments on an empty article.\n", stdout);
        }
        else
        {
            std::fputs(s_empty_article, stdout);
        }
        term_down(2);
        return SAVE_DONE;
    }
    std::printf("Processing attachments...\n");
    term_down(1);
    if (g_is_mime)
    {
        mime_decode_article(true);
    }
    else
    {
        int   part;
        int   total;
        int   cnt = 0;

        // Scan subject for filename and part number information
        std::string filename = decode_subject(g_art, &part, &total);
        for (g_art_pos = g_save_from; read_art(g_art_line, sizeof g_art_line) != nullptr; g_art_pos = tell_art())
        {
            if (*g_art_line <= ' ')
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
                if (mc && !decode_piece(mc, nullptr) && !g_msg.empty())
                {
                    newline();
                    std::fputs(g_msg.c_str(), stdout);
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
            std::printf("Unable to determine type of file.\n");
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
            std::fputs("\nCan't cancel an empty article.\n", stdout);
        }
        else
        {
            std::fputs(s_empty_article, stdout);
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
            std::fputs("\nYou can't cancel someone else's article\n", stdout);
        }
        else
        {
            std::fputs("\nNot your article\n", stdout);
        }
        term_down(2);
    }
    else
    {
        std::FILE *header = std::fopen(g_head_name.c_str(),"w");   // open header file
        if (header == nullptr)
        {
            fmt::print("Can't create {}\n", g_head_name);
            term_down(1);
            goto done;
        }
        std::fputs(do_interp(get_env_var("CANCELHEADER", CANCEL_HEADER), 5 * LINE_BUF_LEN).c_str(),header);
        std::fclose(header);
        std::fputs("\nCanceling...\n",stdout);
        term_down(2);
        r = do_shell(SH, file_exp(get_env_var("CANCEL", CALL_INEWS)).c_str());
    }
done:
    return r;
}

int supersede_article()         // Supersedes:
{
    int  myuid = current_user_id();
    int  r = -1;
    bool incl_body = (*g_buf == 'Z');

    if (art_open(g_art, (ArticlePosition) 0) == nullptr)
    {
        if (g_verbose)
        {
            std::fputs("\nCan't supersede an empty article.\n", stdout);
        }
        else
        {
            std::fputs(s_empty_article, stdout);
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
#ifdef HAS_NEWS_ADMIN                                                //
                && myuid != g_news_uid                                //
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
            std::fputs("\nYou can't supersede someone else's article\n", stdout);
        }
        else
        {
            std::fputs("\nNot your article\n", stdout);
        }
        term_down(2);
    }
    else
    {
        std::FILE *header = std::fopen(g_head_name.c_str(),"w");   // open header file
        if (header == nullptr)
        {
            fmt::print("Can't create {}\n", g_head_name);
            term_down(1);
            goto done;
        }
        std::fputs(do_interp(get_env_var("SUPERSEDEHEADER", SUPERSEDE_HEADER), 5 * LINE_BUF_LEN).c_str(),header);
        if (incl_body && g_art_fp != nullptr)
        {
            parse_header(g_art);
            seek_art(g_header_type[PAST_HEADER].min_pos);
            while (read_art(g_buf,LINE_BUF_LEN) != nullptr)
            {
                std::fputs(g_buf, header);
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
            (nntp_date() <= 0 || (nntp_check() < 0 && std::atoi(g_ser_line) != NNTP_BAD_COMMAND_VAL)))
        {
            ret = 1;
        }
        else
        {
            ret = invoke(file_exp(CALL_INEWS).c_str(), g_orig_dir.c_str());
        }
        if (ret)
        {
            int   appended = 0;
            const std::string deadart = file_exp("%./dead.article");
            std::FILE        *fp_out = std::fopen(deadart.c_str(), "a");
            if (fp_out != nullptr)
            {
                std::FILE *fp_in = std::fopen(g_head_name.c_str(), "r");
                if (fp_in != nullptr)
                {
                    std::string line(CMD_BUF_LEN, '\0');
                    while (std::fgets(line.data(), static_cast<int>(line.size()), fp_in))
                    {
                        std::fputs(line.c_str(), fp_out);
                    }
                    std::fclose(fp_in);
                    appended = 1;
                }
                std::fclose(fp_out);
            }
            if (appended)
            {
                std::printf("Article appended to %s\n", deadart.c_str());
            }
            else
            {
                std::printf("Unable to append article to %s\n", deadart.c_str());
            }
        }
    }
}

void reply()
{
    bool incl_body = (*g_buf == 'R' && g_art);
    const std::string mail_doer = get_env_var("MAILPOSTER", MAIL_POSTER);

    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");       // open header file
    if (header == nullptr)
    {
        fmt::print("Can't create {}\n", g_head_name);
        term_down(1);
        return;
    }
    constexpr int header_size = 5 * LINE_BUF_LEN;
    std::string   header_text(header_size, '\0');
    interp(header_text.data(), header_size, get_env_var("MAILHEADER", MAIL_HEADER).c_str());
    const std::size_t header_end = header_text.find('\0');
    if (header_end != std::string::npos)
    {
        header_text.resize(header_end);
    }
    std::fputs(header_text.c_str(),header);
    if (!in_string(mail_doer, "%h", true))
    {
        if (g_verbose)
        {
            std::printf("\n%s\n(Above lines saved in file %s)\n", g_buf, g_head_name.c_str());
        }
        else
        {
            std::printf("\n%s\n(Header in %s)\n", g_buf, g_head_name.c_str());
        }
        term_down(3);
    }
    if (incl_body && g_art_fp != nullptr)
    {
        char* s;
        interp(g_buf, (sizeof g_buf), get_env_var("YOUSAID", YOU_SAID).c_str());
        std::fprintf(header,"%s\n",g_buf);
        parse_header(g_art);
        mime_set_article();
        clear_art_buf();
        seek_art(g_header_type[PAST_HEADER].min_pos);
        g_wrapped_nl = '\n';
        while ((s = read_art_buf(false)) != nullptr)
        {
            char *t = std::strchr(s, '\n');
            if (t != nullptr)
            {
                *t = '\0';
            }
            fmt::print(header, "{}{}\n", g_indent_string, str_char_subst(s, *g_char_subst));
            if (t)
            {
                *t = '\0';
            }
        }
        std::fprintf(header,"\n");
        g_wrapped_nl = WRAPPED_NL;
    }
    std::fclose(header);
    invoke(file_exp(mail_doer).c_str(), g_orig_dir.c_str());
}

void forward()
{
    constexpr int header_size = 5 * LINE_BUF_LEN;
    std::string   header_text(header_size, '\0');
    const std::string mail_doer = get_env_var("FORWARDPOSTER", FORWARD_POSTER);
    std::size_t header_end = std::string::npos;
#ifdef REGEX_WORKS_RIGHT
    COMPEX mime_compex;
#else
    char* eol;
    std::string mime_boundary_storage;
#endif
    const char *mime_boundary;

#ifdef REGEX_WORKS_RIGHT
    mime_compex.init_compex();
#endif
    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");       // open header file
    if (header == nullptr)
    {
        fmt::print("Can't create {}\n", g_head_name);
        term_down(1);
        goto done;
    }
    interp(header_text.data(), header_size, get_env_var("FORWARDHEADER", FORWARD_HEADER).c_str());
    header_end = header_text.find('\0');
    if (header_end != std::string::npos)
    {
        header_text.resize(header_end);
    }
    std::fputs(header_text.c_str(),header);
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
    for (char *s = header_text.data(); s; s = eol)
    {
        eol = std::strchr(s, '\n');
        if (eol)
        {
            eol++;
        }
        if (*s == 'C' && string_case_equal(s, "Content-Type: multipart/", 24))
        {
            s += 24;
            while (true)
            {
                for (; *s && *s != ';'; s++)
                {
                    if (*s == '\n' && !std::isspace(s[1]))
                    {
                        break;
                    }
                }
                if (*s != ';')
                {
                    break;
                }
                s = skip_eq(++s, ' ');
                if (*s == 'b' && string_case_equal(s, "boundary=\"", 10))
                {
                    char *boundary = s + 10;
                    s = std::strchr(boundary, '"');
                    if (s != nullptr)
                    {
                        *s = '\0';
                    }
                    mime_boundary_storage = boundary;
                    mime_boundary = mime_boundary_storage.c_str();
                    if (s)
                    {
                        *s = '"';
                    }
                    break;
                }
            }
        }
    }
#endif
    if (!in_string(mail_doer, "%h", true))
    {
        if (g_verbose)
        {
            std::printf("\n%s\n(Above lines saved in file %s)\n", header_text.c_str(), g_head_name.c_str());
        }
        else
        {
            std::printf("\n%s\n(Header in %s)\n", header_text.c_str(), g_head_name.c_str());
        }
        term_down(3);
    }
    if (g_art_fp != nullptr)
    {
        interp(g_buf, sizeof g_buf, get_env_var("FORWARDMSG", FORWARD_MSG).c_str());
        if (mime_boundary)
        {
            if (*g_buf && string_case_compare(std::string_view{g_buf}.substr(0, 8), "Content-") != 0)
            {
                std::strcpy(g_buf, "Content-Type: text/plain\n");
            }
            std::fprintf(header,"--%s\n%s\n[Replace this with your comments.]\n\n--%s\nContent-Type: message/rfc822\n\n",
                    mime_boundary,g_buf,mime_boundary);
        }
        else if (*g_buf)
        {
            std::fprintf(header, "%s\n", g_buf);
        }
        parse_header(g_art);
        seek_art((ArticlePosition)0);
        while (read_art(g_buf, sizeof g_buf) != nullptr)
        {
            if (!mime_boundary && *g_buf == '-')
            {
                std::putchar('-');
                std::putchar(' ');
            }
            std::fprintf(header,"%s",g_buf);
        }
        if (mime_boundary)
        {
            std::fprintf(header, "\n--%s--\n", mime_boundary);
        }
        else
        {
            interp(g_buf, (sizeof g_buf), get_env_var("FORWARDMSGEND", FORWARD_MSG_END).c_str());
            if (*g_buf)
            {
                std::fprintf(header, "%s\n", g_buf);
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

void followup()
{
    constexpr int header_size = 5 * LINE_BUF_LEN;
    std::string   header_text(header_size, '\0');
    bool incl_body = (*g_buf == 'F' && g_art);
    ArticleNum oldart = g_art;

    if (!incl_body && g_art <= g_last_art)
    {
        term_down(2);
        in_answer("\n\nAre you starting an unrelated topic? [ynq] ", MM_FOLLOWUP_NEW_TOPIC_PROMPT);
        set_def(g_buf,"y");
        if (*g_buf == 'q')  // TODO: need to add 'h' also
        {
            return;
        }
        if (*g_buf != 'n')
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
    interp(header_text.data(), header_size, get_env_var("NEWSHEADER", NEWS_HEADER).c_str());
    const std::size_t header_end = header_text.find('\0');
    if (header_end != std::string::npos)
    {
        header_text.resize(header_end);
    }
    std::fputs(header_text.c_str(), header);
    if (incl_body && g_art_fp != nullptr)
    {
        char* s;
        if (g_verbose)
        {
            std::fputs("\n"
                  "(Be sure to double-check the attribution against the signature, and\n"
                  "trim the quoted article down as much as possible.)\n",
                  stdout);
        }
        interp(g_buf, (sizeof g_buf), get_env_var("ATTRIBUTION", ATTRIBUTION).c_str());
        std::fprintf(header,"%s\n",g_buf);
        parse_header(g_art);
        mime_set_article();
        clear_art_buf();
        seek_art(g_header_type[PAST_HEADER].min_pos);
        g_wrapped_nl = '\n';
        while ((s = read_art_buf(false)) != nullptr)
        {
            char *t = std::strchr(s, '\n');
            if (t != nullptr)
            {
                *t = '\0';
            }
            fmt::print(header, "{}{}\n", g_indent_string, str_char_subst(s, *g_char_subst));
            if (t)
            {
                *t = '\0';
            }
        }
        std::fprintf(header,"\n");
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
        std::printf("\nInvoking command: %s\n",cmd);
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
    ret = do_shell(SH,cmd);      // do the command
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
