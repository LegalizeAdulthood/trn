/* DataSourceConfig.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/DataSourceConfig.h>

#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>

#include <optional>
#include <string>
#include <string_view>

namespace
{

constexpr int field_id(DataSourceConfigField field)
{
    return static_cast<int>(field);
}

std::optional<std::string> value_or_null(const IniSectionValues &values, DataSourceConfigField field)
{
    const std::optional<std::string_view> value = values.value(field_id(field));
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return std::string{*value};
}

const IniSchema s_schema{
    "DATASRC",
    {
        IniField::value(field_id(DataSourceConfigField::NNTP_SERVER), "NNTP Server"),
        IniField::value(field_id(DataSourceConfigField::ACTIVE_FILE), "Active File"),
        IniField::value(field_id(DataSourceConfigField::ACTIVE_FILE_REFETCH), "Active File Refetch"),
        IniField::value(field_id(DataSourceConfigField::SPOOL_DIR), "Spool Dir"),
        IniField::value(field_id(DataSourceConfigField::OVERVIEW_DIRECTORY), "Overview Dir"),
        IniField::value(field_id(DataSourceConfigField::ACTIVE_TIMES_FILE), "Active Times"),
        IniField::value(field_id(DataSourceConfigField::GROUP_DESCRIPTION), "Group Desc"),
        IniField::value(field_id(DataSourceConfigField::GROUP_DESC_REFETCH), "Group Desc Refetch"),
        IniField::value(field_id(DataSourceConfigField::AUTH_USER), "Auth User"),
        IniField::value(field_id(DataSourceConfigField::AUTH_PASSWORD), "Auth Password"),
        IniField::value(field_id(DataSourceConfigField::AUTH_COMMAND), "Auth Command"),
        IniField::value(field_id(DataSourceConfigField::XHDR_BROKEN), "XHDR Broken"),
        IniField::value(field_id(DataSourceConfigField::XREFS), "Xrefs"),
        IniField::value(field_id(DataSourceConfigField::OVERVIEW_FORMAT_FILE), "Overview Format File"),
        IniField::value(field_id(DataSourceConfigField::FORCE_AUTH), "Force Auth"),
        IniField::value(field_id(DataSourceConfigField::ALLOW_LEGACY_AUTH), "Allow Legacy Auth"),
        IniField::value(field_id(DataSourceConfigField::START_TLS), "STARTTLS"),
        IniField::value(field_id(DataSourceConfigField::IMPLICIT_TLS), "Implicit TLS"),
        IniField::value(field_id(DataSourceConfigField::REQUIRE_TLS_FOR_AUTH), "Require TLS For Auth"),
        IniField::value(field_id(DataSourceConfigField::REQUIRE_TLS_FOR_POSTING), "Require TLS For Posting"),
        IniField::value(field_id(DataSourceConfigField::MODERATORS_FILE), "Moderators File"),
    }};

} // namespace

const IniSchema &DataSourceConfig::schema()
{
    return s_schema;
}

void DataSourceConfig::set_value(std::optional<std::string> &target, std::string_view value)
{
    target = std::string{value};
}

DataSourceConfig DataSourceConfig::from(const IniSectionValues &values)
{
    DataSourceConfig config;
    config.m_nntp_server = value_or_null(values, DataSourceConfigField::NNTP_SERVER);
    config.m_active_file = value_or_null(values, DataSourceConfigField::ACTIVE_FILE);
    config.m_active_file_refetch = value_or_null(values, DataSourceConfigField::ACTIVE_FILE_REFETCH);
    config.m_spool_dir = value_or_null(values, DataSourceConfigField::SPOOL_DIR);
    config.m_overview_dir = value_or_null(values, DataSourceConfigField::OVERVIEW_DIRECTORY);
    config.m_active_times = value_or_null(values, DataSourceConfigField::ACTIVE_TIMES_FILE);
    config.m_group_desc = value_or_null(values, DataSourceConfigField::GROUP_DESCRIPTION);
    config.m_group_desc_refetch = value_or_null(values, DataSourceConfigField::GROUP_DESC_REFETCH);
    config.m_auth_user = value_or_null(values, DataSourceConfigField::AUTH_USER);
    config.m_auth_password = value_or_null(values, DataSourceConfigField::AUTH_PASSWORD);
    config.m_auth_command = value_or_null(values, DataSourceConfigField::AUTH_COMMAND);
    config.m_xhdr_broken = value_or_null(values, DataSourceConfigField::XHDR_BROKEN);
    config.m_xrefs = value_or_null(values, DataSourceConfigField::XREFS);
    config.m_overview_format_file = value_or_null(values, DataSourceConfigField::OVERVIEW_FORMAT_FILE);
    config.m_force_auth = value_or_null(values, DataSourceConfigField::FORCE_AUTH);
    config.m_allow_legacy_auth = value_or_null(values, DataSourceConfigField::ALLOW_LEGACY_AUTH);
    config.m_starttls = value_or_null(values, DataSourceConfigField::START_TLS);
    config.m_implicit_tls = value_or_null(values, DataSourceConfigField::IMPLICIT_TLS);
    config.m_require_tls_for_auth = value_or_null(values, DataSourceConfigField::REQUIRE_TLS_FOR_AUTH);
    config.m_require_tls_for_posting = value_or_null(values, DataSourceConfigField::REQUIRE_TLS_FOR_POSTING);
    config.m_moderators_file = value_or_null(values, DataSourceConfigField::MODERATORS_FILE);
    return config;
}
