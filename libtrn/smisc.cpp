/* This file is Copyright 1993 by Clifford A. Adams */
/* smisc.c
 *
 * Lots of misc. stuff.
 */

#include "config/common.h"
#include "trn/smisc.h"

#include "trn/sadesc.h"
#include "trn/samisc.h"
#include "trn/scan.h"

#include <cstdio>

bool g_s_default_cmd{}; /* true if the last command (run through setdef()) was the default */
bool g_s_follow_temp{}; /* explicitly follow until end of thread */

bool s_eligible(long ent)
{
    switch (g_s_cur_type)
    {
      case S_ART:
        return sa_eligible(ent);
      default:
        std::printf("s_eligible: current type is bad!\n");
        return false;
    }
}

void s_beep()
{
    std::putchar(7);
    std::fflush(stdout);
}

const char *s_get_statchars(long ent, int line)
{
    if (g_s_status_cols == 0)
    {
        return "";
    }
    switch (g_s_cur_type)
    {
      case S_ART:
        return sa_get_statchars(ent,line);
      default:
        return nullptr;
    }
}

const char *s_get_desc(long ent, int line, bool trunc)
{
    switch (g_s_cur_type)
    {
      case S_ART:
        return sa_get_desc(ent,line,trunc);
      default:
        return nullptr;
    }
}

int s_ent_lines(long ent)
{
    switch (g_s_cur_type)
    {
      case S_ART:
        return sa_ent_lines(ent);
      default:
        return 1;
    }
}
