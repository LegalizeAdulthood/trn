/* trn/IniSection.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_SECTION_H
#define TRN_INI_SECTION_H

#include <trn/IniSetting.h>

#include <cstddef>
#include <string_view>

class IniSection
{
public:
    class Iterator
    {
    public:
        Iterator() = default;
        Iterator(std::string_view body, std::size_t position);

        const IniSetting &operator*() const;
        Iterator         &operator++();
        bool              operator==(const Iterator &other) const;
        bool              operator!=(const Iterator &other) const;

    private:
        void advance();

        std::string_view m_body;
        std::size_t      m_position{};
        bool             m_at_end{true};
        IniSetting       m_setting;
    };

    IniSection() = default;
    IniSection(std::string_view name, std::string_view condition, std::string_view body);

    std::string_view name() const;
    std::string_view condition() const;
    bool             has_condition() const;
    Iterator         begin() const;
    Iterator         end() const;

private:
    std::string_view m_name;
    std::string_view m_condition;
    std::string_view m_body;
};

inline std::string_view IniSection::name() const
{
    return m_name;
}

inline std::string_view IniSection::condition() const
{
    return m_condition;
}

inline bool IniSection::has_condition() const
{
    return !m_condition.empty();
}

#endif
