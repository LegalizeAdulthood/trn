// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NG_INTERNAL_H
#define TRN_NG_INTERNAL_H

#include <trn/ng.h>

#include <string_view>

// Internal entry points for testing purposes.

std::string_view ng_unread_prompt_for_test(bool has_current_article, bool verbose);
std::string_view ng_unread_thread_help_for_test(bool has_current_article, bool verbose);

#endif
