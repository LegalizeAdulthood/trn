/* trn/RcGroupConfig.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RC_GROUP_CONFIG_H
#define TRN_RC_GROUP_CONFIG_H

class IniSchema;
class IniSectionValues;

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
    const char *m_id{};
    const char *m_newsrc{};
    const char *m_add_groups{};
};

inline const char *RcGroupConfig::id() const
{
    return m_id;
}

inline const char *RcGroupConfig::newsrc() const
{
    return m_newsrc;
}

inline const char *RcGroupConfig::add_groups() const
{
    return m_add_groups;
}

inline void RcGroupConfig::set_id(const char *value)
{
    m_id = value;
}

inline void RcGroupConfig::set_newsrc(const char *value)
{
    m_newsrc = value;
}

inline void RcGroupConfig::set_add_groups(const char *value)
{
    m_add_groups = value;
}

#endif
