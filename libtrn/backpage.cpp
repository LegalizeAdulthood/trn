/* backpage.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/fdio.h>

#include "config/common.h"
#include "trn/backpage.h"

#include "trn/final.h"
#include "util/util2.h"

#include <cstdio>

static int     s_varyfd{0};         /* virtual array file for storing  file offsets */
static ArticlePosition s_varybuf[VARYSIZE]; /* current window onto virtual array */
static long    s_oldoffset{-1};     /* offset to block currently in window */

void back_page_init()
{
    const char *varyname = file_exp(VARYNAME);
    close(creat(varyname,0600));
    s_varyfd = open(varyname,2);
    remove(varyname);
    if (s_varyfd < 0)
    {
        std::printf(g_cantopen,varyname);
        sig_catcher(0);
    }

}

/* virtual array read */

ArticlePosition virtual_read(ArticleLine index)
{
    int subindx;
    long offset;

#ifdef DEBUG
    if (indx > maxindx)
    {
        std::printf("vrdary(%ld) > %ld\n",(long)indx, (long)maxindx);
        return 0;
    }
#endif
    if (index < 0)
    {
        return 0;
    }
    subindx = index % VARYSIZE;
    offset = (index - subindx) * sizeof(s_varybuf[0]);
    if (offset != s_oldoffset)
    {
        if (s_oldoffset >= 0)
        {
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
void virtual_write(ArticleLine index, ArticlePosition value)
{
    int subindx;
    long offset;

#ifdef DEBUG
    if (indx < 0)
    {
        std::printf("vwtary(%ld)\n",(long)indx);
    }
    if (!indx)
    {
        maxindx = 0;
    }
    if (indx > maxindx)
    {
        if (indx != maxindx + 1)
        {
            std::printf("indx skipped %d-%d\n",maxindx+1,indx-1);
        }
        maxindx = indx;
    }
#endif
    subindx = index % VARYSIZE;
    offset = (index - subindx) * sizeof(s_varybuf[0]);
    if (offset != s_oldoffset)
    {
        if (s_oldoffset >= 0)
        {
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
    s_varybuf[subindx] = value;
}
