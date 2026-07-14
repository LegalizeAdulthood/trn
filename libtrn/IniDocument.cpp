/* IniDocument.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>

#include <config/common.h>
#include <trn/string-algos.h>
#include <trn/util.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

namespace
{

constexpr std::size_t NO_CURSOR = static_cast<std::size_t>(-1);

} // namespace

IniDocument::IniDocument(std::string contents, std::string_view source_name) :
    m_contents{std::move(contents)}
{
    if (!m_contents.empty())
    {
        m_contents.push_back('\0');
        m_contents.push_back('\0');
        prepare(source_name);
    }
}

IniDocument::IniDocument(IniDocument &&other) noexcept
{
    const std::size_t offset = other.cursor_offset();
    m_contents = std::move(other.m_contents);
    restore_cursor(offset);
    other.m_cursor = nullptr;
}

IniDocument &IniDocument::operator=(IniDocument &&other) noexcept
{
    if (this != &other)
    {
        const std::size_t offset = other.cursor_offset();
        m_contents = std::move(other.m_contents);
        restore_cursor(offset);
        other.m_cursor = nullptr;
    }
    return *this;
}

void IniDocument::prepare(std::string_view source_name)
{
    const std::string source_name_text{source_name.empty() ? std::string_view{"<input>"} : source_name};
    char             *cp = m_contents.data();
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

void IniDocument::rewind()
{
    m_cursor = m_contents.empty() ? nullptr : m_contents.data();
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
    return m_cursor == nullptr ? NO_CURSOR : static_cast<std::size_t>(m_cursor - m_contents.data());
}

void IniDocument::restore_cursor(std::size_t offset)
{
    m_cursor = offset == NO_CURSOR || m_contents.empty() ? nullptr : m_contents.data() + offset;
}
