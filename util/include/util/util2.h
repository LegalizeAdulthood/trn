/* util2.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_UTIL2_H
#define TRN_UTIL2_H

#include <string>
#include <string_view>

char       *save_str(std::string_view str);
char       *safe_copy(char *to, const char *from, int len);
const char *copy_till(char *to, const char *from, int delim);
std::string file_exp(std::string_view text);
const char *in_string(const char *big, const char *little, bool case_matters);
char       *in_string(char *big, const char *little, bool case_matters);

struct AuthCredentials
{
    std::string user;
    std::string password;
};

AuthCredentials read_auth_file(const char *file);

#endif
