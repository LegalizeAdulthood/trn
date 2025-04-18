/* autosub.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/autosub.h"

#include "util/env.h"
#include "trn/final.h"
#include "trn/ngsrch.h"
#include "trn/search.h"

#include <cstdio>
#include <cstring>
#include <string>

static bool match_list(const char *pat_list, const char *s);

// Consider the newsgroup specified, and return:
// : if we should autosubscribe to it
// ! if we should autounsubscribe to it
// \0 if we should ask the user.
AddNewType auto_subscribe(const char *name)
{
    const char *s = get_val_const("AUTOSUBSCRIBE", nullptr);
    if (s && match_list(s, name))
    {
        return ADDNEW_SUB;
    }

    s = get_val_const("AUTOUNSUBSCRIBE", nullptr);
    if (s && match_list(s, name))
    {
        return ADDNEW_UNSUB;
    }

    return ADDNEW_ASK;
}

static bool match_list(const char *pat_list, const char *s)
{
    CompiledRegex il_compex;
    bool   tmp_result;

    bool result = false;
    init_compex(&il_compex);
    while (pat_list && *pat_list)
    {
        if (*pat_list == '!')
        {
            pat_list++;
            tmp_result = false;
        }
        else
        {
            tmp_result = true;
        }

        const char *p = std::strchr(pat_list, ',');
        std::string pattern;
        if (p != nullptr)
        {
            pattern.assign(pat_list, p);
        }
        else
        {
            pattern.assign(pat_list);
        }

        // compile regular expression
        const char *err = newsgroup_comp(&il_compex, pattern.c_str(), true, true);

        if (err != nullptr)
        {
            std::printf("\n%s\n", err);
            finalize(1);
        }

        if (execute(&il_compex,s) != nullptr)
        {
            result = tmp_result;
        }
        pat_list = p;
    }
    free_compex(&il_compex);
    return result;
}
