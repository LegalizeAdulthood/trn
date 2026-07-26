/* trn/respond-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RESPOND_INTERNAL_H
#define TRN_RESPOND_INTERNAL_H

#include <string_view>

std::string_view respond_parse_extract_options_for_test(std::string_view command_text, int &part_opt, int &total_opt);

#endif
