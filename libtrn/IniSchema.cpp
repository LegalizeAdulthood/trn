/* IniSchema.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSchema.h>

#include <cctype>

IniField::IniField(int id, std::string_view name, std::string_view help) :
    IniField{id, name, help, Kind::Value}
{
}

IniField::IniField(int id, std::string_view name, std::string_view help, Kind kind) :
    m_id{id},
    m_name{name},
    m_help{help},
    m_kind{kind}
{
}

IniField IniField::group(std::string_view name)
{
    return IniField{-1, name, {}, Kind::Group};
}

IniField IniField::value(int id, std::string_view name, std::string_view help)
{
    return IniField{id, name, help, Kind::Value};
}

IniSchema::IniSchema(std::string_view section_name, std::initializer_list<IniField> fields) :
    m_section_name{section_name},
    m_fields{fields}
{
    for (std::size_t i = 0; i < m_fields.size(); ++i)
    {
        if (m_fields[i].is_value())
        {
            m_lookup[lookup_key(m_fields[i].name())] = i;
        }
    }
}

const IniField *IniSchema::find(std::string_view name) const
{
    const auto it = m_lookup.find(lookup_key(name));
    if (it == m_lookup.end())
    {
        return nullptr;
    }
    return &m_fields[it->second];
}

std::string IniSchema::lookup_key(std::string_view name)
{
    std::string key;
    key.reserve(name.size());
    for (char ch : name)
    {
        key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return key;
}
