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
#include <ctime>
#include <filesystem>
#include <fstream>
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
    std::ifstream input{s_last_file};
    std::string   last_newsgroup_name;
    if (std::getline(input, last_newsgroup_name))
    {
        long old_last = g_last_time;
        if (!last_newsgroup_name.empty())
        {
            g_last_newsgroup_name = last_newsgroup_name;
        }
        input >> g_last_time >> g_last_active_size >> g_last_new_time >> g_last_extra_num;
        if (!g_last_new_time)
        {
            g_last_new_time = s_start_time;
        }
        g_last_time = std::max(old_last, g_last_time);
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
        fmt::print("Can't create {}\n", temp_file.string());
        // term_down(1);
    }
}
