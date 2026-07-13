/* trn/RcGroupConfig.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RC_GROUP_CONFIG_H
#define TRN_RC_GROUP_CONFIG_H

class IniSchema;
class IniSectionValues;

#include <optional>
#include <string>

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

    const char *id() const;
    const char *newsrc() const;
    const char *add_groups() const;

    void set_id(const char *value);
    void set_newsrc(const char *value);
    void set_add_groups(const char *value);

private:
    static const char *c_str(const std::optional<std::string> &value);
    static void        set_value(std::optional<std::string> &target, const char *value);

    std::optional<std::string> m_id;
    std::optional<std::string> m_newsrc;
    std::optional<std::string> m_add_groups;
};

inline const char *RcGroupConfig::id() const
{
    return c_str(m_id);
}

inline const char *RcGroupConfig::newsrc() const
{
    return c_str(m_newsrc);
}

inline const char *RcGroupConfig::add_groups() const
{
    return c_str(m_add_groups);
}

inline void RcGroupConfig::set_id(const char *value)
{
    set_value(m_id, value);
}

inline void RcGroupConfig::set_newsrc(const char *value)
{
    set_value(m_newsrc, value);
}

inline void RcGroupConfig::set_add_groups(const char *value)
{
    set_value(m_add_groups, value);
}

#endif
