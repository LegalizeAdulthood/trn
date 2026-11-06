/* SourceCheckpoint.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/SourceCheckpoint.h>

#include <trn/last.h>

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
