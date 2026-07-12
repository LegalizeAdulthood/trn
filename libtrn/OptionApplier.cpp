/* OptionApplier.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionApplier.h>

#include <trn/OptionDraft.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

void apply_global_option(OptionIndex option, const char *value);

OptionApplier::OptionApplier() :
    OptionApplier(apply_global_option)
{
}

OptionApplier::OptionApplier(ApplyOne apply_one) :
    m_apply_one{apply_one}
{
}

void OptionApplier::apply(char **values) const
{
    char    **cursor = values;
    const int limit = ini_len(g_options_ini);
    for (int i = 1; i < limit; i++)
    {
        if (*++cursor)
        {
            apply(static_cast<OptionIndex>(i), *cursor);
        }
    }
}

void OptionApplier::apply(const OptionDraft &draft) const
{
    const int limit = std::min(static_cast<int>(draft.limit()), ini_len(g_options_ini));
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
        if (value != nullptr && g_option_saved_vals && g_option_saved_vals[i] //
            && !std::strcmp(value, g_option_saved_vals[i]))
        {
            if (g_option_saved_vals[i] != g_option_def_vals[i])
            {
                std::free(g_option_saved_vals[i]);
            }
            g_option_saved_vals[i] = nullptr;
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
