/* trn/uudecode.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_UUDECODE_H
#define TRN_UUDECODE_H

#include <trn/decode.h>

#include <cstdio>
#include <string>
#include <string_view>

// Length of a normal uuencoded line, including newline
enum
{
    UU_LENGTH = 62
};

int         uue_prescan(std::string_view text, std::string &filename, int *partp, int *totalp);
DecodeState uudecode(std::FILE *ifp, DecodeState state);

#endif
