/* last.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "last.h"

#include "init.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

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

    s_starttime = (long)time((time_t*)nullptr);
    readlast();
}

void last_final()
{
    safefree0(s_lastfile);
    g_lastngname.clear();
}

void readlast()
{
    if (FILE *fp = fopen(s_lastfile, "r"))
    {
        if (fgets(g_buf,sizeof g_buf,fp) != nullptr) {
            long old_last = g_lasttime;
            g_buf[strlen(g_buf)-1] = '\0';
            if (*g_buf) {
                g_lastngname = g_buf;
            }
            fscanf(fp,"%ld %ld %ld %ld",&g_lasttime,&g_lastactsiz,
                                           &g_lastnewtime,&g_lastextranum);
            if (!g_lastnewtime)
                g_lastnewtime = s_starttime;
            if (old_last > g_lasttime)
                g_lasttime = old_last;
        }
        fclose(fp);
    }
}

/* Put out certain values for next run of trn */

void writelast()
{
    sprintf(g_buf,"%s.%ld", s_lastfile, g_our_pid);
    if (FILE *fp = fopen(g_buf, "w"))
    {
        if (g_lasttime < s_starttime)
            g_lasttime = s_starttime;
        fprintf(fp,"%s\n%ld\n%ld\n%ld\n%ld\n",
                g_ngname.c_str(),g_lasttime,
                g_lastactsiz,g_lastnewtime,g_lastextranum);
        fclose(fp);
        remove(s_lastfile);
        rename(g_buf,s_lastfile);
    }
    else {
        printf(g_cantcreate,g_buf) FLUSH;
        /*termdown(1);*/
    }
}
