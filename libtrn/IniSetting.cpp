/* IniSetting.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSetting.h>

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace
{

bool is_space_not_newline(char ch)
{
    return ch != '\n' && std::isspace(static_cast<unsigned char>(ch));
}

int hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return ch - 'A' + 10;
}

void append_escape(std::string &result, std::string_view text, std::size_t &index)
{
    if (index == text.size())
    {
        result.push_back('\\');
        --index;
        return;
    }

    char ch = text[index];
    if (ch >= '0' && ch <= '7')
    {
        int value = 0;
        while (value < 01000 && index != text.size() && text[index] >= '0' && text[index] <= '7')
        {
            value <<= 3;
            value += text[index++] - '0';
        }
        result.push_back(static_cast<char>(value & 0377));
        --index;
        return;
    }

    switch (ch)
    {
    case 'a':
        result.push_back('\a');
        break;

    case 'b':
        result.push_back('\b');
        break;

    case 'f':
        result.push_back('\f');
        break;

    case 'n':
        result.push_back('\n');
        break;

    case 'r':
        result.push_back('\r');
        break;

    case 't':
        result.push_back('\t');
        break;

    case 'v':
        result.push_back('\v');
        break;

    case 'x':
        if (index + 1 != text.size() && std::isxdigit(static_cast<unsigned char>(text[index + 1])))
        {
            int value = 0;
            while (value < 01000 && index + 1 != text.size() &&
                   std::isxdigit(static_cast<unsigned char>(text[index + 1])))
            {
                ++index;
                value <<= 4;
                value += hex_digit_value(text[index]);
            }
            result.push_back(static_cast<char>(value & 0377));
        }
        else
        {
            result.push_back(ch);
        }
        break;

    default:
        result.push_back(ch);
        break;
    }
}

} // namespace

IniSetting::IniSetting(std::string_view name, std::string_view raw_value) :
    m_name{name},
    m_raw_value{raw_value}
{
}

std::string IniSetting::value() const
{
    std::string result;
    result.reserve(m_raw_value.size());

    std::size_t index = 0;
    while (index != m_raw_value.size() && is_space_not_newline(m_raw_value[index]))
    {
        ++index;
    }

    char        quote = 0;
    std::size_t trim_limit = result.size();
    for (; index != m_raw_value.size(); ++index)
    {
        const char ch = m_raw_value[index];
        if (quote != 0)
        {
            if (ch == quote)
            {
                quote = 0;
                trim_limit = result.size();
                continue;
            }
        }
        else if (ch == '\n')
        {
            break;
        }
        else if (ch == '\'' || ch == '"')
        {
            quote = ch;
            continue;
        }
        else if (ch == '#')
        {
            break;
        }

        if (ch == '\\')
        {
            ++index;
            if (index != m_raw_value.size() && m_raw_value[index] == '\n')
            {
                continue;
            }
            append_escape(result, m_raw_value, index);
        }
        else
        {
            result.push_back(ch);
        }
    }

    while (result.size() != trim_limit && std::isspace(static_cast<unsigned char>(result[result.size() - 1])))
    {
        result.pop_back();
    }
    return result;
}
