/* last.c
 */
// This software is copyrighted as detailed in the LICENSE file.

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

std::string g_last_newsgroup_name; // last newsgroup read
long        g_last_time{};         // time last we ran
long        g_last_active_size{};  // last known size of active file
long        g_last_new_time{};     // time of last newgroup request
long        g_last_extra_num{};

static char *s_last_file{}; // path name of .rnlast file
static long  s_start_time{};

void last_init()
{
    s_last_file = save_str(file_exp(LASTNAME));

    s_start_time = (long)std::time(nullptr);
    read_last();
}

void last_final()
{
    safe_free0(s_last_file);
    g_last_newsgroup_name.clear();
}

void read_last()
{
    if (std::FILE *fp = std::fopen(s_last_file, "r"))
    {
        if (std::fgets(g_buf, sizeof g_buf, fp) != nullptr)
        {
            long old_last = g_last_time;
            g_buf[std::strlen(g_buf)-1] = '\0';
            if (*g_buf)
            {
                g_last_newsgroup_name = g_buf;
            }
            std::fscanf(fp,"%ld %ld %ld %ld",&g_last_time,&g_last_active_size,
                                           &g_last_new_time,&g_last_extra_num);
            if (!g_last_new_time)
            {
                g_last_new_time = s_start_time;
            }
            g_last_time = std::max(old_last, g_last_time);
        }
        std::fclose(fp);
    }
}

// Put out certain values for next run of trn

void write_last()
{
    std::sprintf(g_buf,"%s.%ld", s_last_file, g_our_pid);
    if (std::FILE *fp = std::fopen(g_buf, "w"))
    {
        g_last_time = std::max(g_last_time, s_start_time);
        std::fprintf(fp,"%s\n%ld\n%ld\n%ld\n%ld\n",
                g_newsgroup_name.c_str(),g_last_time,
                g_last_active_size,g_last_new_time,g_last_extra_num);
        std::fclose(fp);
        remove(s_last_file);
        rename(g_buf,s_last_file);
    }
    else
    {
        std::printf(g_cant_create,g_buf);
        // term_down(1);
    }
}
