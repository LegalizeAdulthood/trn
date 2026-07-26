/* trn/scmd.h
 *
 * Scan command interpreter/router
 */
// This file is Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCMD_H
#define TRN_SCMD_H

#include <config/config2.h>

#include <string>
#include <string_view>

void s_go_bot();
std::string s_finish_cmd(std::string_view prompt, std::string_view command);
int s_cmd_loop();

#endif
