/* IniField.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniField.h>

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
