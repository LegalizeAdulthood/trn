/* trn/ngstuff.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
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

extern bool        g_one_command; /* no ':' processing in perform() */
extern std::string g_savedir;     /* -d */

void ngstuff_init();
int escapade();
int switcheroo();
NumNumResult numnum();
int thread_perform();
int perform(char *cmdlst, int output_level);
int ngsel_perform();
int ng_perform(char *cmdlst, int output_level);
int addgrp_sel_perform();
int addgrp_perform(AddGroup *gp, char *cmdlst, int output_level);

#endif
