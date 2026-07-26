/* trn/rt-select-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_SELECT_INTERNAL_H
#define TRN_RT_SELECT_INTERNAL_H

#include <trn/rt-select.h>

#include <string>

// Internal entry points for testing purposes.

std::string read_selector_command_for_test(char page_command, char end_command, bool at_end);
char        read_selector_escaped_command_for_test();
char        read_selector_numeric_continuation_for_test();

#endif
