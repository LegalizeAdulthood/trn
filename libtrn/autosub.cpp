/* autosub.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <string>

#include "common.h"
#include "autosub.h"

#include "env.h"
#include "final.h"
#include "ngsrch.h"
#include "search.h"

static bool matchlist(const char *patlist, const char *s);

/* Consider the newsgroup specified, and return:        */
/* : if we should autosubscribe to it                   */
/* ! if we should autounsubscribe to it                 */
/* \0 if we should ask the user.                        */
addnew_type auto_subscribe(const char *name)
{
    const char *s = get_val_const("AUTOSUBSCRIBE", nullptr);
    if (s && matchlist(s, name))
    {
        return ADDNEW_SUB;
    }

    s = get_val_const("AUTOUNSUBSCRIBE", nullptr);
    if (s && matchlist(s, name))
    {
        return ADDNEW_UNSUB;
    }

    return ADDNEW_ASK;
}

static bool matchlist(const char *patlist, const char *s)
{
    COMPEX ilcompex;
    bool   tmpresult;

    bool result = false;
    init_compex(&ilcompex);
    while(patlist && *patlist) {
        if (*patlist == '!') {
            patlist++;
            tmpresult = false;
        } else
        {
            tmpresult = true;
        }

        const char *p = strchr(patlist, ',');
        std::string pattern;
        if (p != nullptr)
        {
            pattern.assign(patlist, p);
        }
        else
        {
            pattern.assign(patlist);
        }

        /* compile regular expression */
        const char *err = ng_comp(&ilcompex, pattern.c_str(), true, true);

        if (err != nullptr) {
            printf("\n%s\n", err);
            finalize(1);
        }

        if (execute(&ilcompex,s) != nullptr)
        {
            result = tmpresult;
        }
        patlist = p;
    }
    free_compex(&ilcompex);
    return result;
}
