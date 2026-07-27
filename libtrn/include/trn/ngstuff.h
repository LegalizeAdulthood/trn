/* trn/ngstuff.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NGSTUFF_H
#define TRN_NGSTUFF_H

#include <string>
#include <string_view>

struct AddGroup;

enum NumNumResult
{
    NN_NORM = 0,
    NN_INP,
    NN_REREAD,
    NN_ASK
};

extern bool        g_one_command; // no ':' processing in perform()
extern std::string g_save_dir;    // -d

void         newsgroup_stuff_init();
bool         escapade();
bool         escapade(std::string_view command);
bool         switcheroo();
bool         switcheroo(std::string_view command);
NumNumResult num_num(std::string_view command);
int thread_perform(std::string_view command);
int perform(std::string_view cmdlst, int output_level);
int newsgroup_sel_perform(std::string_view command);
int newsgroup_perform(std::string_view cmdlst, int output_level);
int add_group_sel_perform(std::string_view command);

#endif
