/* common.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_STRING_ALGOS_H
#define TRN_STRING_ALGOS_H

#include <cctype>

inline bool empty(const char *str)
{
    return str == nullptr || str[0] == '\0';
}

template <typename Char>
Char *skip_ne(Char *str, char delim)
{
    if (str)
    {
        while (*str && *str != delim)
        {
            ++str;
        }
    }
    return str;
}

template <typename Char>
Char *skip_eq(Char *str, char delim)
{
    if (str)
    {
        while (*str && *str == delim)
        {
            ++str;
        }
    }
    return str;
}

template <typename Char>
Char *skip_digits(Char *str)
{
    if (str)
    {
        while (*str && std::isdigit(static_cast<unsigned char>(*str)))
        {
            ++str;
        }
    }
    return str;
}

template <typename Char>
Char *skip_space(Char *str)
{
    if (str)
    {
        while (*str && std::isspace(static_cast<unsigned char>(*str)))
        {
            ++str;
        }
    }
    return str;
}

template <typename Char>
Char *skip_non_space(Char *str)
{
    if (str)
    {
        while (*str && !std::isspace(static_cast<unsigned char>(*str)))
        {
            ++str;
        }
    }
    return str;
}

template <typename Char>
Char *skip_alpha(Char *str)
{
    if (str)
    {
        while (*str && std::isalpha(static_cast<unsigned char>(*str)))
        {
            ++str;
        }
    }
    return str;
}

template <typename Char>
Char *skip_non_alpha(Char *str)
{
    if (str)
    {
        while (*str && !std::isalpha(static_cast<unsigned char>(*str)))
        {
            ++str;
        }
    }
    return str;
}

inline bool is_hor_space(char c)
{
    return c == ' ' || c == '\t';
}

template <typename Char>
Char *skip_hor_space(Char *str)
{
    if (str)
    {
        while (*str && is_hor_space(*str))
        {
            ++str;
        }
    }
    return str;
}

#endif
