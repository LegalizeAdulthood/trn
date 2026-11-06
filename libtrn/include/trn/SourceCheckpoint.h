/* trn/SourceCheckpoint.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SOURCE_CHECKPOINT_H
#define TRN_SOURCE_CHECKPOINT_H

struct Newsrc;

struct SourceCheckpoint
{
    long new_time;
    long active_cnt;
    long desc_cnt;
};

struct NewsrcCheckpoint
{
    Newsrc &rc;
};

#endif
