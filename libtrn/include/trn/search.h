/* trn/search.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_SEARCH_H
#define TRN_SEARCH_H

#ifndef NBRA
#define NBRA    10              // the maximum number of meta-brackets in an
                                // RE -- \( \)
#define NALTS   10              // the maximum number of \|'s
#endif

struct CompiledRegex
{
    void        init_compex();
    void        free_compex();
    const char *get_bracket(int n);
    char       *compile(const char *strp, bool re, bool fold);
    char       *grow_eb(char *epp, char **alt);
    const char *execute(const char *addr);
    bool        advance(const char *lp, const char *ep);
    bool        back_ref(int i, const char *lp);

    char       *m_exp_buf;                  // The compiled search string
    int         m_eb_len;                   // Length of above buffer
    char       *m_alternatives[NALTS + 1];  // The list of \| separated alternatives
    const char *m_bracket_start_list[NBRA]; // RE meta-bracket start list
    const char *m_bracket_end_list[NBRA];   // RE meta-bracket end list
    char       *m_bracket_str;              // saved match string after execute()
    char        m_num_brackets;             // The number of meta-brackets int the most
                                            // recently compiled RE
    bool m_do_folding;                      // fold upper and lower case?
};

void        search_init();

#endif
