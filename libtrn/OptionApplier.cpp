/* OptionApplier.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionApplier.h>

#include <trn/IniSectionValues.h>
#include <trn/OptionCatalog.h>
#include <trn/OptionDraft.h>

void apply_global_option(OptionIndex option, const char *value);

OptionApplier::OptionApplier() :
    OptionApplier(apply_global_option)
{
}

OptionApplier::OptionApplier(ApplyOne apply_one) :
    m_apply_one{apply_one}
{
}

void OptionApplier::apply(const IniSectionValues &values) const
{
    const OptionCatalog catalog;
    for (int row = catalog.first_row(); row <= catalog.row_count(); ++row)
    {
        if (catalog.is_option(row))
        {
            const OptionIndex option = catalog.option(row);
            if (const char *value = values.c_str(option); value != nullptr)
            {
                apply(option, value);
            }
        }
    }
}

void OptionApplier::apply(const OptionDraft &draft) const
{
    const int limit = static_cast<int>(draft.limit());
    for (int i = 1; i < limit; i++)
    {
        if (const char *value = draft.value(i); value != nullptr)
        {
            apply(static_cast<OptionIndex>(i), value);
        }
    }
    for (int i = 1; i < limit; i++)
    {
        const char *value = draft.value(i);
        if (value != nullptr && !g_option_saved_vals.empty() && g_option_saved_vals[i] //
            && *g_option_saved_vals[i] == value)
        {
            g_option_saved_vals[i].reset();
        }
    }
}

void OptionApplier::apply(OptionIndex option, const char *value) const
{
    if (m_apply_one != nullptr)
    {
        m_apply_one(option, value);
    }
}
