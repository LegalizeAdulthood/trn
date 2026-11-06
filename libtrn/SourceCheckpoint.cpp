/* SourceCheckpoint.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/SourceCheckpoint.h>

#include <config/common.h>
#include <trn/datasrc.h>
#include <trn/last.h>
#include <trn/rcstuff.h>
#include <trn/trn.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <cstdio>

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

void NewsrcCheckpoint::restore()
{
    if (!rc.info_name.empty())
    {
        std::FILE *info = std::fopen(rc.info_name.string().c_str(), "r");
        if (info != nullptr)
        {
            const std::string info_line = get_a_line(info);
            if (!info_line.empty())
            {
                long             actnum;
                long             descnum;
                std::string_view last_group_line{info_line};
                if (last_group_line.back() == '\n')
                {
                    last_group_line.remove_suffix(1);
                }
                const std::size_t separator = last_group_line.find(':');
                if (separator != std::string_view::npos && separator + 2 < last_group_line.size() &&
                    last_group_line[separator + 1] == ' ')
                {
                    g_last_run.newsgroup_name = last_group_line.substr(separator + 2);
                }
                if (std::fscanf(info, "New-Group-State: %ld,%ld,%ld", //
                                &g_last_run.new_time, &actnum, &descnum) == 3)
                {
                    rc.data_source->m_act_sf.m_recent_cnt = actnum;
                    rc.data_source->m_desc_sf.m_recent_cnt = descnum;
                }
            }
            std::fclose(info);
        }
        rc.data_source->m_last_new_group = g_last_run.new_time;
    }
    else
    {
        g_last_run.read();
        SourceCheckpoint checkpoint{};
        checkpoint.restore_legacy(g_last_run, (rc.data_source->m_flags & DF_REMOTE) != 0);
        rc.data_source->m_act_sf.m_recent_cnt = checkpoint.active_cnt;
        rc.data_source->m_desc_sf.m_recent_cnt = checkpoint.desc_cnt;
        rc.data_source->m_last_new_group = checkpoint.new_time;
    }
}

void NewsrcCheckpoint::save()
{
    if (!rc.info_name.empty())
    {
        std::FILE *info = std::fopen(rc.info_name.string().c_str(), "w");
        if (info != nullptr)
        {
            const SourceCheckpoint checkpoint{rc.data_source->m_last_new_group, rc.data_source->m_act_sf.m_recent_cnt,
                                              rc.data_source->m_desc_sf.m_recent_cnt};
            fmt::print(info, "{}", checkpoint.format_info(g_newsgroup_name));
            std::fclose(info);
        }
    }
    else
    {
        g_last_run.read();
        const SourceCheckpoint checkpoint{rc.data_source->m_last_new_group, rc.data_source->m_act_sf.m_recent_cnt,
                                          rc.data_source->m_desc_sf.m_recent_cnt};
        checkpoint.update_legacy(g_last_run, (rc.data_source->m_flags & DF_REMOTE) != 0);
        g_last_run.write();
    }
}
