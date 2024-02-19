/* last.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_LAST_H
#define TRN_LAST_H

#include <string>

extern std::string g_lastngname;  /* last newsgroup read */
extern long        g_lasttime;    /* time last we ran */
extern long        g_lastactsiz;  /* last known size of active file */
extern long        g_lastnewtime; /* time of last newgroup request */
extern long        g_lastextranum;

void last_init();
void last_final();
void readlast();
void writelast();

#endif
