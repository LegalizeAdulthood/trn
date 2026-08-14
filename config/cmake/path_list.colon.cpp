/* path_list.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/path_list.h>

#include <string_view>
#include <vector>

std::vector<std::string_view> split_path_list(std::string_view text)
{
    std::vector<std::string_view> entries;
    std::size_t                   entry_begin = 0;
    for (std::size_t pos = 0; pos <= text.size(); ++pos)
    {
        const bool at_end = pos == text.size();
        if (!at_end && text[pos] != ':')
        {
            continue;
        }
        if (entry_begin != pos)
        {
            entries.push_back(text.substr(entry_begin, pos - entry_begin));
        }
        entry_begin = pos + 1;
    }
    return entries;
}
