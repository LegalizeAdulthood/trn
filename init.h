/* init.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

enum
{
    TCBUF_SIZE = 1024
};

EXT long our_pid;
/* default string for group entry */
#if 0
EXT char *group_default INIT("");
#endif

bool initialize(int, char **);
void newsnews_check();
