/* trn/ngstuff.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_NGSTUFF_H
#define TRN_NGSTUFF_H

#include <string>

struct AddGroup;

enum NumNumResult
{
    NN_NORM = 0,
    NN_INP,
    NN_REREAD,
    NN_ASK
};

extern bool        g_one_command; // no ':' processing in perform()
extern std::string g_save_dir;     // -d

void         newsgroup_stuff_init();
bool         escapade();
bool         switcheroo();
NumNumResult num_num();
int thread_perform();
int perform(char *cmdlst, int output_level);
int newsgroup_sel_perform();
int newsgroup_perform(char *cmdlst, int output_level);
int add_group_sel_perform();
int add_group_perform(AddGroup *gp, char *cmdlst, int output_level);

#endif
