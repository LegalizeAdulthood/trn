/* IniDocument.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>

#include <config/common.h>
#include <trn/IniSection.h>
#include <trn/string-algos.h>
#include <trn/util.h>

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

namespace
{

constexpr std::size_t NO_CURSOR = static_cast<std::size_t>(-1);
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

IniDocument::IniDocument(std::string contents, std::string_view source_name) :
    m_contents{std::move(contents)},
    m_compat_contents{m_contents}
{
    if (!m_compat_contents.empty())
    {
        m_compat_contents.push_back('\0');
        m_compat_contents.push_back('\0');
        prepare(source_name);
    }
}

IniDocument::IniDocument(IniDocument &&other) noexcept
{
    const std::size_t offset = other.cursor_offset();
    m_contents = std::move(other.m_contents);
    m_compat_contents = std::move(other.m_compat_contents);
    restore_cursor(offset);
    other.m_cursor = nullptr;
}

IniDocument &IniDocument::operator=(IniDocument &&other) noexcept
{
    if (this != &other)
    {
        const std::size_t offset = other.cursor_offset();
        m_contents = std::move(other.m_contents);
        m_compat_contents = std::move(other.m_compat_contents);
        restore_cursor(offset);
        other.m_cursor = nullptr;
    }
    return *this;
}

void IniDocument::prepare(std::string_view source_name)
{
    const std::string source_name_text{source_name.empty() ? std::string_view{"<input>"} : source_name};
    char             *cp = m_compat_contents.data();
    char             *t = cp;

#ifdef DEBUG
    if (g_debug & DEB_RCFILES)
    {
        std::printf("Read %d bytes from %s\n", static_cast<int>(std::strlen(cp)), source_name_text.c_str());
    }
#endif

    while (*cp)
    {
        cp = skip_space(cp);

        if (*cp == '[')
        {
            char *s = t;
            do
            {
                *t++ = *cp++;
            } while (*cp && *cp != ']' && *cp != '\n');
            if (*cp == ']' && t != s)
            {
                *t++ = '\0';
                cp++;
                if (parse_string(&t, &cp))
                {
                    cp++;
                }

                while (*cp)
                {
                    cp = skip_space(cp);
                    if (*cp == '[')
                    {
                        break;
                    }
                    if (*cp == '#')
                    {
                        s = cp;
                    }
                    else
                    {
                        s = t;
                        while (*cp && *cp != '\n')
                        {
                            if (*cp == '=')
                            {
                                break;
                            }
                            if (std::isspace(*cp))
                            {
                                if (s == t || t[-1] != ' ')
                                {
                                    *t++ = ' ';
                                }
                                cp++;
                            }
                            else
                            {
                                *t++ = *cp++;
                            }
                        }
                        if (*cp == '=' && t != s)
                        {
                            while (t != s && std::isspace(t[-1]))
                            {
                                t--;
                            }
                            *t++ = '\0';
                            cp++;
                            if (parse_string(&t, &cp))
                            {
                                s = nullptr;
                            }
                            else
                            {
                                s = cp;
                            }
                        }
                        else
                        {
                            s = cp;
                        }
                    }
                    cp++;
                    if (s)
                    {
                        for (cp = s; *cp && *cp++ != '\n';)
                        {
                        }
                    }
                }
            }
            else
            {
                *t = '\0';
                std::printf("Invalid section in %s: %s\n", source_name_text.c_str(), s);
                t = s;
                cp = skip_ne(cp, '\n');
            }
        }
        else
        {
            cp = skip_ne(cp, '\n');
        }
    }
    *t = '\0';
    rewind();
}

char *IniDocument::find_next_section(char *cursor, char **section, char **condition)
{
    if (cursor == nullptr)
    {
        return nullptr;
    }
    while (*cursor != '[')
    {
        if (!*cursor)
        {
            return nullptr;
        }
        cursor += std::strlen(cursor) + 1;
        cursor += std::strlen(cursor) + 1;
    }
    *section = cursor + 1;
    cursor += std::strlen(cursor) + 1;
    *condition = cursor;
    cursor += std::strlen(cursor) + 1;
#ifdef DEBUG
    if (g_debug & DEB_RCFILES)
    {
        std::printf("Section [%s] (condition: %s)\n", *section, **condition ? *condition : "<none>");
    }
#endif
    return cursor;
}

bool IniDocument::next_section(Section &section)
{
    char *name{};
    char *condition{};
    char *body = find_next_section(m_cursor, &name, &condition);
    if (body == nullptr)
    {
        m_cursor = nullptr;
        return false;
    }

    section.name = name;
    section.condition = condition;
    section.body = body;
    m_cursor = skip_section_body(body);
    return true;
}

IniDocument::Iterator IniDocument::begin() const
{
    return Iterator{m_contents, 0};
}

IniDocument::Iterator IniDocument::end() const
{
    return Iterator{};
}

void IniDocument::rewind()
{
    m_cursor = m_compat_contents.empty() ? nullptr : m_compat_contents.data();
}

char *IniDocument::skip_section_body(char *cursor)
{
    while (*cursor && *cursor != '[')
    {
        cursor += std::strlen(cursor) + 1;
        if (*cursor)
        {
            cursor += std::strlen(cursor) + 1;
        }
        else
        {
            cursor++;
        }
    }
    return cursor;
}

std::size_t IniDocument::cursor_offset() const
{
    return m_cursor == nullptr ? NO_CURSOR : static_cast<std::size_t>(m_cursor - m_compat_contents.data());
}

void IniDocument::restore_cursor(std::size_t offset)
{
    m_cursor = offset == NO_CURSOR || m_compat_contents.empty() ? nullptr : m_compat_contents.data() + offset;
}
