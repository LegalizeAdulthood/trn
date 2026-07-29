/* utf.cpp - Functions for handling Unicode sort of properly
 *
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This file written 2020 by Ambrose Li
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/utf.h>

#include <config/common.h>
#include <config/string_case_compare.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

// OK - valid second and subsequent bytes in UTF-8
#define U(c) (((Uchar)(c)) & 0xFF)

#define IS_UTF8(cs)             ((cs) & 0x8000)
#define IS_SINGLE_BYTE(cs)      ((cs) & 0x4000)
#define IS_DOUBLE_BYTE(cs)      ((cs) & 0x2000)

#define IS_ISO_8859_X(cs)       (((cs) & 0x4010) == 0x4010)
#define IS_WINDOWS_125X(cs)     (((cs) & 0x4020) == 0x4020)

#define XXXXXX INVALID_CODE_POINT

static const CodePoint s_cp1252_himap[128] = {
    // clang-format off
    0x20ac, XXXXXX, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,     0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, XXXXXX, 0x017d, XXXXXX,
    XXXXXX, 0x2018, 0x2019, 0x201c, 0x201d, 0x00b7, 0x2013, 0x2014,     0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, XXXXXX, 0x017e, 0x0178,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,     0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, XXXXXX, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,     0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,     0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,     0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,     0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,     0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff,
    // clang-format on
};

static const CodePoint s_iso8859_1_himap[128] = {
    // clang-format off
    XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,     XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,
    XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,     XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,     0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, XXXXXX, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,     0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,     0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,     0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,     0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,     0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff,
    // clang-format on
};

static const CodePoint s_iso8859_15_himap[128] = {
    // clang-format off
    XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,     XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,
    XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,     XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX, XXXXXX,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x20ac, 0x00a5, 0x0160, 0x00a7,     0x0161, 0x00a9, 0x00aa, 0x00ab, 0x00ac, XXXXXX, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x017d, 0x00b5, 0x00b6, 0x00b7,     0x017e, 0x00b9, 0x00ba, 0x00bb, 0x0152, 0x0153, 0x0178, 0x00bf,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,     0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00cf,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,     0x00d8, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,     0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,     0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff,
    // clang-format on
};

struct CharsetDesc
{
    const char       *name;
    CharsetType      id;
    const CodePoint *himap;
};

static const CharsetDesc s_charset_descs[] = {
    // Tags defined in trn/utf.h go first; these are short labels for charsubst.cpp
    // clang-format off
    { CHARSET_NAME_ASCII, CHARSET_ASCII, nullptr },
    { "us-ascii", CHARSET_ASCII, nullptr },
    { "ascii", CHARSET_ASCII, nullptr },

    { CHARSET_NAME_UTF8, CHARSET_UTF8, nullptr },
    { "utf-8", CHARSET_UTF8, nullptr },
    { "utf8", CHARSET_UTF8, nullptr },

    { CHARSET_NAME_ISO8859_1, CHARSET_ISO8859_1, s_iso8859_1_himap },
    { "iso8859-1", CHARSET_ISO8859_1, s_iso8859_1_himap },
    { "iso-8859-1", CHARSET_ISO8859_1, s_iso8859_1_himap },

    { CHARSET_NAME_ISO8859_15, CHARSET_ISO8859_15, s_iso8859_15_himap },
    { "iso8859-15", CHARSET_ISO8859_15, s_iso8859_15_himap },
    { "iso-8859-15", CHARSET_ISO8859_15, s_iso8859_15_himap },

    { CHARSET_NAME_WINDOWS_1252, CHARSET_WINDOWS_1252, s_cp1252_himap },
    { "cp1252", CHARSET_WINDOWS_1252, s_cp1252_himap },
    { "windows-1252", CHARSET_WINDOWS_1252, s_cp1252_himap },

    { nullptr, CHARSET_UNKNOWN, nullptr }
    // clang-format on
};

struct GraphicState
{
    CharsetType      in;
    CharsetType      out;
    const CodePoint *himap_in;
    const CodePoint *himap_out;
};

static GraphicState s_gs = { CHARSET_UTF8, CHARSET_UTF8, nullptr, nullptr };

static CharsetType find_charset(std::string_view s)
{
    if (s.empty())
    {
        return CHARSET_UNKNOWN;
    }

    for (int i = 0;; ++i)
    {
        const char *name = s_charset_descs[i].name;
        if (name == nullptr)
        {
            break;
        }
        if (string_case_equal(s, name))
        {
            return s_charset_descs[i].id;
        }
    }
    return CHARSET_UNKNOWN;
}

static const CharsetDesc *find_charset_desc(int id)
{
    const CharsetDesc *it = nullptr;
    for (int i = 0;; i += 1)
    {
        const CharsetDesc *node = &s_charset_descs[i];
        if (node->name == nullptr)
        {
            break;
        }
        if (id == node->id)
        {
            it = node;
        }
        if (it != nullptr)
        {
            break;
        }
    }
    return it;
}

CharsetType utf_init(std::string_view from, std::string_view to)
{
    CharsetType in = find_charset(from);
    CharsetType out = find_charset(to);
    if (in != CHARSET_UNKNOWN)
    {
        s_gs.in = in;
        const CharsetDesc *node = find_charset_desc(in);
        if (node)
        {
            s_gs.himap_in = node->himap;
        }
    }
    if (out != CHARSET_UNKNOWN)
    {
        s_gs.out = out;
        const CharsetDesc *node = find_charset_desc(out);
        if (node)
        {
            s_gs.himap_out = node->himap;
        }
    }
    return in;
}

std::string_view input_charset_name()
{
    return find_charset_desc(s_gs.in)->name;
}

std::string_view output_charset_name()
{
    return find_charset_desc(s_gs.out)->name;
}

static bool is_utf8_continuation(std::string_view text, std::size_t index)
{
    return index < text.size() && (U(text[index]) & 0xC0) == 0x80;
}

static CodePoint utf8_lead(Uchar first, Uchar mask, int bits)
{
    return static_cast<CodePoint>(first & mask) << bits;
}

static CodePoint utf8_next(std::string_view text, std::size_t index, int bits)
{
    return static_cast<CodePoint>(U(text[index]) & 0x3F) << bits;
}

int byte_length_at(std::string_view text)
{
    int it = !text.empty(); // correct for ASCII
    if (!it)
    {
    }
    else if (IS_UTF8(s_gs.in))
    {
        const Uchar first = U(text.front());
        if ((first & 0x80) == 0)
        {
        }
        else if ((first & 0xE0) == 0xC0 && is_utf8_continuation(text, 1))
        {
            it = 2;
        }
        else if ((first & 0xF0) == 0xE0 && is_utf8_continuation(text, 1) && is_utf8_continuation(text, 2))
        {
            it = 3;
        }
        else if ((first & 0xF8) == 0xF0 && is_utf8_continuation(text, 1) && is_utf8_continuation(text, 2) &&
                 is_utf8_continuation(text, 3))
        {
            it = 4;
        }
        else if ((first & 0xFC) == 0xF8 && is_utf8_continuation(text, 1) && is_utf8_continuation(text, 2) &&
                 is_utf8_continuation(text, 3) && is_utf8_continuation(text, 4))
        {
            it = 5;
        }
        else if ((first & 0xFE) == 0xFC && is_utf8_continuation(text, 1) && is_utf8_continuation(text, 2) &&
                 is_utf8_continuation(text, 3) && is_utf8_continuation(text, 4) && is_utf8_continuation(text, 5))
        {
            it = 6;
        }
        else
        {
            // FIXME - invalid UTF-8
        }
    }
    else if (IS_SINGLE_BYTE(s_gs.in))
    {
    }
    else if (IS_DOUBLE_BYTE(s_gs.in))
    {
        if ((U(text.front()) & 0x80) != 0 && text.size() > 1)
        {
            it = 2;
        }
    }
    return it;
}

int byte_length_at(const char *s)
{
    if (s == nullptr)
    {
        return 0;
    }
    return byte_length_at(std::string_view{s});
}

// NOTE: correctness is not guaranteed; this is only a rough generalization
int visual_width_at(std::string_view text)
{
    CodePoint c = code_point_at(text);
    int it = 1;
    if (c == INVALID_CODE_POINT)
    {
        it = 0;
    }
    else if (IS_SINGLE_BYTE(s_gs.in))
    {
        it = 1;
    }
    else if (IS_DOUBLE_BYTE(s_gs.in))
    {
        it = (c & 0x80)? 2: 1;
    }
    else if ((c >= 0x00300 && c <= 0x0036F)     // combining diacritics
             || (c >= 0x01AB0 && c <= 0x01AFF)  //
             || (c >= 0x01DC0 && c <= 0x01DFF)  //
             || (c >= 0x0200B && c <= 0x0200F)  // zwsp, zwnj, zwj, lrm, rlm
             || (c >= 0x0202A && c <= 0x0202E)  // lre, rle, pdf, lro, rlo
             || (c >= 0x02060 && c <= 0x02064)) // wj,..., invisible plus
    {
        it = 0;
    }
    else if ((c >= 0x02E80 && c <= 0x04DBF)     // CJK misc, kana, hangul
             || (c >= 0x04E00 && c <= 0x09FFF)  // CJK ideographs
             || (c >= 0x0FE30 && c <= 0x0FE4F)  // CJK compatibility forms
             || (c >= 0x0FF00 && c <= 0x0FF60)  // CJK fullwidth forms
             || (c >= 0x0FFE0 && c <= 0x0FFE6)  //
             || (c >= 0x20000 && c <= 0x2FA1F)) // more CJK ideographs
    {
        it = 2;
    }
    return it;
}

int visual_width_at(const char *s)
{
    if (s == nullptr)
    {
        return 0;
    }
    return visual_width_at(std::string_view{s});
}

int visual_length_of(const char *s)
{
    int it = 0;
    if (s)
    {
        while (*s)
        {
            int w = byte_length_at(s);
            int v = visual_width_at(s);
            it += v;
            s += w;
        }
    }
    return it;
}

CodePoint code_point_at(std::string_view text)
{
    CodePoint it = INVALID_CODE_POINT;
    if (!text.empty())
    {
        if (IS_UTF8(s_gs.in))
        {
            const Uchar first = U(text.front());
            if ((first & 0x80) == 0)
            {
                it = first;
            }
            else if (text.size() > 1 && (first & 0xE0) == 0xC0 && is_utf8_continuation(text, 1))
            {
                it = utf8_lead(first, 0x1F, 6) | utf8_next(text, 1, 0);
            }
            else if (text.size() > 2 && (first & 0xF0) == 0xE0 && is_utf8_continuation(text, 1) &&
                     is_utf8_continuation(text, 2))
            {
                it = utf8_lead(first, 0x0F, 12) | utf8_next(text, 1, 6) | utf8_next(text, 2, 0);
            }
            else if (text.size() > 3 && (first & 0xF8) == 0xF0 && is_utf8_continuation(text, 1) &&
                     is_utf8_continuation(text, 2) && is_utf8_continuation(text, 3))
            {
                it =
                    utf8_lead(first, 0x07, 18) | utf8_next(text, 1, 12) | utf8_next(text, 2, 6) | utf8_next(text, 3, 0);
            }
            else if (text.size() > 4 && (first & 0xFC) == 0xF8 && is_utf8_continuation(text, 1) &&
                     is_utf8_continuation(text, 2) && is_utf8_continuation(text, 3) && is_utf8_continuation(text, 4))
            {
                it = utf8_lead(first, 0x03, 24) | utf8_next(text, 1, 18) | utf8_next(text, 2, 12) |
                     utf8_next(text, 3, 6) | utf8_next(text, 4, 0);
            }
            else if (text.size() > 5 && (first & 0xFE) == 0xFC && is_utf8_continuation(text, 1) &&
                     is_utf8_continuation(text, 2) && is_utf8_continuation(text, 3) && is_utf8_continuation(text, 4) &&
                     is_utf8_continuation(text, 5))
            {
                it = utf8_lead(first, 0x01, 30) | utf8_next(text, 1, 24) | utf8_next(text, 2, 18) |
                     utf8_next(text, 3, 12) | utf8_next(text, 4, 6) | utf8_next(text, 5, 0);
            }
            else
            {
                it = INVALID_CODE_POINT;
            }
        }
        else if (s_gs.in == CHARSET_ASCII)
        {
            it = U(text.front()) & 0x7F;
        }
        else if (s_gs.himap_in != nullptr)
        {
            it = U(text.front());
            if (it & 0x80)
            {
                it = s_gs.himap_in[it & 0x7F];
            }
        }
    }
    return it;
}

CodePoint code_point_at(const char *s)
{
    if (s == nullptr)
    {
        return INVALID_CODE_POINT;
    }
    return code_point_at(std::string_view{s});
}

static std::string utf8_text(CodePoint c)
{
    std::string text;
    text.reserve(6);

    if (c <= 0x0000007F)
    {
        text.push_back(static_cast<char>(c));
    }
    else if (c <= 0x000007FF)
    {
        text.push_back(static_cast<char>(((c >> 6) & 0x1F) | 0xC0));
        text.push_back(static_cast<char>((c & 0x3F) | 0x80));
    }
    else if (c <= 0x0000FFFF)
    {
        text.push_back(static_cast<char>(((c >> 12) & 0x1F) | 0xE0));
        text.push_back(static_cast<char>(((c >> 6) & 0x3F) | 0x80));
        text.push_back(static_cast<char>((c & 0x3F) | 0x80));
    }
    else if (c <= 0x001FFFFF)
    {
        text.push_back(static_cast<char>(((c >> 18) & 0x1F) | 0xF0));
        text.push_back(static_cast<char>(((c >> 12) & 0x3F) | 0x80));
        text.push_back(static_cast<char>(((c >> 6) & 0x3F) | 0x80));
        text.push_back(static_cast<char>((c & 0x3F) | 0x80));
    }
    else if (c <= 0x03FFFFFF)
    {
        text.push_back(static_cast<char>(((c >> 24) & 0x1F) | 0xF8));
        text.push_back(static_cast<char>(((c >> 18) & 0x3F) | 0x80));
        text.push_back(static_cast<char>(((c >> 12) & 0x3F) | 0x80));
        text.push_back(static_cast<char>(((c >> 6) & 0x3F) | 0x80));
        text.push_back(static_cast<char>((c & 0x3F) | 0x80));
    }
    else if (c <= 0x7FFFFFFF)
    {
        text.push_back(static_cast<char>(((c >> 30) & 0x1F) | 0xFC));
        text.push_back(static_cast<char>(((c >> 24) & 0x3F) | 0x80));
        text.push_back(static_cast<char>(((c >> 18) & 0x3F) | 0x80));
        text.push_back(static_cast<char>(((c >> 12) & 0x3F) | 0x80));
        text.push_back(static_cast<char>(((c >> 6) & 0x3F) | 0x80));
        text.push_back(static_cast<char>((c & 0x3F) | 0x80));
    }
    return text;
}

std::string insert_unicode_at(CodePoint c)
{
    if (IS_UTF8(s_gs.in))
    {
        return utf8_text(c);
    }
    if (c <= 0x7F)
    {
        return std::string(1, static_cast<char>(c));
    }
    if (s_gs.himap_in != nullptr)
    {
        return utf8_text(s_gs.himap_in[c & 0x7F]);
    }
    return {};
}

bool at_norm_char(std::string_view s)
{
    bool it = !s.empty();
    if (it)
    {
        const char ch = s.front();
        if (s_gs.in == CHARSET_UTF8)
        {
            CodePoint c = code_point_at(s);
            it = c >= 0x20 && !(c >= 0x7F && c < 0xA0) && c != 0x2028 && c != 0x2029;
        }
        else if (U(ch) < 0x80)
        {
            it = (U(ch) >= ' ' && U(ch) < 0x7F);
        }
        else if (s_gs.himap_in != nullptr)
        {
            it = s_gs.himap_in[U(ch) & 0x7F] != INVALID_CODE_POINT;
        }
    }
    return it;
}

bool at_norm_char(const char *s)
{
    bool it = s != nullptr;
    if (it)
    {
        it = *s != 0;
    }
    if (it)
    {
        if (s_gs.in == CHARSET_UTF8)
        {
            CodePoint c = code_point_at(s);
            it = c >= 0x20 && !(c >= 0x7F && c < 0xA0) && c != 0x2028 && c != 0x2029;
        }
        else
        {
            it = at_norm_char(std::string_view{s, 1});
        }
    }
    return it;
}

int put_char_adv(std::string_view &text, bool outputok)
{
    int it = 0;
    if (!text.empty())
    {
        const char ch = text.front();
        if (s_gs.in == CHARSET_UTF8 || (ch >= ' ' && ch < 0x7F))
        {
            const int w = byte_length_at(text);
            it = visual_width_at(text);
            if (outputok)
            {
                for (int i = 0; i < w; i += 1)
                {
                    std::putchar(text[static_cast<std::size_t>(i)]);
                }
            }
            text.remove_prefix(static_cast<std::size_t>(w));
        }
        else if (s_gs.himap_in)
        {
            std::string encoded_text = utf8_text(s_gs.himap_in[U(ch) & 0x7F]);
            for (char byte : encoded_text)
            {
                std::putchar(byte);
            }
            it = 1;
            text.remove_prefix(1);
        }
    }
    return it;
}

std::string create_utf8_copy(const char *s)
{
    std::string result;
    if (s == nullptr)
    {
        return result;
    }

    // Precalculate size of required space
    std::size_t tlen = 0;
    for (int slen = 0; s[slen];)
    {
        const int         sw = byte_length_at(s + slen);
        const std::string encoded_text = utf8_text(code_point_at(s + slen));
        slen += sw;
        tlen += encoded_text.size();
    }

    result.reserve(tlen);

    // Create the actual copy
    for (int i = 0; s[i];)
    {
        result += utf8_text(code_point_at(s + i));
        i += byte_length_at(s + i);
    }
    return result;
}

void terminate_string_at_visual_index(char *s, int i)
{
    if (s)
    {
        int j;
        for (j = 0; *s;)
        {
            int w = byte_length_at(s);
            int v = visual_width_at(s);
            if (w == 0 || j + v > i)
            {
                break;
            }
            s += w;
            j += v;
        }
        if (j + 1 == i && *s)
        {
            *s++ = ' ';
        }
        if (*s)
        {
            *s = '\0';
        }
    }
}
