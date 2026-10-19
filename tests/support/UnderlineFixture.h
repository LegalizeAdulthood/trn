// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#pragma once

#include <string_view>

// Unfolded subject from the article attached to issue #30:
// https://github.com/LegalizeAdulthood/trn/issues/30
inline constexpr std::string_view UNDERLINE_SUBJECT{"=?UTF-8?Q?=E2=80=9CMicrosoft_CEO_says_up_to_30=25_of_the_company?="
                                                    " =?UTF-8?B?4oCZcyBjb2RlIHdhcyB3cml0dGVuIGJ5IEFJ4oCd?="};
inline constexpr std::string_view UNDERLINE_DECODED_SUBJECT{
    "\xE2\x80\x9CMicrosoft CEO says up to 30% of the company\xE2\x80\x99s code was written by AI\xE2\x80\x9D"};
