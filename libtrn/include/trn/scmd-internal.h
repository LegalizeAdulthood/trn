// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCMD_INTERNAL_H
#define TRN_SCMD_INTERNAL_H

#include <trn/scmd.h>

#include <string_view>

// Internal entry points for testing purposes.

bool scmd_match_description_for_test(long ent, std::string_view search_text);

#endif
