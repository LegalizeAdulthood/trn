/* OptionDraft.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/OptionDraft.h>

OptionDraft::OptionDraft(std::size_t limit) :
    m_values(limit)
{
}

void OptionDraft::clear()
{
    for (std::optional<std::string> &value : m_values)
    {
        value.reset();
    }
    m_size = 0;
}

void OptionDraft::erase(int index)
{
    if (!valid_index(index) || !m_values[index].has_value())
    {
        return;
    }
    m_values[index].reset();
    --m_size;
}

void OptionDraft::set(int index, std::string_view value)
{
    if (!valid_index(index))
    {
        return;
    }
    if (!m_values[index].has_value())
    {
        ++m_size;
    }
    m_values[index] = std::string{value};
}

bool OptionDraft::contains(int index) const
{
    return valid_index(index) && m_values[index].has_value();
}

std::optional<std::string_view> OptionDraft::value(int index) const
{
    if (!contains(index))
    {
        return std::nullopt;
    }
    return *m_values[index];
}

bool OptionDraft::valid_index(int index) const
{
    return index >= 0 && static_cast<std::size_t>(index) < m_values.size();
}
