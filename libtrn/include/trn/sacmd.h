// This file Copyright 1992 by Clifford A. Adams
/* trn/sacmd.h
 *
 * main command loop
 */
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
bool sa_extract_start();
void sa_art_cmd_prim(SaCommand cmd, long a);
int sa_art_cmd(int multiple, SaCommand cmd, long a);
long sa_wrap_next_author(long a);

#endif
