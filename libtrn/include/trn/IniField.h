/* trn/IniField.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_FIELD_H
#define TRN_INI_FIELD_H

#include <string>
#include <string_view>

class IniField
{
public:
    enum class Kind
    {
        VALUE,
        GROUP
    };

    IniField(int id, std::string_view name, std::string_view help = {});

    static IniField group(std::string_view name);
    static IniField value(int id, std::string_view name, std::string_view help = {});

    int              id() const;
    std::string_view name() const;
    std::string_view help() const;
    Kind             kind() const;
    bool             is_value() const;
    bool             is_group() const;

private:
    IniField(int id, std::string_view name, std::string_view help, Kind kind);

    int         m_id;
    std::string m_name;
    std::string m_help;
    Kind        m_kind;
};

inline int IniField::id() const
{
    return m_id;
}

inline std::string_view IniField::name() const
{
    return m_name;
}

inline std::string_view IniField::help() const
{
    return m_help;
}

inline IniField::Kind IniField::kind() const
{
    return m_kind;
}

inline bool IniField::is_value() const
{
    return m_kind == Kind::VALUE;
}

inline bool IniField::is_group() const
{
    return m_kind == Kind::GROUP;
}

#endif
