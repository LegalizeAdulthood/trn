// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/moderators.h>

#include <wildmat/wildmat.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace
{

std::string_view trim(std::string_view text)
{
    const std::size_t first = text.find_first_not_of(" \t\r");
    return first == std::string_view::npos ? std::string_view{}
                                           : text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

bool matches(std::string_view group, std::string_view patterns)
{
    bool matched = false;
    while (!patterns.empty())
    {
        bool        in_class = false;
        std::size_t end = 0;
        for (; end < patterns.size(); ++end)
        {
            const char ch = patterns[end];
            if (ch == '\\' && end + 1 < patterns.size())
            {
                ++end;
            }
            else if (ch == '[')
            {
                in_class = true;
            }
            else if (ch == ']')
            {
                in_class = false;
            }
            else if (ch == ',' && !in_class)
            {
                break;
            }
        }
        std::string_view pattern = trim(patterns.substr(0, end));
        const bool       negative = !pattern.empty() && pattern.front() == '!';
        if (negative)
        {
            pattern.remove_prefix(1);
        }
        if (!pattern.empty() && wildcard_match(group, pattern))
        {
            matched = !negative;
        }
        patterns.remove_prefix(std::min(end + 1, patterns.size()));
    }
    return matched;
}

std::string expand(std::string_view address, std::string_view group)
{
    std::string result;
    bool        used_group = false;
    for (std::size_t i = 0; i < address.size(); ++i)
    {
        if (address[i] != '%')
        {
            result += address[i];
        }
        else if (++i == address.size())
        {
            return {};
        }
        else if (address[i] == '%')
        {
            result += '%';
        }
        else if (address[i] == 's' && !used_group)
        {
            used_group = true;
            for (char ch : group)
            {
                result += ch == '.' ? '-' : ch;
            }
        }
        else
        {
            return {};
        }
    }
    const std::size_t at = result.rfind('@');
    if (at == std::string::npos || at == 0 || at + 1 == result.size() ||
        std::any_of(result.begin(), result.end(),
                    [](unsigned char ch) { return ch <= ' ' || ch >= 0x7F || ch == '<' || ch == '>' || ch == ','; }))
    {
        return {};
    }
    return result;
}

} // namespace

std::string moderator_address(std::string_view group, std::string_view rules)
{
    if (group.empty() || std::any_of(group.begin(), group.end(),
                                     [](unsigned char ch)
                                     {
                                         return !((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                                  (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_' ||
                                                  ch == '+');
                                     }))
    {
        return {};
    }
    while (!rules.empty())
    {
        const std::size_t      end = rules.find('\n');
        const std::string_view line = trim(rules.substr(0, end));
        rules.remove_prefix(end == std::string_view::npos ? rules.size() : end + 1);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }
        const std::size_t colon = line.find(':');
        if (colon != std::string_view::npos && matches(group, line.substr(0, colon)))
        {
            std::string address = expand(trim(line.substr(colon + 1)), group);
            if (!address.empty())
            {
                return address;
            }
        }
    }
    return {};
}
