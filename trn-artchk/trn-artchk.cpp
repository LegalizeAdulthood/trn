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
#include <config/env.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <trn/string-algos.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

enum
{
    MAX_NGS = 100
};

struct ArticleNewsgroup
{
    std::string name;
    bool        found_active{};
    bool        found_description{};
};

int server_connection();
int nntp_handle_timeout();

std::string g_server_name;
std::string g_nntp_auth_file;

int main(int argc, char *argv[])
{
    std::ifstream                 active_input;
    std::fstream                  generated_newsgroups;
    std::ifstream                 newsgroups_input;
    std::istream                 *newsgroups_input_stream{};
    bool                          check_active = false;
    bool                          check_ng = false;
    std::vector<ArticleNewsgroup> newsgroups;
    std::string                   line;
    int                           max_col_len;
    int                           line_num = 0;
    int                           found_newsgroups = 0;

    std::string home_dir = get_env_var("HOME");
    if (home_dir.empty())
    {
        home_dir = get_env_var("LOGDIR");
    }
    if (!home_dir.empty())
    {
        g_home_dir = home_dir;
    }
    g_dot_dir = get_env_var("DOTDIR");
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
    const fs::path  active_file{argv[4]};
    std::error_code file_error;

    std::ifstream article{argv[1]};
    if (!article)
    {
        std::fprintf(stderr, "trn-artchk: unable to open article `%s'.\n", argv[1]);
        std::exit(1);
    }

    // Check the header for proper format and report on the newsgroups
    while (std::getline(article, line))
    {
        line_num++;
        if (line.empty())
        {
            break;
        }
        if (is_hor_space(line.front()))
        {
            continue;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            std::printf("\nERROR: line %d is an invalid header line:\n%s\n", line_num, line.c_str());
            break;
        }
        if (colon + 1 < line.size() && line[colon + 1] != ' ')
        {
            std::printf("\n"
                        "ERROR: header on line %d does not have a space after the colon:\n%s\n",
                        line_num, line.c_str());
        }
        if (colon == 10 && line.compare(0, 10, "Newsgroups") == 0)
        {
            found_newsgroups = 1;
            std::string_view group_names{line.data() + colon + 1, line.size() - colon - 1};
            while (!group_names.empty() && group_names.front() == ' ')
            {
                group_names.remove_prefix(1);
            }
            if (group_names.find(' ') != std::string_view::npos)
            {
                std::printf("\n"
                            "ERROR: the \"Newsgroups:\" line has spaces in it that MUST be removed. The\n"
                            "only allowable space is the one separating the colon (:) from the contents.\n"
                            "Use a comma (,) to separate multiple newsgroup names.\n");
                continue;
            }
            while (!group_names.empty())
            {
                const std::size_t      comma = group_names.find(',');
                const std::string_view group_name =
                    comma == std::string_view::npos ? group_names : group_names.substr(0, comma);
                if (newsgroups.size() < static_cast<std::size_t>(MAX_NGS))
                {
                    ArticleNewsgroup &newsgroup = newsgroups.emplace_back();
                    newsgroup.name.assign(group_name.data(), group_name.size());
                }
                if (comma == std::string_view::npos)
                {
                    break;
                }
                group_names.remove_prefix(comma + 1);
            }
            if (newsgroups.empty())
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
    while (std::getline(article, line))
    {
        line_num++;
        if (article.eof())
        {
            std::printf("\n"
                        "Warning: line %d has no trailing newline character and may get lost.\n",
                        line_num);
        }
        int col = 0;
        for (const char ch : line)
        {
            if (ch == '\t')
            {
                col += 8 - (col % 8);
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
                        max_col_len, line_num, line.c_str());
            break;
        }
    }
    std::string server_name = get_env_var("NNTPSERVER");
    if (server_name.empty())
    {
        server_name = file_exp(SERVER_NAME);
        if (!server_name.empty() && file_ref(server_name))
        {
            server_name = nntp_server_name(server_name);
        }
    }
    if (!server_name.empty() && server_name != "local")
    {
        g_server_name = server_name;
        if (const auto separator = g_server_name.find_first_of(";:"); separator != std::string::npos)
        {
            g_nntp_link.port_number = std::atoi(g_server_name.c_str() + separator + 1);
            g_server_name.resize(separator);
        }
        g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);
        const std::string force_auth = get_env_var("NNTP_FORCE_AUTH");
        if (!force_auth.empty() && (force_auth.front() == 'y' || force_auth.front() == 'Y'))
        {
            g_nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
        if (init_nntp() < 0)
        {
            g_server_name.clear();
        }
    }
    if (!newsgroups.empty())
    {
        if (fs::file_size(newsgroups_file, file_error) > 0 && !file_error)
        {
            newsgroups_input.open(newsgroups_file);
            check_ng = newsgroups_input.is_open();
            if (check_ng)
            {
                newsgroups_input_stream = &newsgroups_input;
            }
        }
        else if (file_error && !g_server_name.empty() && server_connection())
        {
            check_ng = true;
        }
        file_error.clear();
        if (fs::file_size(active_file, file_error) > 0 && !file_error)
        {
            active_input.open(active_file);
            check_active = active_input.is_open();
        }
        else if (file_error && !g_server_name.empty() && server_connection())
        {
            check_active = true;
        }
    }
    if (!newsgroups.empty() && (check_ng || check_active))
    {
        int ngleft;
        // Print a note about each newsgroup
        std::printf("\nYour article's newsgroup%s:\n", plural(static_cast<int>(newsgroups.size())));
        if (!check_active)
        {
            for (ArticleNewsgroup &newsgroup : newsgroups)
            {
                newsgroup.found_active = true;
            }
        }
        else if (active_input.is_open())
        {
            ngleft = static_cast<int>(newsgroups.size());
            while (std::getline(active_input, line))
            {
                if (!ngleft)
                {
                    break;
                }
                for (ArticleNewsgroup &newsgroup : newsgroups)
                {
                    if (!newsgroup.found_active)
                    {
                        const std::size_t name_length = newsgroup.name.size();
                        if (line.size() > name_length //
                            && is_hor_space(line[name_length]) && line.compare(0, name_length, newsgroup.name) == 0)
                        {
                            newsgroup.found_active = true;
                            ngleft--;
                        }
                    }
                }
            }
        }
        else if (!g_server_name.empty())
        {
            int listactive_works = 1;
            for (std::size_t i = 0; i < newsgroups.size(); i++)
            {
                ArticleNewsgroup &newsgroup = newsgroups[i];
                if (listactive_works)
                {
                    if (nntp_command(fmt::format("list active {}", newsgroup.name)) <= 0)
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
                            newsgroup.found_active = true;
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
                    if (nntp_command(fmt::format("GROUP {}", newsgroup.name)) <= 0)
                    {
                        break;
                    }
                    if (nntp_check() > 0)
                    {
                        newsgroup.found_active = true;
                    }
                }
            }
        }
        if (check_ng && newsgroups_input_stream == nullptr)
        {
            generated_newsgroups.open(newsgroups_file, std::ios::in | std::ios::out | std::ios::trunc);
            file_error.clear();
            fs::remove(newsgroups_file, file_error);
            if (generated_newsgroups.is_open())
            {
                for (const ArticleNewsgroup &newsgroup : newsgroups)
                {
                    // issue a description list command
                    if (nntp_command(fmt::format("XGTITLE {}", newsgroup.name)) <= 0)
                    {
                        break;
                    }
                    // TODO: use list newsgroups if this fails...?
                    if (nntp_check() > 0)
                    {
                        while (nntp_gets(g_ser_line, sizeof g_ser_line) >= 0)
                        {
                            if (nntp_at_list_end(g_ser_line))
                            {
                                break;
                            }
                            generated_newsgroups << g_ser_line << '\n';
                        }
                    }
                }
                generated_newsgroups.clear();
                generated_newsgroups.seekg(0L);
                newsgroups_input_stream = &generated_newsgroups;
            }
        }
        if (newsgroups_input_stream != nullptr)
        {
            ngleft = static_cast<int>(newsgroups.size());
            while (std::getline(*newsgroups_input_stream, line))
            {
                if (!ngleft)
                {
                    break;
                }
                for (ArticleNewsgroup &newsgroup : newsgroups)
                {
                    if (newsgroup.found_active && !newsgroup.found_description)
                    {
                        const std::size_t name_length = newsgroup.name.size();
                        if (line.size() > name_length //
                            && is_hor_space(line[name_length]) && line.compare(0, name_length, newsgroup.name) == 0)
                        {
                            std::size_t description_start = name_length;
                            while (description_start < line.size() && is_hor_space(line[description_start]))
                            {
                                description_start++;
                            }
                            std::string description = line.substr(description_start);
                            if (description.size() >= 2 //
                                && description[0] == '?' && description[1] == '?')
                            {
                                description = "[no description available]";
                            }
                            std::printf("%-23s %s\n", newsgroup.name.c_str(), description.c_str());
                            newsgroup.found_description = true;
                            ngleft--;
                        }
                    }
                }
            }
        }
        for (const ArticleNewsgroup &newsgroup : newsgroups)
        {
            if (!newsgroup.found_active)
            {
                std::printf("%-23s ** invalid news group -- check spelling **\n", newsgroup.name.c_str());
            }
            else if (!newsgroup.found_description)
            {
                std::printf("%-23s [no description available]\n", newsgroup.name.c_str());
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
