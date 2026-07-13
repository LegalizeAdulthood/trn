/* DataSourceConfig.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/DataSourceConfig.h>

#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>

namespace
{

constexpr int field_id(DataSourceConfigField field)
{
    return static_cast<int>(field);
}

std::optional<std::string> value_or_null(const IniSectionValues &values, DataSourceConfigField field)
{
    const auto value = values.value(field_id(field));
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return std::string{*value};
}

const IniSchema s_schema{
    "DATASRC",
    {
        IniField::value(field_id(DataSourceConfigField::NntpServer), "NNTP Server"),
        IniField::value(field_id(DataSourceConfigField::ActiveFile), "Active File"),
        IniField::value(field_id(DataSourceConfigField::ActiveFileRefetch), "Active File Refetch"),
        IniField::value(field_id(DataSourceConfigField::SpoolDir), "Spool Dir"),
        IniField::value(field_id(DataSourceConfigField::ThreadDir), "Thread Dir"),
        IniField::value(field_id(DataSourceConfigField::OverviewDir), "Overview Dir"),
        IniField::value(field_id(DataSourceConfigField::ActiveTimes), "Active Times"),
        IniField::value(field_id(DataSourceConfigField::GroupDesc), "Group Desc"),
        IniField::value(field_id(DataSourceConfigField::GroupDescRefetch), "Group Desc Refetch"),
        IniField::value(field_id(DataSourceConfigField::AuthUser), "Auth User"),
        IniField::value(field_id(DataSourceConfigField::AuthPassword), "Auth Password"),
        IniField::value(field_id(DataSourceConfigField::AuthCommand), "Auth Command"),
        IniField::value(field_id(DataSourceConfigField::XhdrBroken), "XHDR Broken"),
        IniField::value(field_id(DataSourceConfigField::Xrefs), "Xrefs"),
        IniField::value(field_id(DataSourceConfigField::OverviewFormatFile), "Overview Format File"),
        IniField::value(field_id(DataSourceConfigField::ForceAuth), "Force Auth"),
    }};

} // namespace

const IniSchema &DataSourceConfig::schema()
{
    return s_schema;
}

const char *DataSourceConfig::c_str(const std::optional<std::string> &value)
{
    return value.has_value() ? value->c_str() : nullptr;
}

void DataSourceConfig::set_value(std::optional<std::string> &target, const char *value)
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

DataSourceConfig DataSourceConfig::from(const IniSectionValues &values)
{
    DataSourceConfig config;
    config.m_nntp_server = value_or_null(values, DataSourceConfigField::NntpServer);
    config.m_active_file = value_or_null(values, DataSourceConfigField::ActiveFile);
    config.m_active_file_refetch = value_or_null(values, DataSourceConfigField::ActiveFileRefetch);
    config.m_spool_dir = value_or_null(values, DataSourceConfigField::SpoolDir);
    config.m_thread_dir = value_or_null(values, DataSourceConfigField::ThreadDir);
    config.m_overview_dir = value_or_null(values, DataSourceConfigField::OverviewDir);
    config.m_active_times = value_or_null(values, DataSourceConfigField::ActiveTimes);
    config.m_group_desc = value_or_null(values, DataSourceConfigField::GroupDesc);
    config.m_group_desc_refetch = value_or_null(values, DataSourceConfigField::GroupDescRefetch);
    config.m_auth_user = value_or_null(values, DataSourceConfigField::AuthUser);
    config.m_auth_password = value_or_null(values, DataSourceConfigField::AuthPassword);
    config.m_auth_command = value_or_null(values, DataSourceConfigField::AuthCommand);
    config.m_xhdr_broken = value_or_null(values, DataSourceConfigField::XhdrBroken);
    config.m_xrefs = value_or_null(values, DataSourceConfigField::Xrefs);
    config.m_overview_format_file = value_or_null(values, DataSourceConfigField::OverviewFormatFile);
    config.m_force_auth = value_or_null(values, DataSourceConfigField::ForceAuth);
    return config;
}
