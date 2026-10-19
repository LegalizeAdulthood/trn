/* charsubst.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// Copyright (c) 2026, Richard Thomson
/*
 * Permission is hereby granted to copy, reproduce, redistribute or otherwise
 * use this software as long as: there is no monetary profit gained
 * specifically from the use or reproduction of this software, it is not
 * sold, rented, traded or otherwise marketed, and this copyright notice is
 * included prominently in any copy made.
 *
 * The authors make no claims as to the fitness or correctness of this software
 * for any use whatsoever, and it is provided as is. Any use of this software
 * is at the user's own risk.
 */

#include <trn/charsubst.h>

#include <config/config2.h>
#include <trn/trn.h>
#include <trn/utf.h>

#include <string>
#include <string_view>

// Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced
std::string g_charsets{"patm"};

namespace
{
constexpr std::string::size_type NO_CHAR_SUBST = std::string::npos;
std::string::size_type           s_char_subst_index{NO_CHAR_SUBST};

void normalize_char_subst_mode()
{
    if (g_charsets.empty())
    {
        s_char_subst_index = NO_CHAR_SUBST;
    }
    else if (s_char_subst_index != NO_CHAR_SUBST && s_char_subst_index >= g_charsets.size())
    {
        s_char_subst_index = 0;
    }
}
} // namespace

