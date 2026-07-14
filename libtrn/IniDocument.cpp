/* IniDocument.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>

#include <trn/IniSection.h>

#include <cctype>
#include <cstddef>
#include <utility>

namespace
{

constexpr std::size_t NOT_FOUND = static_cast<std::size_t>(-1);

bool is_space(char ch)
{
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

bool is_space_not_newline(char ch)
{
    return ch != '\n' && is_space(ch);
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

std::size_t skip_document_space(std::string_view text, std::size_t position)
{
    while (position != text.size() && is_space(text[position]))
    {
        ++position;
    }
    return position;
}

std::size_t skip_space_not_newline(std::string_view text, std::size_t position)
{
    while (position != text.size() && is_space_not_newline(text[position]))
    {
        ++position;
    }
    return position;
}

std::size_t condition_end(std::string_view text, std::size_t first, std::size_t last)
{
    char quote = 0;
    for (std::size_t position = first; position != last; ++position)
    {
        const char ch = text[position];
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
        else if (ch == '#')
        {
            return trim_right(text, first, position);
        }
    }
    return trim_right(text, first, last);
}

std::size_t find_section_header(std::string_view text, std::size_t position)
{
    while (position != text.size())
    {
        position = skip_document_space(text, position);
        if (position == text.size())
        {
            return NOT_FOUND;
        }
        if (text[position] != '[')
        {
            position = next_line(text, position);
            continue;
        }

        const std::size_t end_of_line = line_end(text, position);
        const std::size_t close = text.find(']', position + 1);
        if (position + 1 != end_of_line && close != NOT_FOUND && close < end_of_line)
        {
            return position;
        }
        position = next_line(text, position);
    }
    return NOT_FOUND;
}

} // namespace

IniDocument::Iterator::Iterator(std::string_view text, std::size_t position) :
    m_text{text},
    m_position{position},
    m_at_end{false}
{
    advance();
}

const IniSection &IniDocument::Iterator::operator*() const
{
    return m_section;
}

IniDocument::Iterator &IniDocument::Iterator::operator++()
{
    advance();
    return *this;
}

bool IniDocument::Iterator::operator==(const Iterator &other) const
{
    if (m_at_end || other.m_at_end)
    {
        return m_at_end == other.m_at_end;
    }
    return m_position == other.m_position;
}

bool IniDocument::Iterator::operator!=(const Iterator &other) const
{
    return !(*this == other);
}

void IniDocument::Iterator::advance()
{
    const std::size_t header = find_section_header(m_text, m_position);
    if (header == NOT_FOUND)
    {
        m_position = m_text.size();
        m_at_end = true;
        return;
    }

    const std::size_t line = line_end(m_text, header);
    const std::size_t close = m_text.find(']', header + 1);
    const std::size_t condition_first = skip_space_not_newline(m_text, close + 1);
    const std::size_t condition_last = condition_end(m_text, condition_first, line);
    const std::size_t body_first = line == m_text.size() ? line : line + 1;
    const std::size_t next_header = find_section_header(m_text, body_first);
    const std::size_t body_last = next_header == NOT_FOUND ? m_text.size() : next_header;

    m_section = IniSection{m_text.substr(header + 1, close - header - 1),
                           m_text.substr(condition_first, condition_last - condition_first),
                           m_text.substr(body_first, body_last - body_first)};
    m_position = body_last;
    m_at_end = false;
}

IniDocument::IniDocument(std::string contents, std::string_view) :
    m_contents{std::move(contents)}
{
}

IniDocument::Iterator IniDocument::begin() const
{
    return Iterator{m_contents, 0};
}

IniDocument::Iterator IniDocument::end() const
{
    return Iterator{};
}

