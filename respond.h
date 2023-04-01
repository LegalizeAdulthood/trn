/* respond.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RESPOND_H
#define TRN_RESPOND_H

#include <string>

extern std::string g_savedest;    /* value of %b */
extern char *g_extractdest; /* value of %E */
extern char *g_extractprog; /* value of %e */
extern ART_POS g_savefrom;  /* value of %B */

enum save_result
{
    SAVE_ABORT = 0,
    SAVE_DONE = 1
};

void respond_init();
save_result save_article();
save_result view_article();
int cancel_article();
int supersede_article();
void reply();
void forward();
void followup();
int invoke(const char *cmd, const char *dir);

#endif
