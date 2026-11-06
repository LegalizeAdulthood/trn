/* SourceCheckpoint.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/SourceCheckpoint.h>

#include <trn/last.h>

#include <fmt/format.h>

void SourceCheckpoint::restore_legacy(const LastRunState &last, bool remote)
{
    new_time = last.new_time;
    if (remote)
    {
        active_cnt = last.active_size;
        desc_cnt = last.extra_num;
    }
    else
    {
        active_cnt = last.extra_num;
        desc_cnt = 0;
    }
}

void SourceCheckpoint::update_legacy(LastRunState &last, bool remote) const
{
    if (remote)
    {
        last.active_size = active_cnt;
        last.extra_num = desc_cnt;
    }
    else
    {
        last.extra_num = active_cnt;
    }
    last.new_time = new_time;
}

std::string SourceCheckpoint::format_info(std::string_view group) const
{
    return fmt::format("Last-Group: {}\nNew-Group-State: {},{},{}\n", group, new_time, active_cnt, desc_cnt);
}
