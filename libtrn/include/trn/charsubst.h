/* trn/charsubst.h
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
#ifndef TRN_CHARSUBST_H
#define TRN_CHARSUBST_H

#include <config/config2.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced
extern std::string g_charsets;

char        current_char_subst_mode();
void        set_char_subst_mode(char mode);
void        reset_char_subst_mode();
void        next_char_subst_mode();
std::string current_char_subst();

class IniSection;

class UnicodeSubstitutionProfile
{
public:
    // Overlay a section atomically. Later entries for a scalar take precedence.
    bool                            configure(const IniSection &section);
    std::optional<std::string_view> replacement(unsigned long scalar) const;

private:
    std::vector<std::pair<unsigned long, std::string>> m_mappings;
};

extern UnicodeSubstitutionProfile g_unicode_substitutions;

// Inputs are complete UTF-8 views. Invalid bytes become U+FFFD, one byte at a time.
// TeX prefixes may span calls on this instance; finish flushes a trailing prefix.
class CharSubstitution
{
public:
    explicit CharSubstitution(char_int mode);
    CharSubstitution(char_int mode, const UnicodeSubstitutionProfile &profile);

    std::string substitute(std::string_view input);
    std::string finish();

private:
    char_int m_mode;
    char     m_tex_prefix{};
    UnicodeSubstitutionProfile m_unicode_profile;
};

// Substitute one line, including its first newline, with fresh instance state.
std::string str_char_subst(std::string_view input, char_int subst);

#endif
