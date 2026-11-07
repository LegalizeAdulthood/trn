/* RcGroupConfig.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/RcGroupConfig.h>

#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>

#include <optional>
#include <string>
#include <string_view>

namespace
{

constexpr int field_id(RcGroupConfigField field)
{
    return static_cast<int>(field);
}

std::string value_or_empty(const IniSectionValues &values, RcGroupConfigField field)
{
    const std::optional<std::string_view> value = values.value(field_id(field));
    if (!value.has_value())
    {
        return {};
    }
    return std::string{*value};
}

const IniSchema s_schema{"RCGROUPS",
                         {
                             IniField::value(field_id(RcGroupConfigField::ID), "ID"),
                             IniField::value(field_id(RcGroupConfigField::NEWSRC), "Newsrc"),
                             IniField::value(field_id(RcGroupConfigField::ADD_GROUPS), "Add Groups"),
                         }};

} // namespace

const IniSchema &RcGroupConfig::schema()
{
    return s_schema;
}

RcGroupConfig RcGroupConfig::from(const IniSectionValues &values)
{
    RcGroupConfig config;
    config.m_id = value_or_empty(values, RcGroupConfigField::ID);
    config.m_newsrc = value_or_empty(values, RcGroupConfigField::NEWSRC);
    config.m_add_groups = value_or_empty(values, RcGroupConfigField::ADD_GROUPS);
    return config;
}
