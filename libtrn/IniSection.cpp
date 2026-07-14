/* IniSection.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSection.h>

#include <cctype>
#include <cstddef>
#include <string_view>

namespace
{

bool is_space(char ch)
{
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

std::size_t line_end(std::string_view text, std::size_t position)
{
    while (position != text.size() && text[position] != '\n')
    {
        ++position;
    }
    return position;
}

std::size_t next_line(std::string_view text, std::size_t position)
{
    position = line_end(text, position);
    return position == text.size() ? position : position + 1;
}

std::size_t trim_right(std::string_view text, std::size_t first, std::size_t last)
{
    while (last != first && is_space(text[last - 1]))
    {
        --last;
    }
    return last;
}

std::size_t find_value_end(std::string_view text, std::size_t position)
{
    char quote = 0;
    while (position != text.size())
    {
        const char ch = text[position];
        if (ch == '\\' && position + 1 != text.size() && text[position + 1] == '\n')
        {
            position += 2;
            continue;
        }
        if (quote != 0)
        {
            if (ch == quote)
            {
                quote = 0;
            }
        }
        else if (ch == '\'' || ch == '"')
        {
            quote = ch;
        }
        else if (ch == '\n')
        {
            return position;
        }
        ++position;
    }
    return position;
}

} // namespace

IniSection::Iterator::Iterator(std::string_view body, std::size_t position) :
    m_body{body},
    m_position{position},
    m_at_end{false}
{
    advance();
}

const IniSetting &IniSection::Iterator::operator*() const
{
    return m_setting;
}

IniSection::Iterator &IniSection::Iterator::operator++()
{
    advance();
    return *this;
}

bool IniSection::Iterator::operator==(const Iterator &other) const
{
    if (m_at_end || other.m_at_end)
    {
        return m_at_end == other.m_at_end;
    }
    return m_position == other.m_position;
}

bool IniSection::Iterator::operator!=(const Iterator &other) const
{
    return !(*this == other);
}

void IniSection::Iterator::advance()
{
    while (m_position != m_body.size())
    {
        while (m_position != m_body.size() && is_space(m_body[m_position]))
        {
            ++m_position;
        }
        if (m_position == m_body.size() || m_body[m_position] == '[')
        {
            m_position = m_body.size();
            m_at_end = true;
            return;
        }
        if (m_body[m_position] == '#')
        {
            m_position = next_line(m_body, m_position);
            continue;
        }

        const std::size_t name_begin = m_position;
        const std::size_t name_line_end = line_end(m_body, m_position);
        std::size_t       equal = name_begin;
        while (equal != name_line_end && m_body[equal] != '=')
        {
            ++equal;
        }
        if (equal == name_line_end)
        {
            m_position = next_line(m_body, m_position);
            continue;
        }

        const std::size_t name_end = trim_right(m_body, name_begin, equal);
        const std::size_t value_begin = equal + 1;
        const std::size_t value_end = find_value_end(m_body, value_begin);
        m_position = value_end == m_body.size() ? value_end : value_end + 1;
        if (name_begin == name_end)
        {
            continue;
        }

        m_setting = IniSetting{m_body.substr(name_begin, name_end - name_begin),
                               m_body.substr(value_begin, value_end - value_begin)};
        m_at_end = false;
        return;
    }
    m_at_end = true;
}

IniSection::IniSection(std::string_view name, std::string_view condition, std::string_view body) :
    m_name{name},
    m_condition{condition},
    m_body{body}
{
}

IniSection::Iterator IniSection::begin() const
{
    return Iterator{m_body, 0};
}

IniSection::Iterator IniSection::end() const
{
    return Iterator{};
}
