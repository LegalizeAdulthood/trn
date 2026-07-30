/* trn/search.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SEARCH_H
#define TRN_SEARCH_H

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#ifndef NBRA
#define NBRA    10              // the maximum number of meta-brackets in an
                                // RE -- \( \)
#define NALTS   10              // the maximum number of \|'s
#endif

class CompiledRegex
{
public:
    void             init_compex();
    void             free_compex();
    std::string_view get_bracket(int n) const;
    bool             has_brackets() const;
    std::string_view compile(std::string_view strp, bool re, bool fold);
    bool             execute(std::string_view text);

private:
    char            *grow_eb(char *epp, char **alt);
    bool             advance(const char *lp, const char *ep);
    bool             back_ref(int i, const char *lp);

    char       *m_exp_buf;                  // The compiled search string
    int         m_eb_len;                   // Length of above buffer
    std::array<char *, NALTS + 1>      m_alternatives;        // The list of \| separated alternatives
    std::array<std::size_t, NBRA + 1> m_bracket_start_offsets; // RE meta-bracket start offsets
    std::array<std::size_t, NBRA + 1> m_bracket_end_offsets;   // RE meta-bracket end offsets
    std::string m_bracket_str;              // saved match string after execute()
    int         m_num_brackets;             // The number of meta-brackets int the most
                                            // recently compiled RE
    bool m_do_folding;                      // fold upper and lower case?
};

void        search_init();

#endif
