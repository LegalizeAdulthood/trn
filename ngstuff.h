/* ngstuff.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NGSTUFF_H
#define TRN_NGSTUFF_H

struct ADDGROUP;

enum numnum_result
{
    NN_NORM = 0,
    NN_INP,
    NN_REREAD,
    NN_ASK
};

extern bool g_one_command; /* no ':' processing in perform() */

/* CAA: given the new and complex universal/help possibilities,
 *      the following interlock variable may save some trouble.
 *      (if true, we are currently processing options)
 */
extern bool g_option_sel_ilock;

void ngstuff_init();
int escapade();
int switcheroo();
numnum_result numnum();
int thread_perform();
int perform(char *cmdlst, int output_level);
int ngsel_perform();
int ng_perform(char *cmdlst, int output_level);
int addgrp_sel_perform();
int addgrp_perform(ADDGROUP *gp, char *cmdlst, int output_level);

#endif
