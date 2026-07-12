/* trn/IniDocument.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_DOCUMENT_H
#define TRN_INI_DOCUMENT_H

#include <cstdlib>
#include <memory>
#include <string_view>

class IniDocument
{
public:
    enum class BufferState
    {
        Raw,
        Prepared
    };

    struct Section
    {
        char *name{};
        char *condition{};
        char *body{};

        bool has_condition() const;
    };

    IniDocument() = default;
    IniDocument(char *buffer, const char *filename, BufferState state);
    explicit IniDocument(std::string_view text, const char *filename = "input");

    IniDocument(const IniDocument &) = delete;
    IniDocument &operator=(const IniDocument &) = delete;
    IniDocument(IniDocument &&) noexcept = default;
    IniDocument &operator=(IniDocument &&) noexcept = default;

    static IniDocument read_file(const char *path, const char *filename);
    static void        prepare(char *buffer, const char *filename);
    static char       *find_next_section(char *cursor, char **section, char **condition);

    bool  next_section(Section &section);
    void  rewind();
    char *data() const;
    char *release_buffer();

private:
    struct FreeBuffer
    {
        void operator()(char *buffer) const
        {
            std::free(buffer);
        }
    };

    using BufferPtr = std::unique_ptr<char, FreeBuffer>;

    static char *skip_section_body(char *cursor);
    void         own_raw_text(std::string_view text, const char *filename);
    void         own_buffer(char *buffer, const char *filename);

    BufferPtr m_owned_buffer;
    char     *m_buffer{};
    char     *m_cursor{};
};

inline bool IniDocument::Section::has_condition() const
{
    return condition != nullptr && *condition != '\0';
}

inline char *IniDocument::data() const
{
    return m_buffer;
}

#endif
