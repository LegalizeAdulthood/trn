/* trn/respond-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RESPOND_INTERNAL_H
#define TRN_RESPOND_INTERNAL_H

#include <cstdio>
#include <functional>
#include <string_view>

struct SaveFileOperations
{
    std::function<std::FILE *(const char *, const char *)> open{std::fopen};
    std::function<int(std::FILE *)>                        close{std::fclose};
};

extern SaveFileOperations g_save_file_operations;

std::string_view respond_parse_extract_options_for_test(std::string_view command_text, int &part_opt, int &total_opt);

#endif
