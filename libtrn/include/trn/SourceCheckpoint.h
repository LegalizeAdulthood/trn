/* trn/SourceCheckpoint.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SOURCE_CHECKPOINT_H
#define TRN_SOURCE_CHECKPOINT_H

#include <string>
#include <string_view>

struct LastRunState;
struct Newsrc;

struct SourceCheckpoint
{
    void        restore_legacy(const LastRunState &last, bool remote);
    void        update_legacy(LastRunState &last, bool remote) const;
    std::string format_info(std::string_view group) const;

    long new_time;
    long active_cnt;
    long desc_cnt;
};

struct NewsrcCheckpoint
{
    Newsrc &rc;
};

#endif
