/* common.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_STRING_ALGOS_H
#define TRN_STRING_ALGOS_H

#include <ctype.h>

inline bool empty(const char *str)
{
    return str == nullptr || str[0] == '\0';
}

inline char *skip_ne(char *str, char delim)
{
    if (str)
        while (*str && *str != delim)
            ++str;
    return str;
}

inline char *skip_eq(char *str, char delim)
{
    if (str)
        while (*str && *str == delim)
            ++str;
    return str;
}

template <typename Char>
Char *skip_digits(Char *str)
{
    if (str)
        while (*str && isdigit(static_cast<unsigned char>(*str)))
            ++str;
    return str;
}

template <typename Char>
Char *skip_space(Char *str)
{
    if (str)
        while (*str && isspace(static_cast<unsigned char>(*str)))
            ++str;
    return str;
}

template <typename Char>
Char *skip_non_space(Char *str)
{
    if (str)
        while (*str && !isspace(static_cast<unsigned char>(*str)))
            ++str;
    return str;
}

template <typename Char>
Char *skip_alpha(Char *str)
{
    if (str)
        while (*str && isalpha(static_cast<unsigned char>(*str)))
            ++str;
    return str;
}

template <typename Char>
Char *skip_non_alpha(Char *str)
{
    if (str)
        while (*str && !isalpha(static_cast<unsigned char>(*str)))
            ++str;
    return str;
}

#endif
