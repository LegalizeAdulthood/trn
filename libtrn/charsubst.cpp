/* charsubst.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
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
#include "trn/charsubst.h"

#include "config/config2.h"
#include "trn/trn.h"
#include "config/typedef.h"
#include "util/util2.h"

#include <cstdio>
#include <cstring>
#include <string>

/* Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced */
std::string g_charsets{"patm"};
const char *g_char_subst{};

/* TeX encoding table - gives ISO char for "x (x=32..127) */
static Uchar s_textbl[96] =
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
static char s_texchar{};

static int Latin1toASCII(Uchar *asc, const Uchar *iso, int limit, int t);

int put_subst_char(int c, int limit, bool output_ok)
{
    Uchar oc[2];
    Uchar nc[5];
    int   t;
    int   i = 0;
    switch (*g_char_subst)
    {
    case 'm':
    case 'a':
        t = *g_char_subst == 'm' ? 1 : 2;
        oc[0] = (Uchar)c;
        oc[1] = '\0';
        i = Latin1toASCII(nc, oc, sizeof nc, t);
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
            if (s_texchar && (c == '\\' || s_texchar != '\\'))
            {
                if (output_ok)
                {
                    std::putchar(s_texchar);
                }
                i++;
            }
            s_texchar = (char)c;
            break;
        }
        else if (s_texchar == '\\')
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
        else if (s_texchar == '"')
        {
            Uchar d;
            if (c < 32 || c > 128)
            {
                d = '\0';
            }
            else
            {
                d = s_textbl[c - 32];
            }
            s_texchar = '\0';
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
        /* FALL THROUGH */

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

const char *current_char_subst()
{
#ifdef USE_UTF_HACK
    static char show[50];
    const char* ics = input_charset_name();
    const char* ocs = output_charset_name();
    int maxlen = (sizeof show - 5)/2;
    if (std::strcmp(ics, ocs) == 0)
    {
        std::sprintf(show, "[%.*s]", maxlen, ics, maxlen, ocs);
    }
    else
    {
        std::sprintf(show, "[%.*s->%.*s]", maxlen, ics, maxlen, ocs);
    }
#else /*!USE_UTF_HACK */
    static const char* show;

    switch (*g_char_subst)
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
#endif
    return show;
}

int str_char_subst(char *outb, const char *inb, int limit, char_int subst)
{
    switch (subst)
    {
    case 'm':
        return Latin1toASCII((Uchar *) outb, (const Uchar *) inb, limit, 1);

    case 'a':
        return Latin1toASCII((Uchar *) outb, (const Uchar *) inb, limit, 2);

    default:
        break;
    }
    const char *s = std::strchr(inb, '\n');
    int len;
    if (s != nullptr && s - inb + 1 < limit)
    {
        len = s - inb + 1;
        limit = len + 1;
    }
    else
    {
        len = std::strlen(inb);
    }
    safe_copy(outb, inb, limit);
    return len;
}

/* The following is an adapted version of iso2asc by Markus Kuhn,
   University of Erlangen, Germany <mskuhn@immd4.uni-erlangen.de>
*/

#define ISO_TABLES 2 /* originally: 7 */

/* Conversion tables for displaying the G1 set (0xa0-0xff) of
   ISO Latin 1 (ISO 8859-1) with 7-bit ASCII characters.

   Version 1.2 -- error corrections are welcome

   Table   Purpose
     0     universal table for many languages
     1     single-spacing universal table
     2     table for Danish, Dutch, German, Norwegian and Swedish
     3     table for Danish, Finnish, Norwegian and Swedish using
           the appropriate ISO 646 variant.
     4     table with RFC 1345 codes in brackets
     5     table for printers that allow overstriking with backspace

   Markus Kuhn <mskuhn@immd4.informatik.uni-erlangen.de>                 */
/* In this version, I have taken out all tables except 1 and 2 -ot */

#define SUB nullptr       /* used if no reasonable ASCII string is possible */

static const char* s_iso2asc[ISO_TABLES][96] =
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

/*
 *  Transform an 8-bit ISO Latin 1 string iso into a 7-bit ASCII string asc
 *  readable on old terminals using conversion table t.
 *
 *  worst case: strlen(iso) == 4*strlen(asc)
 */
static int Latin1toASCII(Uchar *asc, const Uchar *iso, int limit, int t)
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
    t--;        /* offset correction -ot */
    const char **tab = s_iso2asc[t] - 0xa0;
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
