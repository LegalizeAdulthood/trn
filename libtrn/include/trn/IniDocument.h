/* trn/IniDocument.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_DOCUMENT_H
#define TRN_INI_DOCUMENT_H

#include <trn/IniSection.h>

#include <cstddef>
#include <string>
#include <string_view>

class IniDocument
{
public:
    class Iterator
    {
    public:
        Iterator() = default;
        Iterator(std::string_view text, std::size_t position);

        const IniSection &operator*() const;
        Iterator         &operator++();
        bool              operator==(const Iterator &other) const;
        bool              operator!=(const Iterator &other) const;

    private:
        void advance();

        std::string_view m_text;
        std::size_t      m_position{};
        bool             m_at_end{true};
        IniSection       m_section;
    };

    struct Section
    {
        char *name{};
        char *condition{};
        char *body{};

        bool has_condition() const;
    };

    IniDocument() = default;
    explicit IniDocument(std::string contents, std::string_view source_name = "input");

    IniDocument(const IniDocument &) = delete;
    IniDocument &operator=(const IniDocument &) = delete;
    IniDocument(IniDocument &&other) noexcept;
    IniDocument &operator=(IniDocument &&other) noexcept;

    bool     next_section(Section &section);
    void     rewind();
    Iterator begin() const;
    Iterator end() const;

private:
    static char *find_next_section(char *cursor, char **section, char **condition);
    static char *skip_section_body(char *cursor);
    void         prepare(std::string_view source_name);
    std::size_t  cursor_offset() const;
    void         restore_cursor(std::size_t offset);

    std::string m_contents;
    std::string m_compat_contents;
    char       *m_cursor{};
};

inline bool IniDocument::Section::has_condition() const
{
    return condition != nullptr && *condition != '\0';
}

#endif
