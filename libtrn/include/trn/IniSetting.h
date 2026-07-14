/* trn/IniSetting.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_INI_SETTING_H
#define TRN_INI_SETTING_H

#include <string>
#include <string_view>

class IniSetting
{
public:
    IniSetting() = default;
    IniSetting(std::string_view name, std::string_view raw_value);

    std::string_view name() const;
    std::string      value() const;

private:
    std::string_view m_name;
    std::string_view m_raw_value;
};

inline std::string_view IniSetting::name() const
{
    return m_name;
}

#endif
