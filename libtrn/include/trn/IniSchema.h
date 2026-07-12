/* trn/IniSchema.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_SCHEMA_H
#define TRN_INI_SCHEMA_H

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class IniField
{
public:
    enum class Kind
    {
        Value,
        Group
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

class IniSchema
{
public:
    using Fields = std::vector<IniField>;

    IniSchema(std::string_view section_name, std::initializer_list<IniField> fields);

    std::string_view section_name() const;
    const Fields    &fields() const;
    std::size_t      size() const;
    const IniField  &field(std::size_t index) const;
    const IniField  *find(std::string_view name) const;

private:
    static std::string lookup_key(std::string_view name);

    std::string                                  m_section_name;
    Fields                                       m_fields;
    std::unordered_map<std::string, std::size_t> m_lookup;
};

inline int IniField::id() const
{
    return m_id;
}

inline std::string_view IniField::name() const
{
    return std::string_view{m_name.data(), m_name.size()};
}

inline std::string_view IniField::help() const
{
    return std::string_view{m_help.data(), m_help.size()};
}

inline IniField::Kind IniField::kind() const
{
    return m_kind;
}

inline bool IniField::is_value() const
{
    return m_kind == Kind::Value;
}

inline bool IniField::is_group() const
{
    return m_kind == Kind::Group;
}

inline std::string_view IniSchema::section_name() const
{
    return std::string_view{m_section_name.data(), m_section_name.size()};
}

inline const IniSchema::Fields &IniSchema::fields() const
{
    return m_fields;
}

inline std::size_t IniSchema::size() const
{
    return m_fields.size();
}

inline const IniField &IniSchema::field(std::size_t index) const
{
    return m_fields[index];
}

#endif
