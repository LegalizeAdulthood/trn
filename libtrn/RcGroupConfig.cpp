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

const char *value_or_null(const IniSectionValues &values, RcGroupConfigField field)
{
    const auto value = values.value(field_id(field));
    if (!value.has_value())
    {
        return nullptr;
    }
    return value->data();
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

RcGroupConfig RcGroupConfig::from(const IniSectionValues &values)
{
    RcGroupConfig config;
    config.set_id(value_or_null(values, RcGroupConfigField::Id));
    config.set_newsrc(value_or_null(values, RcGroupConfigField::Newsrc));
    config.set_add_groups(value_or_null(values, RcGroupConfigField::AddGroups));
    return config;
}
