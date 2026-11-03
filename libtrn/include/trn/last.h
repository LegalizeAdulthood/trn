/* trn/last.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_LAST_H
#define TRN_LAST_H

#include <string>

struct LastRunState
{
    void read();
    void write();

    std::string newsgroup_name; // last newsgroup read
    long        time{};         // time last we ran
    long        active_size{};  // last known size of active file
    long        new_time{};     // time of last newgroup request
    long        extra_num{};
    std::string file; // path name of .rnlast file
    long        start_time{};
};

extern LastRunState g_last_run;

void last_init();
void last_final();

#endif
