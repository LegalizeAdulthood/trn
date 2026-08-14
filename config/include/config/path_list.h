/* config/path_list.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_CONFIG_PATH_LIST_H
#define TRN_CONFIG_PATH_LIST_H

#include <string_view>
#include <vector>

std::vector<std::string_view> split_path_list(std::string_view text);

#endif
