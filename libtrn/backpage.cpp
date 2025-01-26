/* backpage.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <fdio.h>

#include "common.h"
#include "backpage.h"

#include "final.h"
#include "util2.h"


static int     s_varyfd{0};         /* virtual array file for storing  file offsets */
static ART_POS s_varybuf[VARYSIZE]; /* current window onto virtual array */
static long    s_oldoffset{-1};     /* offset to block currently in window */

void backpage_init()
{
    const char *varyname = filexp(VARYNAME);
    close(creat(varyname,0600));
    s_varyfd = open(varyname,2);
    remove(varyname);
    if (s_varyfd < 0) {
        printf(g_cantopen,varyname);
        sig_catcher(0);
    }

}

/* virtual array read */

ART_POS vrdary(ART_LINE indx)
{
    int subindx;
    long offset;

#ifdef DEBUG
    if (indx > maxindx) {
        printf("vrdary(%ld) > %ld\n",(long)indx, (long)maxindx);
        return 0;
    }
#endif
    if (indx < 0)
        return 0;
    subindx = indx % VARYSIZE;
    offset = (indx - subindx) * sizeof(s_varybuf[0]);
    if (offset != s_oldoffset) {
        if (s_oldoffset >= 0) {
#ifndef lint
            (void)lseek(s_varyfd,s_oldoffset,0);
            write(s_varyfd, (char*)s_varybuf,sizeof(s_varybuf));
#endif /* lint */
        }
#ifndef lint
        (void)lseek(s_varyfd,offset,0);
        read(s_varyfd,(char*)s_varybuf,sizeof(s_varybuf));
#endif /* lint */
        s_oldoffset = offset;
    }
    return s_varybuf[subindx];
}

/* write to virtual array */
void vwtary(ART_LINE indx, ART_POS newvalue)
{
    int subindx;
    long offset;

#ifdef DEBUG
    if (indx < 0)
        printf("vwtary(%ld)\n",(long)indx);
    if (!indx)
        maxindx = 0;
    if (indx > maxindx) {
        if (indx != maxindx + 1)
            printf("indx skipped %d-%d\n",maxindx+1,indx-1);
        maxindx = indx;
    }
#endif
    subindx = indx % VARYSIZE;
    offset = (indx - subindx) * sizeof(s_varybuf[0]);
    if (offset != s_oldoffset) {
        if (s_oldoffset >= 0) {
#ifndef lint
            (void)lseek(s_varyfd,s_oldoffset,0);
            write(s_varyfd,(char*)s_varybuf,sizeof(s_varybuf));
#endif /* lint */
        }
#ifndef lint
        (void)lseek(s_varyfd,offset,0);
        read(s_varyfd,(char*)s_varybuf,sizeof(s_varybuf));
#endif /* lint */
        s_oldoffset = offset;
    }
    s_varybuf[subindx] = newvalue;
}
