/* ActiveRecordEditor.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/ActiveRecordEditor.h>

#include <fmt/format.h>

void ActiveRecordEditor::update_low(std::string_view source)
{
    const std::size_t active_flag_separator = source.rfind(' ');
    const std::size_t cached_flag_separator = text.rfind(' ');
    if (active_flag_separator != std::string::npos && active_flag_separator > 0 &&
        cached_flag_separator != std::string::npos && cached_flag_separator > 0)
    {
        const std::size_t active_low_separator = source.rfind(' ', active_flag_separator - 1);
        const std::size_t cached_low_separator = text.rfind(' ', cached_flag_separator - 1);
        if (active_low_separator != std::string::npos && cached_low_separator != std::string::npos)
        {
            const std::string_view active_low{source.data() + active_low_separator + 1,
                                              active_flag_separator - active_low_separator - 1};
            const std::size_t      field_width = cached_flag_separator - cached_low_separator - 1;
            const std::string_view low_digits =
                active_low.substr(active_low.size() > field_width ? active_low.size() - field_width : 0);
            text.replace(cached_low_separator + 1, field_width, fmt::format("{:0>{}}", low_digits, field_width));
        }
    }
}
