/* trn/last.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_LAST_H
#define TRN_LAST_H

#include <string>

extern std::string g_last_newsgroup_name; /* last newsgroup read */
extern long        g_last_time;           /* time last we ran */
extern long        g_last_active_size;    /* last known size of active file */
extern long        g_last_new_time;       /* time of last newgroup request */
extern long        g_last_extra_num;

void last_init();
void last_final();
void read_last();
void write_last();

#endif
