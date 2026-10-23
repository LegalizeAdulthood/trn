/* trn/rt-select-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_SELECT_INTERNAL_H
#define TRN_RT_SELECT_INTERNAL_H

#include <trn/rt-select.h>
#include <trn/terminal.h>

#include <string>
#include <string_view>

// Internal entry points for testing purposes.

enum DisplayState
{
    DS_ASK = 1,
    DS_UPDATE,
    DS_DISPLAY,
    DS_RESTART,
    DS_STATUS,
    DS_QUIT,
    DS_DO_COMMAND,
    DS_ERROR
};

DisplayState article_commands(char_int ch);

std::string read_selector_command_for_test(char page_command, char end_command, bool at_end);
char        read_selector_escaped_command_for_test();
char        read_selector_numeric_continuation_for_test();
std::string read_selector_prompt_command_for_test(std::string_view prompt, MinorMode newmode, std::string_view dflt);
char        read_selector_followup_command_for_test(char_int ch);
std::string finish_selector_command_for_test(char_int ch, bool donewline);

#endif
