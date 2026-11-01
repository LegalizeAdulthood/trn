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

namespace
{

struct BackPageCache
{
    int             vary_fd{-1};         // virtual array file for storing  file offsets
    ArticlePosition vary_buf[VARY_SIZE]; // current window onto virtual array
    long            old_offset{-1};      // offset to block currently in window
#ifdef DEBUG
    int max_index{-1};
#endif
};

static BackPageCache s_back_page;

} // namespace

void back_page_init()
{
    back_page_final();
    const std::string vary_name = file_exp(VARYNAME);
    close(creat(vary_name.c_str(), 0600));
    int open_flags = O_RDWR;
#ifdef _WIN32
    open_flags |= _O_BINARY;
#endif
    s_back_page.vary_fd = open(vary_name.c_str(), open_flags);
    std::error_code error;
    fs::remove(vary_name, error);
    if (s_back_page.vary_fd < 0)
    {
        fmt::print("Can't open {}\n", vary_name);
        sig_catcher(0);
    }
}

void back_page_final()
{
    if (s_back_page.vary_fd >= 0)
    {
        close(s_back_page.vary_fd);
        s_back_page.vary_fd = -1;
    }
    s_back_page.old_offset = -1;
    std::fill_n(s_back_page.vary_buf, VARY_SIZE, ArticlePosition{});
#ifdef DEBUG
    s_back_page.max_index = -1;
#endif
}

// virtual array read

ArticlePosition virtual_read(ArticleLine index)
{
    int  sub_index;
    long offset;

#ifdef DEBUG
    if (index > s_back_page.max_index)
    {
        fmt::print("vrdary({}) > {}\n", index.value_of(), s_back_page.max_index);
        return ArticlePosition{};
    }
#endif
    if (index < 0)
    {
        return ArticlePosition{};
    }
    sub_index = index.value_of() % VARY_SIZE;
    offset = (index.value_of() - sub_index) * sizeof(s_back_page.vary_buf[0]);
    if (offset != s_back_page.old_offset)
    {
        if (s_back_page.old_offset >= 0)
        {
#ifndef lint
            (void) lseek(s_back_page.vary_fd, s_back_page.old_offset, 0);
            write(s_back_page.vary_fd, (char *) s_back_page.vary_buf, sizeof(s_back_page.vary_buf));
#endif // lint
        }
#ifndef lint
        (void) lseek(s_back_page.vary_fd, offset, 0);
        read(s_back_page.vary_fd, (char *) s_back_page.vary_buf, sizeof(s_back_page.vary_buf));
#endif // lint
        s_back_page.old_offset = offset;
    }
    return s_back_page.vary_buf[sub_index];
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
        s_back_page.max_index = 0;
    }
    if (index > s_back_page.max_index)
    {
        if (index != s_back_page.max_index + 1)
        {
            fmt::print("index skipped {}-{}\n", s_back_page.max_index + 1, index.value_of() - 1);
        }
        s_back_page.max_index = index.value_of();
    }
#endif
    sub_index = index.value_of() % VARY_SIZE;
    offset = (index.value_of() - sub_index) * sizeof(s_back_page.vary_buf[0]);
    if (offset != s_back_page.old_offset)
    {
        if (s_back_page.old_offset >= 0)
        {
#ifndef lint
            (void) lseek(s_back_page.vary_fd, s_back_page.old_offset, 0);
            write(s_back_page.vary_fd, (char *) s_back_page.vary_buf, sizeof(s_back_page.vary_buf));
#endif // lint
        }
#ifndef lint
        (void) lseek(s_back_page.vary_fd, offset, 0);
        read(s_back_page.vary_fd, (char *) s_back_page.vary_buf, sizeof(s_back_page.vary_buf));
#endif // lint
        s_back_page.old_offset = offset;
    }
    s_back_page.vary_buf[sub_index] = value;
}
