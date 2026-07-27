/* string-algos.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_STRING_ALGOS_H
#define TRN_STRING_ALGOS_H

#include <algorithm>
#include <cctype>
#include <string_view>

namespace string_algos_detail
{

template <typename Predicate>
std::string_view skip_while(std::string_view str, Predicate predicate)
{
    const std::string_view::const_iterator pos = std::find_if_not(str.begin(), str.end(), predicate);
    return str.substr(static_cast<std::size_t>(pos - str.begin()));
}

} // namespace string_algos_detail

inline bool empty(const char *str)
{
    return str == nullptr || str[0] == '\0';
}

inline std::string_view skip_ne(std::string_view str, char delim)
{
    return string_algos_detail::skip_while(str, [delim](char ch) { return ch != delim; });
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

inline std::string_view skip_eq(std::string_view str, char delim)
{
    return string_algos_detail::skip_while(str, [delim](char ch) { return ch == delim; });
}

inline std::string_view skip_digits(std::string_view str)
{
    return string_algos_detail::skip_while(str, [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
}

inline std::string_view skip_space(std::string_view str)
{
    return string_algos_detail::skip_while(str, [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
}

inline std::string_view skip_non_space(std::string_view str)
{
    return string_algos_detail::skip_while(str, [](char ch) { return !std::isspace(static_cast<unsigned char>(ch)); });
}

inline std::string_view skip_alpha(std::string_view str)
{
    return string_algos_detail::skip_while(str, [](char ch) { return std::isalpha(static_cast<unsigned char>(ch)); });
}

inline std::string_view skip_non_alpha(std::string_view str)
{
    return string_algos_detail::skip_while(str, [](char ch) { return !std::isalpha(static_cast<unsigned char>(ch)); });
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

inline std::string_view skip_hor_space(std::string_view str)
{
    return string_algos_detail::skip_while(str, is_hor_space);
}

#endif
