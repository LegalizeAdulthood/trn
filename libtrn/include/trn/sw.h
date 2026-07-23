/* trn/sw.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SW_H
#define TRN_SW_H

#include <cstdio>
#include <string_view>

void sw_file(std::string_view filename);
void sw_list(std::string_view switches);
void decode_switch(std::string_view s);
void write_init_environment(std::FILE *fp);

#endif
