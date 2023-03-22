/* autosub.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "common.h"
#include "search.h"
#include "ngsrch.h"
#include "env.h"
#include "final.h"
#include "autosub.h"

/* Consider the newsgroup specified, and return:	*/
/* : if we should autosubscribe to it			*/
/* ! if we should autounsubscribe to it			*/
/* \0 if we should ask the user.			*/
int auto_subscribe(char *name)
{
    char* s;

    if((s = get_val("AUTOSUBSCRIBE", nullptr)) && matchlist(s, name))
	return ':';
    if((s = get_val("AUTOUNSUBSCRIBE", nullptr)) && matchlist(s, name))
	return '!';
    return 0;
}

bool matchlist(char *patlist, char *s)
{
    COMPEX ilcompex;
    char* p;
    char* err;
    bool result;
    bool tmpresult;

    result = false;
    init_compex(&ilcompex);
    while(patlist && *patlist) {
	if (*patlist == '!') {
	    patlist++;
	    tmpresult = false;
	} else
	    tmpresult = true;

	if ((p = strchr(patlist, ',')) != nullptr)
	    *p = '\0';
        /* compile regular expression */
	err = ng_comp(&ilcompex,patlist,true,true);
	if (p)
	    *p++ = ',';

	if (err != nullptr) {
	    printf("\n%s\n", err) FLUSH;
	    finalize(1);
	}
	
	if (execute(&ilcompex,s) != nullptr)
	    result = tmpresult;
	patlist = p;
    }
    free_compex(&ilcompex);
    return result;
}
