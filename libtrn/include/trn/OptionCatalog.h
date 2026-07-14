/* trn/OptionCatalog.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_OPTION_CATALOG_H
#define TRN_OPTION_CATALOG_H

#include <trn/IniSchema.h>
#include <trn/opt.h>
#include <trn/size_cast.h>

#include <string_view>
#include <vector>

class OptionCatalog
{
public:
    OptionCatalog();

    const IniSchema &schema() const;
    int              first_row() const;
    int              row_count() const;
    int              option_limit() const;
    bool             contains_row(int row) const;
    const IniField  &display_row(int row) const;
    bool             is_group(int row) const;
    bool             is_option(int row) const;
    std::string_view name(int row) const;
    std::string_view help(OptionIndex option) const;
    OptionIndex      option(int row) const;
    int              row_for(OptionIndex option) const;
    int              previous_group_row(int row) const;

private:
    IniSchema        m_schema;
    std::vector<int> m_rows_by_option;
};

inline const IniSchema &OptionCatalog::schema() const
{
    return m_schema;
}

inline int OptionCatalog::first_row() const
{
    return 1;
}

inline int OptionCatalog::row_count() const
{
    return static_cast<int>(m_schema.size());
}

inline int OptionCatalog::option_limit() const
{
    return size_cast<int>(m_rows_by_option);
}

inline bool OptionCatalog::contains_row(int row) const
{
    return row >= first_row() && row <= row_count();
}

inline const IniField &OptionCatalog::display_row(int row) const
{
    return m_schema.field(static_cast<std::size_t>(row - 1));
}

inline bool OptionCatalog::is_group(int row) const
{
    return contains_row(row) && display_row(row).is_group();
}

inline bool OptionCatalog::is_option(int row) const
{
    return contains_row(row) && display_row(row).is_value();
}

inline std::string_view OptionCatalog::name(int row) const
{
    return display_row(row).name();
}

inline OptionIndex OptionCatalog::option(int row) const
{
    return static_cast<OptionIndex>(display_row(row).id());
}

#endif
