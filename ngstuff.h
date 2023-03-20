/* ngstuff.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#define NN_NORM 0
#define NN_INP 1
#define NN_REREAD 2
#define NN_ASK 3

EXT bool one_command INIT(false);	/* no ':' processing in perform() */

/* CAA: given the new and complex universal/help possibilities,
 *      the following interlock variable may save some trouble.
 *      (if true, we are currently processing options)
 */
EXT bool option_sel_ilock INIT(false);

void ngstuff_init();
int escapade();
int switcheroo();
int numnum();
int thread_perform();
int perform(char *cmdlst, int output_level);
int ngsel_perform();
int ng_perform(char *cmdlst, int output_level);
int addgrp_sel_perform();
int addgrp_perform(ADDGROUP *gp, char *cmdlst, int output_level);
