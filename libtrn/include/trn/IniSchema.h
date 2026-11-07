/* trn/IniSchema.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_SCHEMA_H
#define TRN_INI_SCHEMA_H

#include <trn/IniField.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class IniSchema
{
public:
    using Fields = std::vector<IniField>;

    IniSchema(std::string_view section_name, std::initializer_list<IniField> fields);

    std::string_view section_name() const;
    const Fields    &fields() const;
    std::size_t      size() const;
    const IniField  &field(std::size_t index) const;
    const IniField  *find(std::string_view name) const;

private:
    static std::string lookup_key(std::string_view name);

    std::string                                  m_section_name;
    Fields                                       m_fields;
    std::unordered_map<std::string, std::size_t> m_lookup;
};

inline std::string_view IniSchema::section_name() const
{
    return m_section_name;
}

inline const IniSchema::Fields &IniSchema::fields() const
{
    return m_fields;
}

inline std::size_t IniSchema::size() const
{
    return m_fields.size();
}

inline const IniField &IniSchema::field(std::size_t index) const
{
    return m_fields[index];
}

#endif
