/* backpage.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/backpage.h>

#include <config/common.h>
#include <config/fdio.h>
#include <trn/final.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

BackPageCache g_back_page;

void BackPageCache::init()
{
    final();
    const std::string vary_name = file_exp(VARYNAME);
    close(creat(vary_name.c_str(), 0600));
    int open_flags = O_RDWR;
#ifdef _WIN32
    open_flags |= _O_BINARY;
#endif
    vary_fd = open(vary_name.c_str(), open_flags);
    std::error_code error;
    fs::remove(vary_name, error);
    if (vary_fd < 0)
    {
        fmt::print("Can't open {}\n", vary_name);
        sig_catcher(0);
    }
}

void BackPageCache::final()
{
    if (vary_fd >= 0)
    {
        close(vary_fd);
        vary_fd = -1;
    }
    old_offset = -1;
    std::fill_n(vary_buf, VARY_SIZE, ArticlePosition{});
#ifdef DEBUG
    max_index = -1;
#endif
}

// virtual array read

ArticlePosition virtual_read(ArticleLine index)
{
    int  sub_index;
    long offset;

#ifdef DEBUG
    if (index > g_back_page.max_index)
    {
        fmt::print("vrdary({}) > {}\n", index.value_of(), g_back_page.max_index);
        return ArticlePosition{};
    }
#endif
    if (index < 0)
    {
        return ArticlePosition{};
    }
    sub_index = index.value_of() % VARY_SIZE;
    offset = (index.value_of() - sub_index) * sizeof(g_back_page.vary_buf[0]);
    if (offset != g_back_page.old_offset)
    {
        if (g_back_page.old_offset >= 0)
        {
#ifndef lint
            (void) lseek(g_back_page.vary_fd, g_back_page.old_offset, 0);
            write(g_back_page.vary_fd, (char *) g_back_page.vary_buf, sizeof(g_back_page.vary_buf));
#endif // lint
        }
#ifndef lint
        (void) lseek(g_back_page.vary_fd, offset, 0);
        read(g_back_page.vary_fd, (char *) g_back_page.vary_buf, sizeof(g_back_page.vary_buf));
#endif // lint
        g_back_page.old_offset = offset;
    }
    return g_back_page.vary_buf[sub_index];
}

// write to virtual array
void virtual_write(ArticleLine index, ArticlePosition value)
{
    int  sub_index;
    long offset;

#ifdef DEBUG
    if (index < 0)
    {
        fmt::print("vwtary({})\n", index.value_of());
    }
    if (!index && value >= 0)
    {
        g_back_page.max_index = 0;
    }
    if (index > g_back_page.max_index)
    {
        if (index != g_back_page.max_index + 1)
        {
            fmt::print("index skipped {}-{}\n", g_back_page.max_index + 1, index.value_of() - 1);
        }
        g_back_page.max_index = index.value_of();
    }
#endif
    sub_index = index.value_of() % VARY_SIZE;
    offset = (index.value_of() - sub_index) * sizeof(g_back_page.vary_buf[0]);
    if (offset != g_back_page.old_offset)
    {
        if (g_back_page.old_offset >= 0)
        {
#ifndef lint
            (void) lseek(g_back_page.vary_fd, g_back_page.old_offset, 0);
            write(g_back_page.vary_fd, (char *) g_back_page.vary_buf, sizeof(g_back_page.vary_buf));
#endif // lint
        }
#ifndef lint
        (void) lseek(g_back_page.vary_fd, offset, 0);
        read(g_back_page.vary_fd, (char *) g_back_page.vary_buf, sizeof(g_back_page.vary_buf));
#endif // lint
        g_back_page.old_offset = offset;
    }
    g_back_page.vary_buf[sub_index] = value;
}
