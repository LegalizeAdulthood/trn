/* trn/ActiveRecordEditor.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ACTIVE_RECORD_EDITOR_H
#define TRN_ACTIVE_RECORD_EDITOR_H

#include <config/typedef.h>

#include <cstddef>
#include <string>
#include <string_view>

struct ActiveRecordEditor
{
    void       update_low(std::string_view source);
    ArticleNum high() const;

    std::string &text;
    std::size_t  name_len;
};

#endif
