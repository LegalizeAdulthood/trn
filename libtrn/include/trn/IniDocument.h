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

    IniDocument() = default;
    explicit IniDocument(std::string contents, std::string_view source_name = "input");

    IniDocument(const IniDocument &) = delete;
    IniDocument &operator=(const IniDocument &) = delete;
    IniDocument(IniDocument &&other) noexcept = default;
    IniDocument &operator=(IniDocument &&other) noexcept = default;

    Iterator begin() const;
    Iterator end() const;

private:
    std::string m_contents;
};

#endif
