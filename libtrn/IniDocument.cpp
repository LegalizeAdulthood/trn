/* IniDocument.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>

#include <config/common.h>
#include <config/fdio.h>
#include <trn/string-algos.h>
#include <trn/util.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

IniDocument::IniDocument(char *buffer, const char *filename, BufferState state) :
    m_buffer{buffer},
    m_cursor{buffer}
{
    if (m_buffer != nullptr && state == BufferState::Raw)
    {
        prepare(m_buffer, filename);
    }
}

IniDocument::IniDocument(std::string_view text, const char *filename)
{
    own_raw_text(text, filename);
}

IniDocument IniDocument::read_file(const char *path, const char *filename)
{
    IniDocument document;
    const int   fd = open(path, 0);
    if (fd < 0)
    {
        return document;
    }

    stat_t file_stat{};
    fstat(fd, &file_stat);
    if (file_stat.st_size > 0)
    {
        char     *buffer = safe_malloc(static_cast<MemorySize>(file_stat.st_size) + 2);
        const int length = read(fd, buffer, static_cast<int>(file_stat.st_size));
        if (length > 0)
        {
            buffer[length] = '\0';
            buffer[length + 1] = '\0';
            document.own_buffer(buffer, filename);
            buffer = nullptr;
        }
        std::free(buffer);
    }
    close(fd);
    return document;
}

void IniDocument::prepare(char *buffer, const char *filename)
{
    const char *source_name = filename != nullptr ? filename : "<input>";
    char       *cp = buffer;
    char       *t = cp;

#ifdef DEBUG
    if (g_debug & DEB_RCFILES)
    {
        std::printf("Read %d bytes from %s\n", static_cast<int>(std::strlen(cp)), source_name);
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
                std::printf("Invalid section in %s: %s\n", source_name, s);
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
    m_cursor = m_buffer;
}

char *IniDocument::release_buffer()
{
    char *buffer = m_owned_buffer.release();
    if (buffer == m_buffer)
    {
        m_buffer = nullptr;
        m_cursor = nullptr;
    }
    return buffer;
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

void IniDocument::own_raw_text(std::string_view text, const char *filename)
{
    char *buffer = safe_malloc(static_cast<MemorySize>(text.size()) + 2);
    std::memcpy(buffer, text.data(), text.size());
    buffer[text.size()] = '\0';
    buffer[text.size() + 1] = '\0';
    own_buffer(buffer, filename);
}

void IniDocument::own_buffer(char *buffer, const char *filename)
{
    m_owned_buffer.reset(buffer);
    m_buffer = buffer;
    m_cursor = buffer;
    prepare(buffer, filename);
}
