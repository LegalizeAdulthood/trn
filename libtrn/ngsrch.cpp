/* ngsrch.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/ngsrch.h"

#include "trn/addng.h"
#include "trn/final.h"
#include "trn/ng.h"
#include "trn/ngdata.h"
#include "trn/ngstuff.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/search.h"
#include "trn/terminal.h"
#include "util/util2.h"

static bool   s_ng_doempty{}; /* search empty newsgroups? */
static COMPEX s_ngcompex;

void ngsrch_init()
{
    s_ng_doempty = false;
    init_compex(&s_ngcompex);
}

// patbuf   if patbuf != g_buf, get_cmd must */
// get_cmd  be set to false!!! */
ng_search_result ng_search(char *patbuf, bool get_cmd)
{
    g_int_count = 0;
    if (get_cmd && g_buf == patbuf)
    {
        if (!finish_command(false))     /* get rest of command */
        {
            return NGS_ABORT;
        }
    }

    perform_status_init(g_newsgroup_toread);
    char const cmdchr = *patbuf;         /* what kind of search? */
    char *s = cpytill(g_buf, patbuf + 1, cmdchr); /* ok to cpy g_buf+1 to g_buf */
    char *pattern;                                /* unparsed pattern */
    for (pattern = g_buf; *pattern == ' '; pattern++)
    {
    }
    if (*pattern)
    {
        s_ng_doempty = false;
    }

    if (*s) {                           /* modifiers or commands? */
        while (*++s) {
            switch (*s) {
            case 'r':
                s_ng_doempty = true;
                break;
            default:
                goto loop_break;
            }
        }
      loop_break:;
    }
    while (isspace(*s) || *s == ':')
    {
        s++;
    }
    char *cmdlst = nullptr; /* list of commands to do */
    if (*s)
    {
        cmdlst = savestr(s);
    }
    else if (g_general_mode == GM_SELECTOR)
    {
        cmdlst = savestr("+");
    }
    ng_search_result ret = NGS_NOTFOUND; /* assume no commands */
    if (cmdlst)
    {
        ret = NGS_DONE;
    }
    const char *err = ng_comp(&s_ngcompex, pattern, true, true);
    if (err != nullptr)
    {
                                        /* compile regular expression */
        errormsg(err);
        if (cmdlst)
        {
            free(cmdlst);
        }
        return NGS_ERROR;
    }
    if (!cmdlst) {
        fputs("\nSearching...",stdout); /* give them something to read */
        fflush(stdout);
    }

    bool const output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    if (g_first_addgroup) {
        ADDGROUP *gp = g_first_addgroup;
        do {
            if (execute(&s_ngcompex,gp->name) != nullptr) {
                if (!cmdlst)
                {
                    return NGS_FOUND;
                }
                if (addgrp_perform(gp,cmdlst,output_level && g_page_line==1)<0) {
                    free(cmdlst);
                    return NGS_INTR;
                }
            }
            if (!output_level && g_page_line == 1)
            {
                perform_status(g_newsgroup_toread, 50);
            }
        } while ((gp = gp->next) != nullptr);
        if (cmdlst)
        {
            free(cmdlst);
        }
        return ret;
    }

    bool const backward = cmdchr == '?'; /* direction of search */
    NGDATA const *ng_start = g_ngptr;
    if (backward) {
        if (!g_ngptr)
        {
            g_ngptr = g_last_ng;
            ng_start = g_last_ng;
        }
        else if (!cmdlst) {
            if (g_ngptr == g_first_ng)  /* skip current newsgroup */
            {
                g_ngptr = g_last_ng;
            }
            else
            {
                g_ngptr = g_ngptr->prev;
            }
        }
    }
    else {
        if (!g_ngptr)
        {
            g_ngptr = g_first_ng;
            ng_start = g_first_ng;
        }
        else if (!cmdlst) {
            if (g_ngptr == g_last_ng)   /* skip current newsgroup */
            {
                g_ngptr = g_first_ng;
            }
            else
            {
                g_ngptr = g_ngptr->next;
            }
        }
    }

    if (!g_ngptr)
    {
        return NGS_NOTFOUND;
    }

    do {
        if (g_int_count) {
            g_int_count = 0;
            ret = NGS_INTR;
            break;
        }

        if (g_ngptr->toread >= TR_NONE && ng_wanted(g_ngptr)) {
            if (g_ngptr->toread == TR_NONE)
            {
                set_toread(g_ngptr, ST_LAX);
            }
            if (s_ng_doempty || ((g_ngptr->toread > TR_NONE) ^ g_sel_rereading)) {
                if (!cmdlst)
                {
                    return NGS_FOUND;
                }
                set_ng(g_ngptr);
                if (ng_perform(cmdlst,output_level && g_page_line == 1) < 0) {
                    free(cmdlst);
                    return NGS_INTR;
                }
            }
            if (output_level && !cmdlst) {
                printf("\n[0 unread in %s -- skipping]",g_ngptr->rcline);
                fflush(stdout);
            }
        }
        if (!output_level && g_page_line == 1)
        {
            perform_status(g_newsgroup_toread, 50);
        }
    } while ((g_ngptr = (backward? (g_ngptr->prev? g_ngptr->prev : g_last_ng)
                               : (g_ngptr->next? g_ngptr->next : g_first_ng)))
                != ng_start);

    if (cmdlst)
    {
        free(cmdlst);
    }
    return ret;
}

bool ng_wanted(NGDATA *np)
{
    return execute(&s_ngcompex,np->rcline) != nullptr;
}

const char *ng_comp(COMPEX *compex, const char *pattern, bool RE, bool fold)
{
    char ng_pattern[128];
    const char* s = pattern;
    char* d = ng_pattern;

    if (!*s) {
        if (compile(compex, "", RE, fold))
        {
            return "No previous search pattern";
        }
        return nullptr;                 /* reuse old pattern */
    }
    for (; *s; s++) {
        if (*s == '.') {
            *d++ = '\\';
            *d++ = *s;
        }
        else if (*s == '?') {
            *d++ = '.';
        }
        else if (*s == '*') {
            *d++ = '.';
            *d++ = *s;
        }
        else
        {
            *d++ = *s;
        }
    }
    *d = '\0';
    return compile(compex,ng_pattern,RE,fold);
}
