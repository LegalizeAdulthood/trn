/* last.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/last.h"

#include "trn/init.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

std::string g_lastngname;    /* last newsgroup read */
long        g_lasttime{};    /* time last we ran */
long        g_lastactsiz{};  /* last known size of active file */
long        g_lastnewtime{}; /* time of last newgroup request */
long        g_lastextranum{};

static char *s_lastfile{}; /* path name of .rnlast file */
static long  s_starttime{};

void last_init()
{
    s_lastfile = savestr(filexp(LASTNAME));

    s_starttime = (long)std::time(nullptr);
    readlast();
}

void last_final()
{
    safefree0(s_lastfile);
    g_lastngname.clear();
}

void readlast()
{
    if (std::FILE *fp = std::fopen(s_lastfile, "r"))
    {
        if (std::fgets(g_buf, sizeof g_buf, fp) != nullptr)
        {
            long old_last = g_lasttime;
            g_buf[std::strlen(g_buf)-1] = '\0';
            if (*g_buf)
            {
                g_lastngname = g_buf;
            }
            std::fscanf(fp,"%ld %ld %ld %ld",&g_lasttime,&g_lastactsiz,
                                           &g_lastnewtime,&g_lastextranum);
            if (!g_lastnewtime)
            {
                g_lastnewtime = s_starttime;
            }
            g_lasttime = std::max(old_last, g_lasttime);
        }
        std::fclose(fp);
    }
}

/* Put out certain values for next run of trn */

void writelast()
{
    std::sprintf(g_buf,"%s.%ld", s_lastfile, g_our_pid);
    if (std::FILE *fp = std::fopen(g_buf, "w"))
    {
        g_lasttime = std::max(g_lasttime, s_starttime);
        std::fprintf(fp,"%s\n%ld\n%ld\n%ld\n%ld\n",
                g_ngname.c_str(),g_lasttime,
                g_lastactsiz,g_lastnewtime,g_lastextranum);
        std::fclose(fp);
        remove(s_lastfile);
        rename(g_buf,s_lastfile);
    }
    else
    {
        std::printf(g_cantcreate,g_buf);
        /*termdown(1);*/
    }
}
