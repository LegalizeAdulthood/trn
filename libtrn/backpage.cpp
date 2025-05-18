/* backpage.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/fdio.h>

#include "config/common.h"
#include "trn/backpage.h"

#include "trn/final.h"
#include "util/util2.h"

#include <cstdio>

static int             s_vary_fd{0};         // virtual array file for storing  file offsets
static ArticlePosition s_vary_buf[VARY_SIZE]; // current window onto virtual array
static long            s_old_offset{-1};     // offset to block currently in window
#ifdef DEBUG
static int             s_max_index{-1};
#endif

void back_page_init()
{
    const char *vary_name = file_exp(VARYNAME);
    close(creat(vary_name,0600));
    s_vary_fd = open(vary_name,2);
    remove(vary_name);
    if (s_vary_fd < 0)
    {
        std::printf(g_cant_open,vary_name);
        sig_catcher(0);
    }

}

// virtual array read

ArticlePosition virtual_read(ArticleLine index)
{
    int sub_index;
    long offset;

#ifdef DEBUG
    if (index > s_max_index)
    {
        std::printf("vrdary(%ld) > %ld\n",(long)index.value_of(), (long)s_max_index);
        return ArticlePosition{};
    }
#endif
    if (index < 0)
    {
        return ArticlePosition{};
    }
    sub_index = index.value_of() % VARY_SIZE;
    offset = (index.value_of() - sub_index) * sizeof(s_vary_buf[0]);
    if (offset != s_old_offset)
    {
        if (s_old_offset >= 0)
        {
#ifndef lint
            (void)lseek(s_vary_fd,s_old_offset,0);
            write(s_vary_fd, (char*)s_vary_buf,sizeof(s_vary_buf));
#endif // lint
        }
#ifndef lint
        (void)lseek(s_vary_fd,offset,0);
        read(s_vary_fd,(char*)s_vary_buf,sizeof(s_vary_buf));
#endif // lint
        s_old_offset = offset;
    }
    return s_vary_buf[sub_index];
}

// write to virtual array
void virtual_write(ArticleLine index, ArticlePosition value)
{
    int sub_index;
    long offset;

#ifdef DEBUG
    if (index < 0)
    {
        std::printf("vwtary(%ld)\n",(long)index.value_of());
    }
    if (!index)
    {
        s_max_index = 0;
    }
    if (index > s_max_index)
    {
        if (index != s_max_index + 1)
        {
            std::printf("index skipped %d-%d\n",s_max_index+1,index.value_of()-1);
        }
        s_max_index = index.value_of();
    }
#endif
    sub_index = index.value_of() % VARY_SIZE;
    offset = (index.value_of() - sub_index) * sizeof(s_vary_buf[0]);
    if (offset != s_old_offset)
    {
        if (s_old_offset >= 0)
        {
#ifndef lint
            (void)lseek(s_vary_fd,s_old_offset,0);
            write(s_vary_fd,(char*)s_vary_buf,sizeof(s_vary_buf));
#endif // lint
        }
#ifndef lint
        (void)lseek(s_vary_fd,offset,0);
        read(s_vary_fd,(char*)s_vary_buf,sizeof(s_vary_buf));
#endif // lint
        s_old_offset = offset;
    }
    s_vary_buf[sub_index] = value;
}
