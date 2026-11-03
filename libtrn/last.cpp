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

void LastRunState::init()
{
    file = file_exp(LASTNAME);

    start_time = (long) std::time(nullptr);
    read();
}

void LastRunState::final()
{
    file.clear();
    newsgroup_name.clear();
}

void LastRunState::read()
{
    std::ifstream input{file};
    std::string   last_newsgroup_name;
    if (std::getline(input, last_newsgroup_name))
    {
        long old_last = time;
        if (!last_newsgroup_name.empty())
        {
            newsgroup_name = last_newsgroup_name;
        }
        input >> time >> active_size >> new_time >> extra_num;
        if (!new_time)
        {
            new_time = start_time;
        }
        time = std::max(old_last, time);
    }
}

// Put out certain values for next run of trn

void LastRunState::write()
{
    fs::path temp_file{fmt::format("{}.{}", file, g_our_pid)};
    if (std::FILE *fp = std::fopen(temp_file.string().c_str(), "w"))
    {
        time = std::max(time, start_time);
        fmt::print(fp, "{}\n{}\n{}\n{}\n{}\n", g_newsgroup_name, time, active_size, new_time, extra_num);
        std::fclose(fp);
        std::error_code error;
        fs::remove(file, error);
        fs::rename(temp_file, file, error);
    }
    else
    {
        fmt::print("Can't create {}\n", temp_file.string());
        // term_down(1);
    }
}
