/* ngsrch.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/ngsrch.h"

#include "trn/ngdata.h"
#include "trn/addng.h"
#include "trn/final.h"
#include "trn/ng.h"
#include "trn/ngstuff.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/search.h"
#include "trn/terminal.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

static bool          s_newsgroup_do_empty{}; // search empty newsgroups?
static CompiledRegex s_newsgroup_compex;

void newsgroup_search_init()
{
    s_newsgroup_do_empty = false;
    init_compex(&s_newsgroup_compex);
}

// patbuf   if patbuf != g_buf, get_cmd must */
// get_cmd  be set to false!!! */
NewsgroupSearchResult newsgroup_search(char *patbuf, bool get_cmd)
{
    g_int_count = 0;
    if (get_cmd && g_buf == patbuf)
    {
        if (!finish_command(false))     // get rest of command
        {
            return NGS_ABORT;
        }
    }

    perform_status_init(g_newsgroup_to_read.num);
    const char cmdchr = *patbuf;         // what kind of search?
    char *s = copy_till(g_buf, patbuf + 1, cmdchr); // ok to cpy g_buf+1 to g_buf
    char *pattern;                                // unparsed pattern
    for (pattern = g_buf; *pattern == ' '; pattern++)
    {
    }
    if (*pattern)
    {
        s_newsgroup_do_empty = false;
    }

    if (*s)                             // modifiers or commands?
    {
        while (*++s)
        {
            switch (*s)
            {
            case 'r':
                s_newsgroup_do_empty = true;
                break;

            default:
                goto loop_break;
            }
        }
      loop_break:;
    }
    while (std::isspace(*s) || *s == ':')
    {
        s++;
    }
    char *cmdlst = nullptr; // list of commands to do
    if (*s)
    {
        cmdlst = save_str(s);
    }
    else if (g_general_mode == GM_SELECTOR)
    {
        cmdlst = save_str("+");
    }
    NewsgroupSearchResult ret = NGS_NOT_FOUND; // assume no commands
    if (cmdlst)
    {
        ret = NGS_DONE;
    }
    const char *err = newsgroup_comp(&s_newsgroup_compex, pattern, true, true);
    if (err != nullptr)
    {
                                        // compile regular expression
        error_msg(err);
        if (cmdlst)
        {
            std::free(cmdlst);
        }
        return NGS_ERROR;
    }
    if (!cmdlst)
    {
        std::fputs("\nSearching...", stdout); // give them something to read
        std::fflush(stdout);
    }

    const bool output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    if (g_first_add_group)
    {
        AddGroup *gp = g_first_add_group;
        do
        {
            if (execute(&s_newsgroup_compex, gp->name) != nullptr)
            {
                if (!cmdlst)
                {
                    return NGS_FOUND;
                }
                if (add_group_perform(gp, cmdlst, output_level && g_page_line == 1) < 0)
                {
                    std::free(cmdlst);
                    return NGS_INTR;
                }
            }
            if (!output_level && g_page_line == 1)
            {
                perform_status(g_newsgroup_to_read.num, 50);
            }
        } while ((gp = gp->next) != nullptr);
        if (cmdlst)
        {
            std::free(cmdlst);
        }
        return ret;
    }

    const bool           backward = cmdchr == '?'; // direction of search
    const NewsgroupData *ng_start = g_newsgroup_ptr;
    if (backward)
    {
        if (!g_newsgroup_ptr)
        {
            g_newsgroup_ptr = g_last_newsgroup;
            ng_start = g_last_newsgroup;
        }
        else if (!cmdlst)
        {
            if (g_newsgroup_ptr == g_first_newsgroup)  // skip current newsgroup
            {
                g_newsgroup_ptr = g_last_newsgroup;
            }
            else
            {
                g_newsgroup_ptr = g_newsgroup_ptr->prev;
            }
        }
    }
    else
    {
        if (!g_newsgroup_ptr)
        {
            g_newsgroup_ptr = g_first_newsgroup;
            ng_start = g_first_newsgroup;
        }
        else if (!cmdlst)
        {
            if (g_newsgroup_ptr == g_last_newsgroup)   // skip current newsgroup
            {
                g_newsgroup_ptr = g_first_newsgroup;
            }
            else
            {
                g_newsgroup_ptr = g_newsgroup_ptr->next;
            }
        }
    }

    if (!g_newsgroup_ptr)
    {
        return NGS_NOT_FOUND;
    }

    do
    {
        if (g_int_count)
        {
            g_int_count = 0;
            ret = NGS_INTR;
            break;
        }

        if (g_newsgroup_ptr->to_read >= TR_NONE && newsgroup_wanted(g_newsgroup_ptr))
        {
            if (g_newsgroup_ptr->to_read == TR_NONE)
            {
                set_to_read(g_newsgroup_ptr, ST_LAX);
            }
            if (s_newsgroup_do_empty || ((g_newsgroup_ptr->to_read > TR_NONE) ^ g_sel_rereading))
            {
                if (!cmdlst)
                {
                    return NGS_FOUND;
                }
                set_newsgroup(g_newsgroup_ptr);
                if (newsgroup_perform(cmdlst, output_level && g_page_line == 1) < 0)
                {
                    std::free(cmdlst);
                    return NGS_INTR;
                }
            }
            if (output_level && !cmdlst)
            {
                std::printf("\n[0 unread in %s -- skipping]",g_newsgroup_ptr->rc_line);
                std::fflush(stdout);
            }
        }
        if (!output_level && g_page_line == 1)
        {
            perform_status(g_newsgroup_to_read.num, 50);
        }
    } while ((g_newsgroup_ptr = (backward? (g_newsgroup_ptr->prev? g_newsgroup_ptr->prev : g_last_newsgroup)
                               : (g_newsgroup_ptr->next? g_newsgroup_ptr->next : g_first_newsgroup)))
                != ng_start);

    if (cmdlst)
    {
        std::free(cmdlst);
    }
    return ret;
}

bool newsgroup_wanted(NewsgroupData *np)
{
    return execute(&s_newsgroup_compex,np->rc_line) != nullptr;
}

const char *newsgroup_comp(CompiledRegex *compex, const char *pattern, bool re, bool fold)
{
    char ng_pattern[128];
    const char* s = pattern;
    char* d = ng_pattern;

    if (!*s)
    {
        if (compile(compex, "", re, fold))
        {
            return "No previous search pattern";
        }
        return nullptr;                 // reuse old pattern
    }
    for (; *s; s++)
    {
        if (*s == '.')
        {
            *d++ = '\\';
            *d++ = *s;
        }
        else if (*s == '?')
        {
            *d++ = '.';
        }
        else if (*s == '*')
        {
            *d++ = '.';
            *d++ = *s;
        }
        else
        {
            *d++ = *s;
        }
    }
    *d = '\0';
    return compile(compex,ng_pattern,re,fold);
}
