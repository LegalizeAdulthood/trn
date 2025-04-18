/* respond.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/respond.h"

#include "config/user_id.h"
#include "nntp/nntpclient.h"
#include "trn/ngdata.h"
#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artstate.h"
#include "trn/change_dir.h"
#include "trn/charsubst.h"
#include "trn/datasrc.h"
#include "trn/decode.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/intrp.h"
#include "trn/mime.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "trn/uudecode.h"
#include "util/env.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

std::string     g_save_dest;          // value of %b
std::string     g_extract_dest;       // value of %E
std::string     g_extract_prog;       // value of %e
ArticlePosition g_save_from{};        // value of %B
bool            g_mbox_always{};      // -M
bool            g_norm_always{};      // -N
std::string     g_priv_dir;           // private news directory
std::string     g_indent_string{">"}; // indent for old article embedded in followup

static char       s_empty_article[] = "\nEmpty article.\n";
static std::FILE *s_tmp_fp{};

static void follow_it_up();
#if 0
static bool cut_line(char *str);
#endif

void respond_init()
{
    g_save_dest.clear();
    g_extract_dest.clear();
}

SaveResult save_article()
{
    char* s;
    char* c;
    char altbuf[CMD_BUF_LEN];
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
    g_save_from = (cmd == 'w' || cmd == 'e')? g_header_type[PAST_HEADER].min_pos : 0;
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
        std::printf(g_no_cd,g_priv_dir.c_str());
        sig_catcher(0);
    }
    if (cmd == 'e')             // is this an extract command?
    {
        static bool custom_extract = false;
        char*       cmdstr;
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
        safe_copy(altbuf,file_exp(s),sizeof altbuf);
        s = altbuf;
        if (*s)
        {
            cmdstr = copy_till(g_buf,s,'|');      // check for |
            s = g_buf + std::strlen(g_buf)-1;
            while (*s == ' ')
            {
                s--;                            // trim trailing spaces
            }
            *++s = '\0';
            if (*cmdstr)
            {
                s = cmdstr+1;                   // skip |
                s = skip_eq(s, ' ');
                if (*s)                         // if new command, use it
                {
                    g_extract_prog = s;          // put extractor in %e
                }
                else
                {
                    static char cmdbuff[1024];
                    std::strcpy(cmdbuff, g_extract_prog.c_str());
                    cmdstr = cmdbuff;
                }
            }
            else
            {
                cmdstr = nullptr;
            }
            s = g_buf;
        }
        else
        {
            if (!g_extract_dest.empty())
            {
                std::strcpy(s, g_extract_dest.c_str());
            }
            if (custom_extract)
            {
                static char cmdbuff[1024];
                std::strcpy(cmdbuff, g_extract_prog.c_str());
                cmdstr = cmdbuff;
            }
            else
            {
                cmdstr = nullptr;
            }
        }
        custom_extract = (cmdstr != nullptr);

        if (!FILE_REF(s))       // relative path?
        {
            c = (s==g_buf ? altbuf : g_buf);
            interp(c, (sizeof g_buf), get_val("SAVEDIR",SAVEDIR));
            if (make_dir(c, MD_DIR))      // ensure directory exists
            {
                std::strcpy(c,g_priv_dir.c_str());
            }
            if (*s)
            {
                while (*c)
                {
                    c++;
                }
                *c++ = '/';
                std::strcpy(c,s);            // add filename
            }
            s = (s==g_buf ? altbuf : g_buf);
        }
        if (!FILE_REF(s))       // path still relative?
        {
            c = (s==g_buf ? altbuf : g_buf);
            std::sprintf(c, "%s/%s", g_priv_dir.c_str(), s);
            s = c;                      // absolutize it
        }
        g_extract_dest = s; // make it handy for %E
        {
            static char buff[512];
            std::strcpy(buff, g_extract_dest.c_str());
            s = buff;
        }
        if (make_dir(s, MD_DIR))         // ensure directory exists
        {
            g_int_count++;
            return SAVE_DONE;
        }
        if (change_dir(s))
        {
            std::printf(g_no_cd,s);
            sig_catcher(0);
        }
        c = trn_getwd(g_buf, sizeof(g_buf));    // simplify path for output
        if (custom_extract)
        {
            std::printf("Extracting article into %s using %s\n",c,g_extract_prog.c_str());
            term_down(1);
            interp(g_cmd_buf, sizeof g_cmd_buf, get_val("CUSTOMSAVER",CUSTOMSAVER));
            invoke(g_cmd_buf, nullptr);
        }
        else if (g_is_mime)
        {
            std::printf("Extracting MIME article into %s:\n", c);
            term_down(1);
            mime_decode_article(false);
        }
        else
        {
            char *filename;
            int   part;
            int   total;
            int decode_type = 0;
            int cnt = 0;

            // Scan subject for filename and part number information
            filename = decode_subject(g_art, &part, &total);
            if (partOpt)
            {
                part = partOpt;
            }
            if (totalOpt)
            {
                total = totalOpt;
            }
            for (g_art_pos = g_save_from;
                 read_art(g_art_line,sizeof g_art_line) != nullptr;
                 g_art_pos = tell_art())
            {
                if (*g_art_line <= ' ')
                {
                    continue;   // Ignore empty or initially-whitespace lines
                }
                if (((*g_art_line == '#' || *g_art_line == ':')            //
                     && (!std::strncmp(g_art_line + 1, "! /bin/sh", 9)     //
                         || !std::strncmp(g_art_line + 1, "!/bin/sh", 8)   //
                         || !std::strncmp(g_art_line + 2, "This is ", 8))) //
#if 0
                    || !std::strncmp(g_art_line, "sed ", 4)                //
                    || !std::strncmp(g_art_line, "cat ", 4)                //
                    || !std::strncmp(g_art_line, "echo ", 5)               //
#endif
                )
                {
                    g_save_from = g_art_pos;
                    decode_type = 1;
                    break;
                }
                if (uue_prescan(g_art_line, &filename, &part, &total))
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
            }// for
            switch (decode_type)
            {
            case 1:
                std::printf("Extracting shar into %s:\n", c);
                term_down(1);
                interp(g_cmd_buf,(sizeof g_cmd_buf),get_val("SHARSAVER",SHARSAVER));
                invoke(g_cmd_buf, nullptr);
                break;

            case 2:
                std::printf("Extracting uuencoded file into %s:\n", c);
                term_down(1);
                g_mime_section->type = IMAGE_MIME;
                safe_free(g_mime_section->filename);
                g_mime_section->filename = filename? save_str(filename) : nullptr;
                g_mime_section->encoding = MENCODE_UUE;
                g_mime_section->part = part;
                g_mime_section->total = total;
                if (!decode_piece(nullptr,nullptr) && *g_msg)
                {
                    newline();
                    std::fputs(g_msg,stdout);
                }
                newline();
                break;

            default:
                std::printf("Unable to determine type of file.\n");
                term_down(1);
                break;
            }
        }// if
    }
    else if ((s = std::strchr(g_buf,'|')) != nullptr)   // is it a pipe command?
    {
        s++;                    // skip the |
        s = skip_eq(s, ' ');
        safe_copy(altbuf,file_exp(s),sizeof altbuf);
        g_save_dest = altbuf;
        if (g_data_source->flags & DF_REMOTE)
        {
            nntp_finish_body(FB_SILENT);
        }
        interp(g_cmd_buf, (sizeof g_cmd_buf), get_val("PIPESAVER",PIPESAVER));
                                // then set up for command
        termlib_reset();
        reset_tty();              // restore tty state
        if (use_pref)           // use preferred shell?
        {
            do_shell(nullptr,g_cmd_buf);
                                // do command with it
        }
        else
        {
            do_shell(SH,g_cmd_buf);  // do command with sh
        }
        no_echo();               // and stop echoing
        cr_mode();               // and start cbreaking
        termlib_init();
    }
    else                        // normal save
    {
        bool  there;
        bool  mailbox;
        char * savename = get_val("SAVENAME",SAVENAME);

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
        safe_copy(altbuf,file_exp(s),sizeof altbuf);
        s = altbuf;
        if (!FILE_REF(s))
        {
            interp(g_buf, (sizeof g_buf), get_val("SAVEDIR",SAVEDIR));
            if (make_dir(g_buf, MD_DIR))  // ensure directory exists
            {
                std::strcpy(g_buf, g_priv_dir.c_str());
            }
            if (*s)
            {
                for (c = g_buf; *c; c++)
                {
                }
                *c++ = '/';
                std::strcpy(c,s);            // add filename
            }
            s = g_buf;
        }
        stat_t save_dir_stat{};
        for (int i = 0;
            (there = stat(s,&save_dir_stat) >= 0) && S_ISDIR(save_dir_stat.st_mode);
            i++)                        // is it a directory?
        {
            c = (s+std::strlen(s));
            *c++ = '/';                 // put a slash before filename
            static char s_news[] = "News";
            interp(c, s == g_buf ? (sizeof g_buf) : (sizeof altbuf), i ? s_news : savename);
                                // generate a default name somehow or other
        }
        make_dir(s, MD_FILE);
        if (!FILE_REF(s))       // relative path?
        {
            c = (s==g_buf ? altbuf : g_buf);
            std::sprintf(c, "%s/%s", g_priv_dir.c_str(), s);
            s = c;                      // absolutize it
        }
        g_save_dest = s; // make it handy for %b
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
                const char* dflt = (in_string(savename,"%a", true) ? "nyq" : "ynq");

                std::sprintf(g_cmd_buf,
                "\nFile %s doesn't exist--\n        use mailbox format?",s);
reask_save:
                in_char(g_cmd_buf, MM_USE_MAILBOX_FORMAT_PROMPT, dflt);
                newline();
                print_cmd();
                if (*g_buf == 'h')
                {
                    if (g_verbose)
                    {
                        std::printf("\n"
                               "Type y to create %s as a mailbox.\n"
                               "Type n to create it as a normal file.\n"
                               "Type q to abort the save.\n",
                               s);
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
                    std::fputs(g_h_for_help,stdout);
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
            s_tmp_fp = std::fopen(s,"r+");
            if (!s_tmp_fp)
            {
                mailbox = false;
            }
            else
            {
                if (std::fread(g_buf, 1, LINE_BUF_LEN, s_tmp_fp))
                {
                    c = g_buf;
                    if (!std::isspace(MBOXCHAR))   // if non-zero,
                    {
                        c = skip_space(c);   // check the first character
                    }
                    mailbox = (*c == MBOXCHAR);
                }
                else
                {
                    mailbox = g_mbox_always;    // if zero length, recheck -M
                }
            }
        }

        s = get_val(mailbox ? "MBOXSAVER" : "NORMSAVER");
        int i;
        if (s)
        {
            if (s_tmp_fp)
            {
                std::fclose(s_tmp_fp);
            }
            safe_copy(g_cmd_buf, file_exp(s), sizeof g_cmd_buf);
            if (g_data_source->flags & DF_REMOTE)
            {
                nntp_finish_body(FB_SILENT);
            }
            termlib_reset();
            reset_tty();          // make terminal behave
            i = do_shell(use_pref?nullptr:SH,g_cmd_buf);
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
#if MBOXCHAR == '\001'
                std::fprintf(s_tmpfp,"\001\001\001\001\n");
#else
                interp(g_cmd_buf, sizeof g_cmd_buf, "From %t %`LANG= date`\n");
                std::fputs(g_cmd_buf, s_tmp_fp);
                quote_From = true;
#endif
            }
            if (g_save_from == 0 && g_art != 0)
            {
                std::fprintf(s_tmp_fp, "Article: %ld of %s\n", g_art, g_newsgroup_name.c_str());
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
#if MBOXCHAR == '\001'
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
            std::printf("%s to %s %s", there? "Appended" : "Saved",
                   mailbox? "mailbox" : "file", g_save_dest.c_str());
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
        char *filename;
        int   part;
        int   total;
        int cnt = 0;

        // Scan subject for filename and part number information
        filename = decode_subject(g_art, &part, &total);
        for (g_art_pos = g_save_from;
             read_art(g_art_line,sizeof g_art_line) != nullptr;
             g_art_pos = tell_art())
        {
            if (*g_art_line <= ' ')
            {
                continue;       // Ignore empty or initially-whitespace lines
            }
            if (uue_prescan(g_art_line, &filename, &part, &total))
            {
                MimeCapEntry*mc = mime_find_mimecap_entry("image/jpeg", MCF_NONE); // TODO: refine this
                g_save_from = g_art_pos;
                seek_art(g_save_from);
                g_mime_section->type = UNHANDLED_MIME;
                safe_free(g_mime_section->filename);
                g_mime_section->filename = filename? save_str(filename) : nullptr;
                g_mime_section->encoding = MENCODE_UUE;
                g_mime_section->part = part;
                g_mime_section->total = total;
                if (mc && !decode_piece(mc, nullptr) && *g_msg)
                {
                    newline();
                    std::fputs(g_msg,stdout);
                }
                newline();
                cnt = 0;
            }
            else if (++cnt == 300)
            {
                break;
            }
        }// for
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
    char hbuf[5*LINE_BUF_LEN];
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
    char *reply_buf = fetch_lines(g_art, REPLY_LINE);
    char *from_buf = fetch_lines(g_art, FROM_LINE);
    char *ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
    if (!string_case_equal(get_val_const("FROM", ""), from_buf)      //
        && (!in_string(from_buf, g_host_name, false)                  //
            || (!in_string(from_buf, g_login_name.c_str(), true)     //
                && !in_string(reply_buf, g_login_name.c_str(), true) //
#ifdef HAS_NEWS_ADMIN
                && myuid != g_news_uid //
#endif
                && myuid != ROOTID)))
    {
#ifdef DEBUG
        if (debug)
        {
            std::printf("\n%s@%s != %s\n",g_login_name.c_str(),g_hostname,from_buf);
            std::printf("%s != %s\n",get_val("FROM",""),from_buf);
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
            std::printf(g_cant_create,g_head_name.c_str());
            term_down(1);
            goto done;
        }
        interp(hbuf, sizeof hbuf, get_val("CANCELHEADER",CANCELHEADER));
        std::fputs(hbuf,header);
        std::fclose(header);
        std::fputs("\nCanceling...\n",stdout);
        term_down(2);
        r = do_shell(SH,file_exp(get_val_const("CANCEL",CALL_INEWS)));
    }
done:
    std::free(ngs_buf);
    std::free(from_buf);
    std::free(reply_buf);
    return r;
}

int supersede_article()         // Supersedes:
{
    char hbuf[5*LINE_BUF_LEN];
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
    char *reply_buf = fetch_lines(g_art, REPLY_LINE);
    char *from_buf = fetch_lines(g_art, FROM_LINE);
    char *ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
    if (!string_case_equal(get_val_const("FROM", ""), from_buf)      //
        && (!in_string(from_buf, g_host_name, false)                  //
            || (!in_string(from_buf, g_login_name.c_str(), true)     //
                && !in_string(reply_buf, g_login_name.c_str(), true) //
#ifdef HAS_NEWS_ADMIN                                                //
                && myuid != g_news_uid                                //
#endif
                && myuid != ROOTID)))
    {
#ifdef DEBUG
        if (debug)
        {
            std::printf("\n%s@%s != %s\n",g_login_name.c_str(),g_hostname,from_buf);
            std::printf("%s != %s\n",get_val("FROM",""),from_buf);
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
            std::printf(g_cant_create,g_head_name.c_str());
            term_down(1);
            goto done;
        }
        interp(hbuf, sizeof hbuf, get_val("SUPERSEDEHEADER",SUPERSEDEHEADER));
        std::fputs(hbuf,header);
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
    std::free(ngs_buf);
    std::free(from_buf);
    std::free(reply_buf);
    return r;
}

static int nntp_date()
{
    return nntp_command("DATE");
}

static void follow_it_up()
{
    safe_copy(g_cmd_buf,file_exp(get_val_const("NEWSPOSTER",NEWSPOSTER)), sizeof g_cmd_buf);
    if (invoke(g_cmd_buf, g_orig_dir.c_str()) == 42)
    {
        int ret;
        if ((g_data_source->flags & DF_REMOTE) &&
            (nntp_date() <= 0 || (nntp_check() < 0 && std::atoi(g_ser_line) != NNTP_BAD_COMMAND_VAL)))
        {
            ret = 1;
        }
        else
        {
            ret = invoke(file_exp(CALL_INEWS),g_orig_dir.c_str());
        }
        if (ret)
        {
            int   appended = 0;
            char* deadart = file_exp("%./dead.article");
            std::FILE *fp_out = std::fopen(deadart, "a");
            if (fp_out != nullptr)
            {
                std::FILE *fp_in = std::fopen(g_head_name.c_str(), "r");
                if (fp_in != nullptr)
                {
                    while (std::fgets(g_cmd_buf, sizeof g_cmd_buf, fp_in))
                    {
                        std::fputs(g_cmd_buf, fp_out);
                    }
                    std::fclose(fp_in);
                    appended = 1;
                }
                std::fclose(fp_out);
            }
            if (appended)
            {
                std::printf("Article appended to %s\n", deadart);
            }
            else
            {
                std::printf("Unable to append article to %s\n", deadart);
            }
        }
    }
}

void reply()
{
    char hbuf[5*LINE_BUF_LEN];
    bool incl_body = (*g_buf == 'R' && g_art);
    char* maildoer = save_str(get_val_const("MAILPOSTER",MAILPOSTER));

    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");       // open header file
    if (header == nullptr)
    {
        std::printf(g_cant_create,g_head_name.c_str());
        term_down(1);
        goto done;
    }
    interp(hbuf, sizeof hbuf, get_val("MAILHEADER",MAILHEADER));
    std::fputs(hbuf,header);
    if (!in_string(maildoer, "%h", true))
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
        interp(g_buf, (sizeof g_buf), get_val("YOUSAID",YOUSAID));
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
            str_char_subst(hbuf,s,sizeof hbuf,*g_char_subst);
            std::fprintf(header,"%s%s\n",g_indent_string.c_str(),hbuf);
            if (t)
            {
                *t = '\0';
            }
        }
        std::fprintf(header,"\n");
        g_wrapped_nl = WRAPPED_NL;
    }
    std::fclose(header);
    safe_copy(g_cmd_buf,file_exp(maildoer),sizeof g_cmd_buf);
    invoke(g_cmd_buf,g_orig_dir.c_str());
done:
    std::free(maildoer);
}

void forward()
{
    char hbuf[5*LINE_BUF_LEN];
    char* maildoer = save_str(get_val_const("FORWARDPOSTER",FORWARDPOSTER));
#ifdef REGEX_WORKS_RIGHT
    COMPEX mime_compex;
#else
    char* eol;
#endif
    char* mime_boundary;

#ifdef REGEX_WORKS_RIGHT
    init_compex(&mime_compex);
#endif
    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");       // open header file
    if (header == nullptr)
    {
        std::printf(g_cant_create,g_head_name.c_str());
        term_down(1);
        goto done;
    }
    interp(hbuf, sizeof hbuf, get_val("FORWARDHEADER",FORWARDHEADER));
    std::fputs(hbuf,header);
#ifdef REGEX_WORKS_RIGHT
    if (!compile(&mime_compex,"Content-Type: multipart/.*; *boundary=\"\\([^\"]*\\)\"",true,true)
     && execute(&mime_compex,hbuf) != nullptr)
    {
        mime_boundary = getbracket(&mime_compex,1);
    }
    else
    {
        mime_boundary = nullptr;
    }
#else
    mime_boundary = nullptr;
    for (char *s = hbuf; s; s = eol)
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
                    mime_boundary = s+10;
                    s = std::strchr(mime_boundary, '"');
                    if (s != nullptr)
                    {
                        *s = '\0';
                    }
                    mime_boundary = save_str(mime_boundary);
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
    if (!in_string(maildoer, "%h", true))
    {
        if (g_verbose)
        {
            std::printf("\n%s\n(Above lines saved in file %s)\n", hbuf, g_head_name.c_str());
        }
        else
        {
            std::printf("\n%s\n(Header in %s)\n", hbuf, g_head_name.c_str());
        }
        term_down(3);
    }
    if (g_art_fp != nullptr)
    {
        interp(g_buf, sizeof g_buf, get_val("FORWARDMSG",FORWARDMSG));
        if (mime_boundary)
        {
            if (*g_buf && string_case_compare(g_buf, "Content-", 8))
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
            interp(g_buf, (sizeof g_buf), get_val("FORWARDMSGEND",FORWARDMSGEND));
            if (*g_buf)
            {
                std::fprintf(header, "%s\n", g_buf);
            }
        }
    }
    std::fclose(header);
    safe_copy(g_cmd_buf,file_exp(maildoer),sizeof g_cmd_buf);
    invoke(g_cmd_buf,g_orig_dir.c_str());
done:
    std::free(maildoer);
#ifdef REGEX_WORKS_RIGHT
    free_compex(&mime_compex);
#else
    safe_free(mime_boundary);
#endif
}

void followup()
{
    char hbuf[5*LINE_BUF_LEN];
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
            g_art = g_last_art + 1;
        }
    }
    art_open(g_art,(ArticlePosition)0);
    std::FILE *header = std::fopen(g_head_name.c_str(),"w");
    if (header == nullptr)
    {
        std::printf(g_cant_create,g_head_name.c_str());
        term_down(1);
        g_art = oldart;
        return;
    }
    interp(hbuf, sizeof hbuf, get_val("NEWSHEADER",NEWSHEADER));
    std::fputs(hbuf,header);
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
        interp(g_buf, (sizeof g_buf), get_val("ATTRIBUTION",ATTRIBUTION));
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
            str_char_subst(hbuf,s,sizeof hbuf,*g_char_subst);
            std::fprintf(header,"%s%s\n",g_indent_string.c_str(),hbuf);
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

int invoke(const char *cmd, const char *dir)
{
    MinorMode oldmode = g_mode;
    int ret = -1;

    if (g_data_source->flags & DF_REMOTE)
    {
        nntp_finish_body(FB_SILENT);
    }
#ifdef DEBUG
    if (debug)
    {
        std::printf("\nInvoking command: %s\n",cmd);
    }
#endif
    if (dir)
    {
        if (change_dir(dir))
        {
            std::printf(g_no_cd,dir);
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
    set_mode(g_general_mode,oldmode);
    if (dir)
    {
        chdir_news_dir();
    }
    return ret;
}

/*
** cut_line() determines if a line is meant as a "cut here" marker.
** Some examples that we understand:
**
**  BEGIN--cut here--cut here
**
**  ------------------ tear at this line ------------------
**
**  #----cut here-----cut here-----cut here-----cut here----#
*/
#if 0
static bool cut_line(char *str)
{
    char* cp;
    char got_flag;
    char word[80];
    int  dash_cnt, equal_cnt, other_cnt;

    // Disallow any single-/double-quoted, parenthetical or c-commented
    // string lines.  Make sure it has the cut-phrase and at least six
    // '-'s or '='s.  If only four '-'s are present, check for a duplicate
    // of the cut phrase.  If over 20 unknown characters are encountered,
    // assume it isn't a cut line.  If we succeed, return true.
    //
    for (cp = str, dash_cnt = equal_cnt = other_cnt = 0; *cp; cp++)
    {
        switch (*cp)
        {
        case '-':
            dash_cnt++;
            break;

        case '=':
            equal_cnt++;
            break;

        case '/':
            if (*(cp+1) != '*')
            {
                break;
            }

        case '"':
        case '\'':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
            return false;

        default:
            other_cnt++;
            break;
        }
    }
    if (dash_cnt < 4 && equal_cnt < 6)
    {
        return false;
    }

    got_flag = 0;

    for (*(cp = word) = '\0'; *str; str++)
    {
        if (std::islower(*str))
        {
            *cp++ = *str;
        }
        else if (std::isupper(*str))
        {
            *cp++ = std::tolower(*str);
        }
        else
        {
            if (*word)
            {
                *cp = '\0';
                switch (got_flag)
                {
                case 2:
                    if (!std::strcmp(word, "line")     //
                        || !std::strcmp(word, "here")) //
                    {
                        if ((other_cnt -= 4) <= 20)
                        {
                            return true;
                        }
                    }
                    break;

                case 1:
                    if (!std::strcmp(word, "this"))
                    {
                        got_flag = 2;
                        other_cnt -= 4;
                    }
                    else if (!std::strcmp(word, "here"))
                    {
                        other_cnt -= 4;
                        if ((dash_cnt >= 6 || equal_cnt >= 6)
                         && other_cnt <= 20)
                        {
                            return true;
                        }
                        dash_cnt = 6;
                        got_flag = 0;
                    }
                    break;

                case 0:
                    if (!std::strcmp(word, "cut")     //
                        || !std::strcmp(word, "snip") //
                        || !std::strcmp(word, "tear"))
                    {
                        got_flag = 1;
                        other_cnt -= std::strlen(word);
                    }
                    break;
                }
                *(cp = word) = '\0';
            }
        }
    } // for *str

    return false;
}
#endif
