/* trn/IniSectionValues.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_SECTION_VALUES_H
#define TRN_INI_SECTION_VALUES_H

#include <trn/IniSchema.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

class IniSectionValues
{
public:
    void                            reset();
    bool                            set(const IniField &field, std::string_view value);
    bool                            contains(int field_id) const;
    const char                     *c_str(int field_id) const;
    std::optional<std::string_view> value(int field_id) const;
    std::size_t                     size() const;

private:
    std::unordered_map<int, std::string> m_values;
};

inline std::size_t IniSectionValues::size() const
{
    return m_values.size();
}

#endif
