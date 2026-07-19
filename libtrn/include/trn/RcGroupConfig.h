/* trn/RcGroupConfig.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RC_GROUP_CONFIG_H
#define TRN_RC_GROUP_CONFIG_H

class IniSchema;
class IniSectionValues;

#include <string>
#include <string_view>

enum class RcGroupConfigField
{
    Id = 1,
    Newsrc,
    AddGroups
};

class RcGroupConfig
{
public:
    static const IniSchema &schema();
    static RcGroupConfig    from(const IniSectionValues &values);

    std::string_view id() const;
    std::string_view newsrc() const;
    std::string_view add_groups() const;

    void set_id(std::string_view value);
    void set_newsrc(std::string_view value);
    void set_add_groups(std::string_view value);

private:
    std::string m_id;
    std::string m_newsrc;
    std::string m_add_groups;
};

inline std::string_view RcGroupConfig::id() const
{
    return m_id;
}

inline std::string_view RcGroupConfig::newsrc() const
{
    return m_newsrc;
}

inline std::string_view RcGroupConfig::add_groups() const
{
    return m_add_groups;
}

inline void RcGroupConfig::set_id(std::string_view value)
{
    m_id = value;
}

inline void RcGroupConfig::set_newsrc(std::string_view value)
{
    m_newsrc = value;
}

inline void RcGroupConfig::set_add_groups(std::string_view value)
{
    m_add_groups = value;
}

#endif
