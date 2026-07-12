/* trn-artchk.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

// A program to check an article's validity and print warnings if problems
// are found.
//
// Usage: trn-artchk <article> <maxLineLen> <newsgroupsFile> <activeFile>
//

#include <config/common.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <trn/string-algos.h>
#include <util/env.h>
#include <util/util2.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

enum
{
    MAX_NGS = 100
};

int server_connection();
int nntp_handle_timeout();

std::string g_server_name;
std::string g_nntp_auth_file;
char        g_buf[LINE_BUF_LEN + 1]; // general purpose line buffer

int main(int argc, char *argv[])
{
    std::FILE *fp_active{};
    std::FILE *fp_ng{};
    bool check_active = false;
    bool check_ng = false;
    char buff[LINE_BUF_LEN];
    char*cp;
    char*ngptrs[MAX_NGS];
    int  nglens[MAX_NGS];
    int  foundactive[MAX_NGS];
    int  max_col_len;
    int  line_num = 0;
    int  ngcnt = 0;
    int  found_newsgroups = 0;

    const char *home_dir = std::getenv("HOME");
    if (home_dir == nullptr)
    {
        home_dir = std::getenv("LOGDIR");
    }
    if (home_dir != nullptr)
    {
        g_home_dir = home_dir;
    }
    g_dot_dir = std::getenv("DOTDIR");
    if (g_dot_dir.empty())
    {
        g_dot_dir = g_home_dir;
    }

    if (argc != 5 || !(max_col_len = std::atoi(argv[2])))
    {
        std::fprintf(stderr, "Usage: trn-artchk <article> <maxLineLen> <newsgroupsFile> <activeFile>\n");
        std::exit(1);
    }
    const fs::path newsgroups_file{argv[3]};
    const fs::path active_file{argv[4]};
    std::error_code file_error;

    std::FILE *fp = std::fopen(argv[1], "r");
    if (fp == nullptr)
    {
        std::fprintf(stderr, "trn-artchk: unable to open article `%s'.\n", argv[1]);
        std::exit(1);
    }

    // Check the header for proper format and report on the newsgroups
    while (std::fgets(buff, LINE_BUF_LEN, fp))
    {
        line_num++;
        buff[std::strlen(buff)-1] = '\0';
        if (!*buff)
        {
            break;
        }
        if (is_hor_space(*buff))
        {
            continue;
        }
        cp = std::strchr(buff, ':');
        if (!cp)
        {
            std::printf("\nERROR: line %d is an invalid header line:\n%s\n",
                   line_num, buff);
            break;
        }
        if (cp[1] != ' ' && cp[1] != '\0')
        {
            std::printf("\n"
                   "ERROR: header on line %d does not have a space after the colon:\n%s\n",
                   line_num, buff);
        }
        if (cp - buff == 10 && !std::strncmp(buff, "Newsgroups", 10))
        {
            found_newsgroups = 1;
            for (cp = buff + 11; *cp == ' '; cp++)
            {
            }
            if (std::strchr(cp, ' '))
            {
                std::printf("\n"
                       "ERROR: the \"Newsgroups:\" line has spaces in it that MUST be removed. The\n"
                       "only allowable space is the one separating the colon (:) from the contents.\n"
                       "Use a comma (,) to separate multiple newsgroup names.\n");
                continue;
            }
            while (*cp)
            {
                char *cp2 = std::strchr(cp, ',');
                if (!cp2)
                {
                    cp2 = cp + std::strlen(cp);
                }
                else
                {
                    *cp2++ = '\0';
                }
                if (ngcnt < MAX_NGS)
                {
                    nglens[ngcnt] = std::strlen(cp);
                    foundactive[ngcnt] = 0;
                    ngptrs[ngcnt] = safe_malloc(nglens[ngcnt]+1);
                    std::strcpy(ngptrs[ngcnt], cp);
                    ngcnt++;
                }
                cp = cp2;
            }
            if (!ngcnt)
            {
                std::printf("\n"
                       "ERROR: the \"Newsgroups:\" line lists no newsgroups.\n");
                continue;
            }
        }
    }
    if (!found_newsgroups)
    {
        printf("\nERROR: the \"Newsgroups:\" line is missing from the header.\n");
    }

    // Check the body of the article for long lines
    while (std::fgets(buff, LINE_BUF_LEN, fp))
    {
        line_num++;
        int col = std::strlen(buff) - 1;
        if (buff[col] != '\n')
        {
            std::printf("\n"
                   "Warning: line %d has no trailing newline character and may get lost.\n",
                   line_num);
        }
        else
        {
            buff[col] = '\0';
        }
        col = 0;
        for (cp = buff; *cp; cp++)
        {
            if (*cp == '\t')
            {
                col += 8 - (col%8);
            }
            else
            {
                col++;
            }
        }
        if (col > max_col_len)
        {
            std::printf("\n"
                   "Warning: posting exceeds %d columns.  Line %d is the first long one:\n%s\n",
                   max_col_len, line_num, buff);
            break;
        }
    }
    cp = std::getenv("NNTPSERVER");
    std::string server_name;
    if (!cp)
    {
        server_name = file_exp(SERVER_NAME);
        if (!server_name.empty())
        {
            cp = server_name.data();
            if (FILE_REF(cp))
            {
                cp = nntp_server_name(cp);
            }
        }
    }
    if (cp != nullptr && std::strcmp(cp, "local") != 0)
    {
        g_server_name = cp;
        if (const auto separator = g_server_name.find_first_of(";:");
            separator != std::string::npos)
        {
            g_nntp_link.port_number =
                std::atoi(g_server_name.c_str() + separator + 1);
            g_server_name.resize(separator);
        }
        g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);
        cp = std::getenv("NNTP_FORCE_AUTH");
        if (cp != nullptr && (*cp == 'y' || *cp == 'Y'))
        {
            g_nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
        if (init_nntp() < 0)
        {
            g_server_name.clear();
        }
    }
    if (ngcnt)
    {
        if (fs::file_size(newsgroups_file, file_error) > 0 && !file_error)
        {
            check_ng = (fp_ng = std::fopen(newsgroups_file.string().c_str(), "r")) != nullptr;
        }
        else if (file_error && !g_server_name.empty() && server_connection())
        {
            check_ng = true;
        }
        file_error.clear();
        if (fs::file_size(active_file, file_error) > 0 && !file_error)
        {
            check_active = (fp_active = std::fopen(active_file.string().c_str(), "r")) != nullptr;
        }
        else if (file_error && !g_server_name.empty() && server_connection())
        {
            check_active = true;
        }
    }
    if (ngcnt && (check_ng || check_active))
    {
        int ngleft;
        // Print a note about each newsgroup
        std::printf("\nYour article's newsgroup%s:\n", plural(ngcnt));
        if (!check_active)
        {
            for (int i = 0; i < ngcnt; i++)
            {
                foundactive[i] = 1;
            }
        }
        else if (fp_active != nullptr)
        {
            ngleft = ngcnt;
            while (std::fgets(buff, LINE_BUF_LEN, fp_active))
            {
                if (!ngleft)
                {
                    break;
                }
                for (int i = 0; i < ngcnt; i++)
                {
                    if (!foundactive[i])
                    {
                        if (is_hor_space(buff[nglens[i]]) //
                            && !std::strncmp(ngptrs[i], buff, nglens[i]))
                        {
                            foundactive[i] = 1;
                            ngleft--;
                        }
                    }
                }
            }
            std::fclose(fp_active);
        }
        else if (!g_server_name.empty())
        {
            int listactive_works = 1;
            for (int i = 0; i < ngcnt; i++)
            {
                if (listactive_works)
                {
                    std::sprintf(g_ser_line, "list active %s", ngptrs[i]);
                    if (nntp_command(g_ser_line) <= 0)
                    {
                        break;
                    }
                    if (nntp_check() > 0)
                    {
                        while (nntp_gets(g_ser_line, sizeof g_ser_line) >= 0)
                        {
                            if (nntp_at_list_end(g_ser_line))
                            {
                                break;
                            }
                            foundactive[i] = 1;
                        }
                    }
                    else if (*g_ser_line == NNTP_CLASS_FATAL)
                    {
                        listactive_works = false;
                        i--;
                    }
                }
                else
                {
                    std::sprintf(g_ser_line, "GROUP %s", ngptrs[i]);
                    if (nntp_command(g_ser_line) <= 0)
                    {
                        break;
                    }
                    if (nntp_check() > 0)
                    {
                        foundactive[i] = 1;
                    }
                }
            }
        }
        if (check_ng && fp_ng == nullptr)
        {
            fp_ng = std::fopen(newsgroups_file.string().c_str(), "w+");
            file_error.clear();
            fs::remove(newsgroups_file, file_error);
            if (fp_ng != nullptr)
            {
                for (int i = 0; i < ngcnt; i++)
                {
                    // issue a description list command
                    std::sprintf(g_ser_line, "XGTITLE %s", ngptrs[i]);
                    if (nntp_command(g_ser_line) <= 0)
                    {
                        break;
                    }
                    // TODO: use list newsgroups if this fails...?
                    if (nntp_check() > 0)
                    {
                        // write results to fp_ng
                        while (nntp_gets(g_ser_line, sizeof g_ser_line) >= 0)
                        {
                            if (nntp_at_list_end(g_ser_line))
                            {
                                break;
                            }
                            std::fprintf(fp_ng, "%s\n", g_ser_line);
                        }
                    }
                }
                std::fseek(fp_ng, 0L, 0);
            }
        }
        if (fp_ng != nullptr)
        {
            ngleft = ngcnt;
            while (std::fgets(buff, LINE_BUF_LEN, fp_ng))
            {
                if (!ngleft)
                {
                    break;
                }
                for (int i = 0; i < ngcnt; i++)
                {
                    if (foundactive[i] && ngptrs[i])
                    {
                        if (is_hor_space(buff[nglens[i]]) //
                            && !std::strncmp(ngptrs[i], buff, nglens[i]))
                        {
                            cp = &buff[nglens[i]];
                            *cp++ = '\0';
                            const char *desc = skip_hor_space(cp);
                            if (desc[0] == '?' && desc[1] == '?')
                            {
                                desc = "[no description available]\n";
                            }
                            std::printf("%-23s %s", buff, desc);
                            free(ngptrs[i]);
                            ngptrs[i] = nullptr;
                            ngleft--;
                        }
                    }
                }
            }
            std::fclose(fp_ng);
        }
        for (int i = 0; i < ngcnt; i++)
        {
            if (!foundactive[i])
            {
                std::printf("%-23s ** invalid news group -- check spelling **\n",
                   ngptrs[i]);
                std::free(ngptrs[i]);
            }
            else if (ngptrs[i])
            {
                std::printf("%-23s [no description available]\n", ngptrs[i]);
                std::free(ngptrs[i]);
            }
        }
    }

    nntp_close(true);
    if (!g_server_name.empty())
    {
        cleanup_nntp();
    }

    return 0;
}

int server_connection()
{
    static int server_stat = 0;
    if (!server_stat)
    {
        if (nntp_connect(g_server_name.c_str(),false) > 0)
        {
            server_stat = 1;
        }
        else
        {
            server_stat = -1;
        }
    }
    return server_stat == 1;
}

int nntp_handle_timeout()
{
    std::fputs("\n503 Server timed out.\n",stderr);
    return -2;
}
