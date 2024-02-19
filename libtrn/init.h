/* init.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_INIT_H
#define TRN_INIT_H

enum
{
    TCBUF_SIZE = 1024
};

extern long g_our_pid;

bool initialize(int argc, char *argv[]);
void newsnews_check();

#endif
