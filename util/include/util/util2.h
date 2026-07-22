/* util2.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_UTIL2_H
#define TRN_UTIL2_H

#include <filesystem>
#include <string>
#include <string_view>

char       *safe_copy(char *to, const char *from, int len);
std::string file_exp(std::string_view text);
bool        in_string(std::string_view haystack, std::string_view needle, bool case_matters);

struct AuthCredentials
{
    std::string user;
    std::string password;
};

AuthCredentials read_auth_file(const std::filesystem::path &file);

#endif
