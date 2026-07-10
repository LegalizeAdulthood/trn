/* autosub.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <trn/autosub.h>

#include <config/common.h>
#include <trn/final.h>
#include <trn/ngsrch.h>
#include <trn/search.h>
#include <util/env.h>

#include <cstdio>
#include <string>
#include <string_view>

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

    bool             result = false;
    std::string_view patterns = pat_list != nullptr ? std::string_view{pat_list} : std::string_view{};
    il_compex.init_compex();
    while (!patterns.empty())
    {
        bool tmp_result = true;
        if (patterns.front() == '!')
        {
            patterns.remove_prefix(1);
            tmp_result = false;
        }

        const std::size_t      comma = patterns.find(',');
        const std::string_view pattern_view = patterns.substr(0, comma);
        const std::string      pattern{pattern_view};

        // compile regular expression
        const char *err = newsgroup_comp(&il_compex, pattern.c_str(), true, true);

        if (err != nullptr)
        {
            std::printf("\n%s\n", err);
            finalize(1);
        }

        if (il_compex.execute(s) != nullptr)
        {
            result = tmp_result;
        }
        if (comma == std::string_view::npos)
        {
            break;
        }
        patterns.remove_prefix(comma + 1);
    }
    il_compex.free_compex();
    return result;
}
