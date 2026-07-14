/* IniSectionValues.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSectionValues.h>

void IniSectionValues::reset()
{
    m_values.clear();
}

bool IniSectionValues::set(const IniField &field, std::string_view value)
{
    if (field.is_group())
    {
        return false;
    }
    m_values[field.id()] = value;
    return true;
}

bool IniSectionValues::contains(int field_id) const
{
    return m_values.find(field_id) != m_values.end();
}

const char *IniSectionValues::c_str(int field_id) const
{
    const auto it = m_values.find(field_id);
    if (it == m_values.end())
    {
        return nullptr;
    }
    return it->second.c_str();
}

std::optional<std::string_view> IniSectionValues::value(int field_id) const
{
    const auto it = m_values.find(field_id);
    if (it == m_values.end())
    {
        return std::nullopt;
    }
    return it->second;
}
