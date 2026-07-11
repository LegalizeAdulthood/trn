/* trn/sw.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SW_H
#define TRN_SW_H

#include <cstdio>

void sw_file(char **tcbufptr);
void sw_list(char *swlist);
void decode_switch(const char *s);
void write_init_environment(std::FILE *fp);

#endif
