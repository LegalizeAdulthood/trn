/* init.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXTERN_C_BEGIN

#define TCBUF_SIZE 1024

EXT long our_pid;
/* default string for group entry */
#if 0
EXT char *group_default INIT(nullstr);
#endif

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

bool initialize _((int,char**));
void newsnews_check _((void));

EXTERN_C_END
