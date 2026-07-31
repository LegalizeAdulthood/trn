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
#include <config/typedef.h>
#include <trn/trn.h>

#include <fmt/format.h>

#include <cstdio>
#include <string>
#include <string_view>

// Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced
std::string g_charsets{"patm"};
const char *g_char_subst{};

// TeX encoding table - gives ISO char for "x (x=32..127)
static Uchar s_tex_tbl[96] =
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
static char s_tex_char{};

static int put_subst_char(int c, int limit, bool output_ok);
static int latin1_to_ascii(Uchar *asc, const Uchar *iso, int limit, int t);

static int put_subst_char(int c, int limit, bool output_ok)
{
    Uchar oc[2];
    Uchar nc[5];
    int   t;
    int   i = 0;
    const char subst = current_char_subst_mode();
    switch (subst)
    {
    case 'm':
    case 'a':
        t = subst == 'm' ? 1 : 2;
        oc[0] = (Uchar)c;
        oc[1] = '\0';
        i = latin1_to_ascii(nc, oc, sizeof nc, t);
        if (i <= limit)
        {
            if (output_ok)
            {
                for (int t2 = 0; t2 < i; t2++)
                {
                    std::putchar((char) nc[t2]);
                }
            }
        }
        else
        {
            i = -1;
        }
        break;

    case 't':
        if (c == '\\' || c == '"')
        {
            if (s_tex_char && (c == '\\' || s_tex_char != '\\'))
            {
                if (output_ok)
                {
                    std::putchar(s_tex_char);
                }
                i++;
            }
            s_tex_char = (char)c;
            break;
        }
        else if (s_tex_char == '\\')
        {
            if (output_ok)
            {
                std::putchar('\\');
            }
            if (limit == 1)
            {
                i = -2;
                break;
            }
            i++;
        }
        else if (s_tex_char == '"')
        {
            Uchar d;
            if (c < 32 || c > 128)
            {
                d = '\0';
            }
            else
            {
                d = s_tex_tbl[c - 32];
            }
            s_tex_char = '\0';
            if (d)
            {
                c = d;
            }
            else
            {
                if (output_ok)
                {
                    std::putchar('"');
                }
                if (limit == 1)
                {
                    i = -2;
                    break;
                }
                i++;
            }
        }
        // FALL THROUGH

    default:
        if (output_ok)
        {
            std::putchar(c);
        }
        i++;
        break;
    }
    return i;
}

char current_char_subst_mode()
{
    return g_char_subst == nullptr ? '\0' : *g_char_subst;
}

void set_char_subst_mode(char mode)
{
    if (mode == '\0')
    {
        g_char_subst = nullptr;
        return;
    }

    const std::string::size_type mode_pos = g_charsets.find(mode);
    if (mode_pos == std::string::npos)
    {
        reset_char_subst_mode();
        return;
    }

    g_char_subst = g_charsets.c_str() + mode_pos;
}

void reset_char_subst_mode()
{
    g_char_subst = g_charsets.c_str();
}

void next_char_subst_mode()
{
    if (g_char_subst == nullptr)
    {
        reset_char_subst_mode();
        return;
    }
    ++g_char_subst;
    if (*g_char_subst == '\0')
    {
        reset_char_subst_mode();
    }
}

std::string current_char_subst()
{
#ifdef USE_UTF_HACK
    std::string_view input = input_charset_name();
    std::string_view output = output_charset_name();
    constexpr int    maxlen = (50 - 5) / 2;
    input = input.substr(0, maxlen);
    output = output.substr(0, maxlen);
    if (input == output)
    {
        return fmt::format("[{}]", input);
    }
    return fmt::format("[{}->{}]", input, output);
#else // !USE_UTF_HACK
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
#endif
}

// The following is an adapted version of iso2asc by Markus Kuhn,
//   University of Erlangen, Germany <mskuhn@immd4.uni-erlangen.de>

#define ISO_TABLES 2 // originally: 7

// Conversion tables for displaying the G1 set (0xa0-0xff) of
//   ISO Latin 1 (ISO 8859-1) with 7-bit ASCII characters.
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

static const char* s_iso_to_ascii[ISO_TABLES][96] =
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

std::string str_char_subst(std::string_view input, char_int subst)
{
    if (subst != 'a' && subst != 'm')
    {
        const std::size_t newline = input.find('\n');
        if (newline != std::string_view::npos)
        {
            input = input.substr(0, newline + 1);
        }
        return std::string{input};
    }

    const char *const *table = s_iso_to_ascii[subst == 'm' ? 0 : 1];
    std::string        result;
    result.reserve(input.size() * 4);
    for (const char value : input)
    {
        const Uchar ch = static_cast<Uchar>(value);
        if (ch > 0x9f)
        {
            const char *const replacement = table[ch - 0xa0];
            if (replacement != nullptr)
            {
                result += replacement;
            }
        }
        else
        {
            result.push_back(ch < 0x80 ? static_cast<char>(ch) : ' ');
        }
    }
    return result;
}

//
//  Transform an 8-bit ISO Latin 1 string iso into a 7-bit ASCII string asc
//  readable on old terminals using conversion table t.
//
//  worst case: strlen(iso) == 4*strlen(asc)
//
static int latin1_to_ascii(Uchar *asc, const Uchar *iso, int limit, int t)
{
    Uchar *s = asc;

    if (iso == nullptr || asc == nullptr || limit <= 0)
    {
        return 0;
    }
    if (limit == 1)
    {
        *s = '\0';
        return s - asc;
    }
    t--;        // offset correction -ot
    const char **tab = s_iso_to_ascii[t] - 0xa0;
    while (*iso)
    {
        if (*iso > 0x9f)
        {
            const char *p = tab[*iso++];
            if (p)
            {
                while (*p)
                {
                    *s++ = *p++;
                    if (!--limit)
                    {
                        *s = '\0';
                        return s - asc;
                    }
                }
            }
        }
        else
        {
            if (*iso < 0x80)
            {
                *s++ = *iso++;
            }
            else
            {
                *s++ = ' ';
                iso++;
            }
            if (!--limit)
            {
                break;
            }
        }
    }
    *s = '\0';
    return s - asc;
}
