/* trn/NewsrcCheckpoint.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NEWSRC_CHECKPOINT_H
#define TRN_NEWSRC_CHECKPOINT_H

struct Newsrc;

struct NewsrcCheckpoint
{
    void restore();
    void save();

    Newsrc &rc;
};

#endif
