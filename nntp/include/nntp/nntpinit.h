/* nntptrn/init.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NNTPINIT_H
#define TRN_NNTPINIT_H

#include <string_view>

int init_nntp();
int server_init(std::string_view machine);
void cleanup_nntp();

#endif
