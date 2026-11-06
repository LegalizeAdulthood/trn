/* ActiveRecordEditor.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/ActiveRecordEditor.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>

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

ArticleNum ActiveRecordEditor::high() const
{
    const std::string_view fields = std::string_view{text}.substr(std::min(name_len + 1, text.size()));
    long                   high{};
    std::from_chars(fields.data(), fields.data() + fields.size(), high);
    return ArticleNum{high};
}

void ActiveRecordEditor::update_high(ArticleNum high)
{
    const std::size_t high_start = name_len + 1;
    const std::size_t high_end = text.find(' ', high_start);
    if (high_end != std::string::npos)
    {
        const std::size_t      field_width = high_end - high_start;
        const std::string      high_text = std::to_string(value_of(high));
        const std::string_view high_digits =
            std::string_view{high_text}.substr(high_text.size() > field_width ? high_text.size() - field_width : 0);
        text.replace(high_start, field_width, fmt::format("{:0>{}}", high_digits, field_width));
    }
}
