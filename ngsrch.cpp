/* ngsrch.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "ngdata.h"
#include "hash.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "addng.h"
#include "nntp.h"
#include "ngstuff.h"
#include "rcstuff.h"
#include "final.h"
#include "search.h"
#include "trn.h"
#include "util.h"
#include "util2.h"
#include "term.h"
#include "rcln.h"
#include "cache.h"
#include "rt-select.h"
#include "rt-util.h"
#include "INTERN.h"
#include "ngsrch.h"

COMPEX ngcompex;

void ngsrch_init()
{
    init_compex(&ngcompex);
}

// patbuf   if patbuf != buf, get_cmd must */
// get_cmd  be set to false!!! */
int ng_search(char *patbuf, int get_cmd)
{
    char cmdchr = *patbuf;	/* what kind of search? */
    char* s;
    char* pattern;			/* unparsed pattern */
    char* cmdlst = nullptr;		/* list of commands to do */
    int ret = NGS_NOTFOUND;		/* assume no commands */
    bool backward = cmdchr == '?';	/* direction of search */
    bool output_level = (!use_threads && gmode != 's');
    NGDATA* ng_start = g_ngptr;

    g_int_count = 0;
    if (get_cmd && buf == patbuf)
	if (!finish_command(false))	/* get rest of command */
	    return NGS_ABORT;

    perform_status_init(g_newsgroup_toread);
    s = cpytill(buf,patbuf+1,cmdchr);	/* ok to cpy buf+1 to buf */
    for (pattern = buf; *pattern == ' '; pattern++) ;
    if (*pattern)
	ng_doempty = false;

    if (*s) {				/* modifiers or commands? */
	while (*++s) {
	    switch (*s) {
	    case 'r':
		ng_doempty = true;
		break;
	    default:
		goto loop_break;
	    }
	}
      loop_break:;
    }
    while (isspace(*s) || *s == ':')
	s++;
    if (*s)
	cmdlst = savestr(s);
    else if (gmode == 's')
	cmdlst = savestr("+");
    if (cmdlst)
	ret = NGS_DONE;
    if ((s = ng_comp(&ngcompex,pattern,true,true)) != nullptr) {
					/* compile regular expression */
	errormsg(s);
	ret = NGS_ERROR;
	goto exit;
    }
    if (!cmdlst) {
	fputs("\nSearching...",stdout);	/* give them something to read */
	fflush(stdout);
    }

    if (g_first_addgroup) {
	ADDGROUP *gp = g_first_addgroup;
	do {
	    if (execute(&ngcompex,gp->name) != nullptr) {
		if (!cmdlst)
		    return NGS_FOUND;
		if (addgrp_perform(gp,cmdlst,output_level && page_line==1)<0) {
		    free(cmdlst);
		    return NGS_INTR;
		}
	    }
	    if (!output_level && page_line == 1)
		perform_status(g_newsgroup_toread, 50);
	} while ((gp = gp->next) != nullptr);
	goto exit;
    }

    if (backward) {
	if (!g_ngptr)
	    ng_start = g_ngptr = g_last_ng;
	else if (!cmdlst) {
	    if (g_ngptr == g_first_ng)	/* skip current newsgroup */
		g_ngptr = g_last_ng;
	    else
		g_ngptr = g_ngptr->prev;
	}
    }
    else {
	if (!g_ngptr)
	    ng_start = g_ngptr = g_first_ng;
	else if (!cmdlst) {
	    if (g_ngptr == g_last_ng)	/* skip current newsgroup */
		g_ngptr = g_first_ng;
	    else
		g_ngptr = g_ngptr->next;
	}
    }

    if (!g_ngptr)
	return NGS_NOTFOUND;

    do {
	if (g_int_count) {
	    g_int_count = 0;
	    ret = NGS_INTR;
	    break;
	}

	if (g_ngptr->toread >= TR_NONE && ng_wanted(g_ngptr)) {
	    if (g_ngptr->toread == TR_NONE)
		set_toread(g_ngptr, ST_LAX);
	    if (ng_doempty || ((g_ngptr->toread > TR_NONE) ^ sel_rereading)) {
		if (!cmdlst)
		    return NGS_FOUND;
		set_ng(g_ngptr);
		if (ng_perform(cmdlst,output_level && page_line == 1) < 0) {
		    free(cmdlst);
		    return NGS_INTR;
		}
	    }
	    if (output_level && !cmdlst) {
		printf("\n[0 unread in %s -- skipping]",g_ngptr->rcline);
		fflush(stdout);
	    }
	}
	if (!output_level && page_line == 1)
	    perform_status(g_newsgroup_toread, 50);
    } while ((g_ngptr = (backward? (g_ngptr->prev? g_ngptr->prev : g_last_ng)
			       : (g_ngptr->next? g_ngptr->next : g_first_ng)))
		!= ng_start);
exit:
    if (cmdlst)
	free(cmdlst);
    return ret;
}

bool ng_wanted(NGDATA *np)
{
    return execute(&ngcompex,np->rcline) != nullptr;
}

char *ng_comp(COMPEX *compex, const char *pattern, bool RE, bool fold)
{
    char ng_pattern[128];
    const char* s = pattern;
    char* d = ng_pattern;

    if (!*s) {
	if (compile(compex, "", RE, fold))
	    return "No previous search pattern";
	return nullptr;			/* reuse old pattern */
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
	    *d++ = *s;
    }
    *d = '\0';
    return compile(compex,ng_pattern,RE,fold);
}
