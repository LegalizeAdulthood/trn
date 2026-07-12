/* last.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/last.h>

#include <config/common.h>
#include <trn/init.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

std::string g_last_newsgroup_name; // last newsgroup read
long        g_last_time{};         // time last we ran
long        g_last_active_size{};  // last known size of active file
long        g_last_new_time{};     // time of last newgroup request
long        g_last_extra_num{};

static std::string s_last_file; // path name of .rnlast file
static long  s_start_time{};

void last_init()
{
    s_last_file = file_exp(LASTNAME);

    s_start_time = (long)std::time(nullptr);
    read_last();
}

void last_final()
{
    s_last_file.clear();
    g_last_newsgroup_name.clear();
}

void read_last()
{
    if (std::FILE *fp = std::fopen(s_last_file.c_str(), "r"))
    {
        if (std::fgets(g_buf, sizeof g_buf, fp) != nullptr)
        {
            long old_last = g_last_time;
            g_buf[std::strlen(g_buf)-1] = '\0';
            if (*g_buf)
            {
                g_last_newsgroup_name = g_buf;
            }
            std::fscanf(fp,"%ld %ld %ld %ld",&g_last_time,&g_last_active_size,
                                           &g_last_new_time,&g_last_extra_num);
            if (!g_last_new_time)
            {
                g_last_new_time = s_start_time;
            }
            g_last_time = std::max(old_last, g_last_time);
        }
        std::fclose(fp);
    }
}

// Put out certain values for next run of trn

void write_last()
{
    fs::path temp_file{fmt::format("{}.{}", s_last_file, g_our_pid)};
    if (std::FILE *fp = std::fopen(temp_file.string().c_str(), "w"))
    {
        g_last_time = std::max(g_last_time, s_start_time);
        fmt::print(fp, "{}\n{}\n{}\n{}\n{}\n", g_newsgroup_name, g_last_time, g_last_active_size, g_last_new_time,
                   g_last_extra_num);
        std::fclose(fp);
        std::error_code error;
        fs::remove(s_last_file, error);
        fs::rename(temp_file, s_last_file, error);
    }
    else
    {
        std::printf(g_cant_create, temp_file.string().c_str());
        // term_down(1);
    }
}
