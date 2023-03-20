/* init.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

enum
{
    TCBUF_SIZE = 1024
};

EXT long our_pid;

bool initialize(int argc, char *argv[]);
void newsnews_check();
