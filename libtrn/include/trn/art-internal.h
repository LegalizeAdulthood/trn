/* trn/art-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ART_INTERNAL_H
#define TRN_ART_INTERNAL_H

#include <trn/art.h>

#include <string_view>

// page_switch() return values
enum PageSwitchResult
{
    PS_NORM = 0,
    PS_ASK = 1,
    PS_RAISE = 2,
    PS_TO_END = 3
};

PageSwitchResult page_switch(std::string_view command);

#endif
