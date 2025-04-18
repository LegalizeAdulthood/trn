/* backpage.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/fdio.h>

#include "config/common.h"
#include "trn/backpage.h"

#include "trn/final.h"
#include "util/util2.h"

#include <cstdio>

static int             s_vary_fd{0};         /* virtual array file for storing  file offsets */
static ArticlePosition s_vary_buf[VARY_SIZE]; /* current window onto virtual array */
static long            s_old_offset{-1};     /* offset to block currently in window */

void back_page_init()
{
    const char *varyname = file_exp(VARYNAME);
    close(creat(varyname,0600));
    s_vary_fd = open(varyname,2);
    remove(varyname);
    if (s_vary_fd < 0)
    {
        std::printf(g_cant_open,varyname);
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
    subindx = index % VARY_SIZE;
    offset = (index - subindx) * sizeof(s_vary_buf[0]);
    if (offset != s_old_offset)
    {
        if (s_old_offset >= 0)
        {
#ifndef lint
            (void)lseek(s_vary_fd,s_old_offset,0);
            write(s_vary_fd, (char*)s_vary_buf,sizeof(s_vary_buf));
#endif /* lint */
        }
#ifndef lint
        (void)lseek(s_vary_fd,offset,0);
        read(s_vary_fd,(char*)s_vary_buf,sizeof(s_vary_buf));
#endif /* lint */
        s_old_offset = offset;
    }
    return s_vary_buf[subindx];
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
    subindx = index % VARY_SIZE;
    offset = (index - subindx) * sizeof(s_vary_buf[0]);
    if (offset != s_old_offset)
    {
        if (s_old_offset >= 0)
        {
#ifndef lint
            (void)lseek(s_vary_fd,s_old_offset,0);
            write(s_vary_fd,(char*)s_vary_buf,sizeof(s_vary_buf));
#endif /* lint */
        }
#ifndef lint
        (void)lseek(s_vary_fd,offset,0);
        read(s_vary_fd,(char*)s_vary_buf,sizeof(s_vary_buf));
#endif /* lint */
        s_old_offset = offset;
    }
    s_vary_buf[subindx] = value;
}
