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

LastRunState g_last_run{};

void last_init()
{
    g_last_run.file = file_exp(LASTNAME);

    g_last_run.start_time = (long) std::time(nullptr);
    read_last();
}

void last_final()
{
    g_last_run.file.clear();
    g_last_run.newsgroup_name.clear();
}

void read_last()
{
    std::ifstream input{g_last_run.file};
    std::string   last_newsgroup_name;
    if (std::getline(input, last_newsgroup_name))
    {
        long old_last = g_last_run.time;
        if (!last_newsgroup_name.empty())
        {
            g_last_run.newsgroup_name = last_newsgroup_name;
        }
        input >> g_last_run.time >> g_last_run.active_size >> g_last_run.new_time >> g_last_run.extra_num;
        if (!g_last_run.new_time)
        {
            g_last_run.new_time = g_last_run.start_time;
        }
        g_last_run.time = std::max(old_last, g_last_run.time);
    }
}

// Put out certain values for next run of trn

void write_last()
{
    fs::path temp_file{fmt::format("{}.{}", g_last_run.file, g_our_pid)};
    if (std::FILE *fp = std::fopen(temp_file.string().c_str(), "w"))
    {
        g_last_run.time = std::max(g_last_run.time, g_last_run.start_time);
        fmt::print(fp, "{}\n{}\n{}\n{}\n{}\n", g_newsgroup_name, g_last_run.time, g_last_run.active_size,
                   g_last_run.new_time, g_last_run.extra_num);
        std::fclose(fp);
        std::error_code error;
        fs::remove(g_last_run.file, error);
        fs::rename(temp_file, g_last_run.file, error);
    }
    else
    {
        fmt::print("Can't create {}\n", temp_file.string());
        // term_down(1);
    }
}
