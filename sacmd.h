/* This file Copyright 1992 by Clifford A. Adams */
/* sacmd.h
 *
 * main command loop
 */
#ifndef TRN_SACMD_H
#define TRN_SACMD_H

enum sa_cmd
{
    SA_KILL = 1,
    SA_MARK = 2,
    SA_SELECT = 3,
    SA_KILL_UNMARKED = 4,
    SA_KILL_MARKED = 5,
    SA_EXTRACT = 6
};

int sa_docmd();
bool sa_extract_start();
void sa_art_cmd_prim(sa_cmd cmd, long a);
int sa_art_cmd(int multiple, sa_cmd cmd, long a);
long sa_wrap_next_author(long a);

#endif
