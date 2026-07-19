/* trn/OptionDraft.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_OPTION_DRAFT_H
#define TRN_OPTION_DRAFT_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class OptionDraft
{
public:
    explicit OptionDraft(std::size_t limit);

    void        clear();
    void        erase(int index);
    void        set(int index, std::string_view value);
    bool        contains(int index) const;
    std::optional<std::string_view> value(int index) const;
    std::size_t size() const;
    std::size_t limit() const;

private:
    bool valid_index(int index) const;

    std::vector<std::optional<std::string>> m_values;
    std::size_t                             m_size{};
};

inline std::size_t OptionDraft::size() const
{
    return m_size;
}

inline std::size_t OptionDraft::limit() const
{
    return m_values.size();
}

#endif
