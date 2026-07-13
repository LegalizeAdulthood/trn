/* RcGroupConfig.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/RcGroupConfig.h>

#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>

namespace
{

constexpr int field_id(RcGroupConfigField field)
{
    return static_cast<int>(field);
}

std::optional<std::string> value_or_null(const IniSectionValues &values, RcGroupConfigField field)
{
    const auto value = values.value(field_id(field));
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return std::string{*value};
}

const IniSchema s_schema{"RCGROUPS",
                         {
                             IniField::value(field_id(RcGroupConfigField::Id), "ID"),
                             IniField::value(field_id(RcGroupConfigField::Newsrc), "Newsrc"),
                             IniField::value(field_id(RcGroupConfigField::AddGroups), "Add Groups"),
                         }};

} // namespace

const IniSchema &RcGroupConfig::schema()
{
    return s_schema;
}

const char *RcGroupConfig::c_str(const std::optional<std::string> &value)
{
    return value.has_value() ? value->c_str() : nullptr;
}

void RcGroupConfig::set_value(std::optional<std::string> &target, const char *value)
{
    if (value == nullptr)
    {
        target.reset();
    }
    else
    {
        target = value;
    }
}

RcGroupConfig RcGroupConfig::from(const IniSectionValues &values)
{
    RcGroupConfig config;
    config.m_id = value_or_null(values, RcGroupConfigField::Id);
    config.m_newsrc = value_or_null(values, RcGroupConfigField::Newsrc);
    config.m_add_groups = value_or_null(values, RcGroupConfigField::AddGroups);
    return config;
}
