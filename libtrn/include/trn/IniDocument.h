/* trn/IniDocument.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_DOCUMENT_H
#define TRN_INI_DOCUMENT_H

#include <cstddef>
#include <string>
#include <string_view>

class IniDocument
{
public:
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

    bool next_section(Section &section);
    void rewind();

private:
    static char *find_next_section(char *cursor, char **section, char **condition);
    static char *skip_section_body(char *cursor);
    void         prepare(std::string_view source_name);
    std::size_t  cursor_offset() const;
    void         restore_cursor(std::size_t offset);

    std::string m_contents;
    char       *m_cursor{};
};

inline bool IniDocument::Section::has_condition() const
{
    return condition != nullptr && *condition != '\0';
}

#endif