// TeX encoding table - gives a Unicode scalar for "x (x=32..127).
static constexpr CodePoint s_tex_tbl[96] =
    // clang-format off
{
    0,  0,'"',  0,  0,  0,  0,'"',  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,196,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,214,
    0,  0,  0,  0,  0,220,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  '"',228,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,246,
    0,  0,  0,223,  0,252,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
// clang-format on
char current_char_subst_mode()
{
    normalize_char_subst_mode();
    return s_char_subst_index == NO_CHAR_SUBST ? '\0' : g_charsets[s_char_subst_index];
}

void set_char_subst_mode(char mode)
{
    if (mode == '\0' || g_charsets.empty())
    {
        s_char_subst_index = NO_CHAR_SUBST;
        return;
    }

    const std::string::size_type mode_pos = g_charsets.find(mode);
    if (mode_pos == std::string::npos)
    {
        reset_char_subst_mode();
        return;
    }

    s_char_subst_index = mode_pos;
}

void reset_char_subst_mode()
{
    s_char_subst_index = g_charsets.empty() ? NO_CHAR_SUBST : 0;
}

void next_char_subst_mode()
{
    normalize_char_subst_mode();
    if (s_char_subst_index == NO_CHAR_SUBST)
    {
        reset_char_subst_mode();
        return;
    }
    ++s_char_subst_index;
    if (s_char_subst_index >= g_charsets.size())
    {
        reset_char_subst_mode();
    }
}

std::string current_char_subst()
{
    std::string_view show;

    switch (current_char_subst_mode())
    {
    case 'm':
        show = g_verbose ? "[ISO->USmono] " : "[M] ";
        break;

    case 'a':
        show = g_verbose ? "[ISO->US] " : "[U] ";
        break;

    case 't':
        show = g_verbose ? "[TeX->ISO] " : "[T] ";
        break;

    default:
        show = "";
        break;
    }
    return std::string{show};
}

// The following is an adapted version of iso2asc by Markus Kuhn,
//   University of Erlangen, Germany <mskuhn@immd4.uni-erlangen.de>

#define ISO_TABLES 2 // originally: 7

// Conversion tables for Unicode U+00A0..U+00FF (the ISO Latin 1 G1 set)
//   with 7-bit ASCII characters.
//
//   Version 1.2 -- error corrections are welcome
//
//   Table   Purpose
//     0     universal table for many languages
//     1     single-spacing universal table
//     2     table for Danish, Dutch, German, Norwegian and Swedish
//     3     table for Danish, Finnish, Norwegian and Swedish using
//           the appropriate ISO 646 variant.
//     4     table with RFC 1345 codes in brackets
//     5     table for printers that allow overstriking with backspace
//
//   Markus Kuhn <mskuhn@immd4.informatik.uni-erlangen.de>
//
// In this version, I have taken out all tables except 1 and 2 -ot

#define SUB nullptr       // used if no reasonable ASCII string is possible

static constexpr const char *s_iso_to_ascii[ISO_TABLES][96] =
    // clang-format off
{
    {
        " ", "!", "c", SUB, SUB, "Y", "|", SUB, "\"", "c", "a", "<", "-", "-", "R", "-",
        " ", SUB, "2", "3", "'", "u", "P", ".", ",",  "1", "o", ">", SUB, SUB, SUB, "?",
        "A", "A", "A", "A", "A", "A", "A", "C", "E",  "E", "E", "E", "I", "I", "I", "I",
        "D", "N", "O", "O", "O", "O", "O", "x", "O",  "U", "U", "U", "U", "Y", "T", "s",
        "a", "a", "a", "a", "a", "a", "a", "c", "e",  "e", "e", "e", "i", "i", "i", "i",
        "d", "n", "o", "o", "o", "o", "o", ":", "o",  "u", "u", "u", "u", "y", "t", "y"
    },
    {
        " ", "!",   "c", SUB, SUB,  "Y",  "|",  SUB, "\"", "(c)", "a", "<<", "-",    "-",    "(R)",  "-",
        " ", "+/-", "2", "3", "'",  "u",  "P",  ".", ",",  "1",   "o", ">>", " 1/4", " 1/2", " 3/4", "?",
        "A", "A",   "A", "A", "Ae", "Aa", "AE", "C", "E",  "E",   "E", "E",  "I",    "I",    "I",    "I",
        "D", "N",   "O", "O", "O",  "O",  "Oe", "x", "Oe", "U",   "U", "U",  "Ue",   "Y",    "Th",   "ss",
        "a", "a",   "a", "a", "ae", "aa", "ae", "c", "e",  "e",   "e", "e",  "i",    "i",    "i",    "i",
        "d", "n",   "o", "o", "o",  "o",  "oe", ":", "oe", "u",   "u", "u",  "ue",   "y",    "th",   "ij"
    }
};
// clang-format on

CharSubstitution::CharSubstitution(char_int mode) :
    m_mode{mode}
{
}

std::string CharSubstitution::substitute(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    while (!input.empty())
    {
        const Utf8Scalar       decoded = decode_utf8_scalar(input);
        const CodePoint        scalar = decoded.code_point == INVALID_CODE_POINT ? 0xFFFD : decoded.code_point;
        const std::string_view original = input.substr(0, decoded.byte_count);
        input.remove_prefix(decoded.byte_count);
        if ((m_mode == 'a' || m_mode == 'm') && scalar >= 0xA0 && scalar <= 0xFF)
        {
            const char *const replacement = s_iso_to_ascii[m_mode == 'm' ? 0 : 1][scalar - 0xA0];
            if (replacement != nullptr)
            {
                result += replacement;
            }
            continue;
        }
        if (m_mode == 't')
        {
            if (scalar == '\\' || scalar == '"')
            {
                if (m_tex_prefix && (scalar == '\\' || m_tex_prefix != '\\'))
                {
                    result.push_back(m_tex_prefix);
                }
                m_tex_prefix = static_cast<char>(scalar);
                continue;
            }
            const char prefix = m_tex_prefix;
            m_tex_prefix = '\0';
            if (prefix == '"' && scalar >= 32 && scalar < 128)
            {
                const CodePoint replacement = s_tex_tbl[scalar - 32];
                if (replacement != 0)
                {
                    result += encode_utf8_scalar(replacement);
                    continue;
                }
            }
            if (prefix)
            {
                result.push_back(prefix);
            }
        }
        if (decoded.code_point == INVALID_CODE_POINT)
        {
            result += encode_utf8_scalar(scalar);
        }
        else
        {
            result += original;
        }
    }
    return result;
}

std::string CharSubstitution::finish()
{
    std::string result;
    if (m_tex_prefix)
    {
        result.push_back(m_tex_prefix);
        m_tex_prefix = '\0';
    }
    return result;
}

std::string str_char_subst(std::string_view input, char_int subst)
{
    const std::size_t newline = input.find('\n');
    if (newline != std::string_view::npos)
    {
        input = input.substr(0, newline + 1);
    }
    CharSubstitution substitution{subst};
    std::string      result = substitution.substitute(input);
    result += substitution.finish();
    return result;
}
