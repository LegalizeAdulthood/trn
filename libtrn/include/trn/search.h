/* trn/search.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_SEARCH_H
#define TRN_SEARCH_H

#ifndef NBRA
#define NBRA    10              /* the maximum number of meta-brackets in an
                                   RE -- \( \) */
#define NALTS   10              // the maximum number of \|'s

struct CompiledRegex
{
    char* expbuf;               // The compiled search string
    int eblen;                  // Length of above buffer
    char* alternatives[NALTS+1];// The list of \| seperated alternatives
    const char* braslist[NBRA]; // RE meta-bracket start list
    const char* braelist[NBRA]; // RE meta-bracket end list
    char* brastr;               // saved match string after execute()
    char nbra;                  /* The number of meta-brackets int the most
                                   recenlty compiled RE */
    bool do_folding;            // fold upper and lower case?
};
#endif

void        search_init();
void        init_compex(CompiledRegex *compex);
void        free_compex(CompiledRegex *compex);
const char *getbracket(CompiledRegex *compex, int n);
void        case_fold(bool which);
char       *compile(CompiledRegex *compex, const char *strp, bool RE, bool fold);
char       *grow_eb(CompiledRegex *compex, char *epp, char **alt);
const char *execute(CompiledRegex *compex, const char *addr);
bool        advance(CompiledRegex *compex, const char *lp, const char *ep);
bool        backref(CompiledRegex *compex, int i, const char *lp);
bool        cclass(const char *set, int c, int af);

#endif
