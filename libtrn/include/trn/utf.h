/* trn/utf.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef UTF_H_INCLUDED
#define UTF_H_INCLUDED

#include <cstddef>
#include <string>
#include <string_view>

#define USE_UTF_HACK

enum CharsetType
{
    CHARSET_ASCII = 0x0000,
    CHARSET_UTF8 = 0x8000,
    CHARSET_ISO8859_1 = 0x4010,
    CHARSET_ISO8859_15 = 0x4011,
    CHARSET_WINDOWS_1252 = 0x4020,
    CHARSET_UNKNOWN = 0x0FFF
};

#define CHARSET_NAME_ASCII          "US"
#define CHARSET_NAME_UTF8           "UTF"
#define CHARSET_NAME_ISO8859_1      "Latin1"
#define CHARSET_NAME_ISO8859_15     "Latin9"
#define CHARSET_NAME_WINDOWS_1252   "CP1252"

using CodePoint = unsigned long;

CharsetType      utf_init(std::string_view from, std::string_view to);
std::string_view input_charset_name();
std::string_view output_charset_name();

bool at_norm_char(std::string_view s);

int         byte_length_at(std::string_view s);
int         visual_width_at(std::string_view s);
int         visual_length_of(std::string_view s);
std::string insert_unicode_at(CodePoint c);

enum : CodePoint
{
    INVALID_CODE_POINT = static_cast<CodePoint>(~0L)
};
CodePoint code_point_at(std::string_view s);

struct Utf8Scalar
{
    CodePoint   code_point{INVALID_CODE_POINT};
    std::size_t byte_count{};
};

// Pure modern UTF-8 operations; independent of utf_init and charset state.
// Empty input returns {INVALID_CODE_POINT, 0}; malformed input returns
// {INVALID_CODE_POINT, 1}, preserving the remaining bytes for the caller.
Utf8Scalar decode_utf8_scalar(std::string_view text) noexcept;
// Invalid scalars (surrogates or values above U+10FFFF) return no bytes.
std::string encode_utf8_scalar(CodePoint scalar);

int put_char_adv(std::string_view &text, bool outputok);

std::string create_utf8_copy(std::string_view s);

#endif
