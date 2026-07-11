/* trn/sacmd.h
 *
 * main command loop
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SACMD_H
#define TRN_SACMD_H

enum SaCommand
{
    SA_KILL = 1,
    SA_MARK = 2,
    SA_SELECT = 3,
    SA_KILL_UNMARKED = 4,
    SA_KILL_MARKED = 5,
    SA_EXTRACT = 6
};

int sa_do_cmd();

#endif
