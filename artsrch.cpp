/* artsrch.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "hash.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "search.h"
#include "term.h"
#include "util.h"
#include "util2.h"
#include "intrp.h"
#include "cache.h"
#include "bits.h"
#include "kfile.h"
#include "head.h"
#include "final.h"
#include "ng.h"
#include "addng.h"
#include "ngstuff.h"
#include "artio.h"
#include "rthread.h"
#include "rt-util.h"
#include "rt-select.h"
#include "INTERN.h"
#include "artsrch.h"

void artsrch_init()
{
    init_compex(&sub_compex);
    init_compex(&art_compex);
}

/* search for an article containing some pattern */

/* if patbuf != buf, get_cmd must be set to false!!! */
int art_search(char *patbuf, int patbufsiz, int get_cmd)
{
    char* pattern;			/* unparsed pattern */
    char cmdchr = *patbuf;	/* what kind of search? */
    char* s;
    bool backward = cmdchr == '?' || cmdchr == Ctl('p');
					/* direction of search */
    COMPEX* compex;			/* which compiled expression */
    char* cmdlst = nullptr;		/* list of commands to do */
    int ret = SRCH_NOTFOUND;		/* assume no commands */
    int saltaway = 0;			/* store in KILL file? */
    int howmuch;			/* search scope: subj/from/Hdr/head/art */
    int srchhdr;			/* header to search if Hdr scope */
    bool topstart = false;
    bool doread;			/* search read articles? */
    bool foldcase = true;		/* fold upper and lower case? */
    int ignorethru = 0;			/* should we ignore the thru line? */
    bool output_level = (!use_threads && gmode != 's');
    ART_NUM srchfirst;

    g_int_count = 0;
    if (cmdchr == '/' || cmdchr == '?') {	/* normal search? */
	if (get_cmd && buf == patbuf)
	    if (!finish_command(false))	/* get rest of command */
		return SRCH_ABORT;
	compex = &art_compex;
	if (patbuf[1]) {
	    howmuch = ARTSCOPE_SUBJECT;
	    srchhdr = SOME_LINE;
	    doread = false;
	}
	else {
	    howmuch = art_howmuch;
	    srchhdr = art_srchhdr;
	    doread = art_doread;
	}
	s = cpytill(buf,patbuf+1,cmdchr);/* ok to cpy buf+1 to buf */
	pattern = buf;
	if (*pattern) {
	    if (*lastpat)
		free(lastpat);
	    lastpat = savestr(pattern);
	}
	if (*s) {			/* modifiers or commands? */
	    while (*++s) {
		switch (*s) {
		case 'f':		/* scan the From line */
		    howmuch = ARTSCOPE_FROM;
		    break;
		case 'H':		/* scan a specific header */
		    howmuch = ARTSCOPE_ONEHDR;
		    s = cpytill(msg, s+1, ':');
		    srchhdr = get_header_num(msg);
		    goto loop_break;
		case 'h':		/* scan header */
		    howmuch = ARTSCOPE_HEAD;
		    break;
		case 'b':		/* scan body sans signature */
		    howmuch = ARTSCOPE_BODY_NOSIG;
		    break;
		case 'B':		/* scan body */
		    howmuch = ARTSCOPE_BODY;
		    break;
		case 'a':		/* scan article */
		    howmuch = ARTSCOPE_ARTICLE;
		    break;
		case 't':		/* start from the top */
		    topstart = true;
		    break;
		case 'r':		/* scan read articles */
		    doread = true;
		    break;
		case 'K':		/* put into KILL file */
		    saltaway = 1;
		    break;
		case 'c':		/* make search case sensitive */
		    foldcase = false;
		    break;
		case 'I':		/* ignore the killfile thru line */
		    ignorethru = 1;
		    break;
		case 'N':		/* override ignore if -k was used */
		    ignorethru = -1;
		    break;
		default:
		    goto loop_break;
		}
	    }
	  loop_break:;
	}
	while (isspace(*s) || *s == ':') s++;
	if (*s) {
	    if (*s == 'm')
		doread = true;
	    if (*s == 'k')		/* grandfather clause */
		*s = 'j';
	    cmdlst = savestr(s);
	    ret = SRCH_DONE;
	}
	art_howmuch = howmuch;
	art_srchhdr = srchhdr;
	art_doread = doread;
	if (g_srchahead)
	    g_srchahead = -1;
    }
    else {
	char* h;
	int saltmode = patbuf[2] == 'g'? 2 : 1;
	char *finding_str = patbuf[1] == 'f'? "author" : "subject";

	howmuch = patbuf[1] == 'f'? ARTSCOPE_FROM : ARTSCOPE_SUBJECT;
	srchhdr = SOME_LINE;
	doread = (cmdchr == Ctl('p'));
	if (cmdchr == Ctl('n'))
	    ret = SRCH_SUBJDONE;
	compex = &sub_compex;
	pattern = patbuf+1;
	if (howmuch == ARTSCOPE_SUBJECT) {
	    strcpy(pattern,": *");
	    h = pattern + strlen(pattern);
	    interp(h,patbufsiz - (h-patbuf),"%\\s");  /* fetch current subject */
	}
	else {
	    h = pattern;
	    /*$$ if using thread files, make this "%\\)f" */
	    interp(pattern, patbufsiz - 1, "%\\>f");
	}
	if (cmdchr == 'k' || cmdchr == 'K' || cmdchr == ','
	 || cmdchr == '+' || cmdchr == '.' || cmdchr == 's') {
	    if (cmdchr != 'k')
		saltaway = saltmode;
	    ret = SRCH_DONE;
	    if (cmdchr == '+') {
		cmdlst = savestr("+");
		if (!ignorethru && kill_thru_kludge)
		    ignorethru = 1;
	    }
	    else if (cmdchr == '.') {
		cmdlst = savestr(".");
		if (!ignorethru && kill_thru_kludge)
		    ignorethru = 1;
	    }
	    else if (cmdchr == 's') {
		cmdlst = savestr(patbuf);
		/*ignorethru = 1;*/
	    }
	    else {
		if (cmdchr == ',')
		    cmdlst = savestr(",");
		else
		    cmdlst = savestr("j");
		mark_as_read(article_ptr(g_art));	/* this article needs to die */
	    }
	    if (!*h) {
		if (verbose)
		    sprintf(msg, "Current article has no %s.", finding_str);
	        else
		    sprintf(msg, "Null %s.", finding_str);
		errormsg(msg);
		ret = SRCH_ABORT;
		goto exit;
	    }
	    if (verbose) {
		if (cmdchr != '+' && cmdchr != '.')
		    printf("\nMarking %s \"%s\" as read.\n",finding_str,h) FLUSH;
		else
		    printf("\nSelecting %s \"%s\".\n",finding_str,h) FLUSH;
		termdown(2);
	    }
	}
	else if (!g_srchahead)
	    g_srchahead = -1;

	{			/* compensate for notesfiles */
	    int i;
	    for (i = 24; *h && i--; h++)
		if (*h == '\\')
		    h++;
	    *h = '\0';
	}
#ifdef DEBUG
	if (debug) {
	    printf("\npattern = %s\n",pattern) FLUSH;
	    termdown(2);
	}
#endif
    }
    if ((s = compile(compex,pattern,true,foldcase)) != nullptr) {
					/* compile regular expression */
	errormsg(s);
	ret = SRCH_ABORT;
	goto exit;
    }
    if (cmdlst && strchr(cmdlst,'='))
	ret = SRCH_ERROR;		/* listing subjects is an error? */
    if (gmode == 's') {
	if (!cmdlst) {
	    if (sel_mode == SM_ARTICLE)/* set the selector's default command */
		cmdlst = savestr("+");
	    else
		cmdlst = savestr("++");
	}
	ret = SRCH_DONE;
    }
    if (saltaway) {
	char saltbuf[LBUFLEN], *f;

	s = saltbuf;
	f = pattern;
	*s++ = '/';
	while (*f) {
	    if (*f == '/')
		*s++ = '\\';
	    *s++ = *f++;
	}
	*s++ = '/';
	if (doread)
	    *s++ = 'r';
	if (!foldcase)
	    *s++ = 'c';
	if (ignorethru)
	    *s++ = (ignorethru == 1 ? 'I' : 'N');
	if (howmuch != ARTSCOPE_SUBJECT) {
	    *s++ = scopestr[howmuch];
	    if (howmuch == ARTSCOPE_ONEHDR) {
		safecpy(s,g_htype[srchhdr].name,LBUFLEN-(s-saltbuf));
		s += g_htype[srchhdr].length;
		if (s - saltbuf > LBUFLEN-2)
		    s = saltbuf+LBUFLEN-2;
	    }
	}
	*s++ = ':';
	if (!cmdlst)
	    cmdlst = savestr("j");
	safecpy(s,cmdlst,LBUFLEN-(s-saltbuf));
	kf_append(saltbuf, saltaway == 2? KF_GLOBAL : KF_LOCAL);
    }
    if (get_cmd) {
	if (use_threads)
	    newline();
	else {
	    fputs("\nSearching...\n",stdout) FLUSH;
	    termdown(2);
	}
					/* give them something to read */
    }
    if (ignorethru == 0 && kill_thru_kludge && cmdlst
     && (*cmdlst == '+' || *cmdlst == '.'))
	ignorethru = 1;
    srchfirst = doread || sel_rereading? absfirst
		      : (mode != 'k' || ignorethru > 0)? firstart : g_killfirst;
    if (topstart || g_art == 0) {
	g_art = lastart+1;
	topstart = false;
    }
    if (backward) {
	if (cmdlst && g_art <= lastart)
	    g_art++;			/* include current article */
    }
    else {
	if (g_art > lastart)
	    g_art = srchfirst-1;
	else if (cmdlst && g_art >= absfirst)
	    g_art--;			/* include current article */
    }
    if (g_srchahead > 0) {
	if (!backward)
	    g_art = g_srchahead - 1;
	g_srchahead = -1;
    }
    assert(!cmdlst || *cmdlst);
    perform_status_init(ngptr->toread);
    for (;;) {
	/* check if we're out of articles */
	if (backward? ((g_art = article_prev(g_art)) < srchfirst)
		    : ((g_art = article_next(g_art)) > lastart))
	    break;
	if (g_int_count) {
	    g_int_count = 0;
	    ret = SRCH_INTR;
	    break;
	}
	g_artp = article_ptr(g_art);
	if (doread || (!(g_artp->flags & AF_UNREAD) ^ !sel_rereading)) {
	    if (wanted(compex,g_art,howmuch)) {
				    /* does the shoe fit? */
		if (!cmdlst)
		    return SRCH_FOUND;
		if (perform(cmdlst,output_level && page_line == 1) < 0) {
		    free(cmdlst);
		    return SRCH_INTR;
		}
	    }
	    else if (output_level && !cmdlst && !(g_art%50)) {
		printf("...%ld",(long)g_art);
		fflush(stdout);
	    }
	}
	if (!output_level && page_line == 1)
	    perform_status(ngptr->toread, 60 / (howmuch+1));
    }
exit:
    if (cmdlst)
	free(cmdlst);
    return ret;
}

/* determine if article fits pattern */
/* returns true if it exists and fits pattern, false otherwise */

bool wanted(COMPEX *compex, ART_NUM artnum, char_int scope)
{
    ARTICLE* ap = article_find(artnum);

    if (!ap || !(ap->flags & AF_EXISTS))
	return false;

    switch (scope) {
      case ARTSCOPE_SUBJECT:
	strcpy(buf,"Subject: ");
	strncpy(buf+9,fetchsubj(artnum,false),256);
#ifdef DEBUG
	if (debug & DEB_SEARCH_AHEAD)
	    printf("%s\n",buf) FLUSH;
#endif
	break;
      case ARTSCOPE_FROM:
	strcpy(buf, "From: ");
	strncpy(buf+6,fetchfrom(artnum,false),256);
	break;
      case ARTSCOPE_ONEHDR:
	g_untrim_cache = true;
	sprintf(buf, "%s: %s", g_htype[art_srchhdr].name,
		prefetchlines(artnum,art_srchhdr,false));
	g_untrim_cache = false;
	break;
      default: {
	char* s;
	char* nlptr;
	char ch;
	bool success = false, in_sig = false;
	if (scope != ARTSCOPE_BODY && scope != ARTSCOPE_BODY_NOSIG) {
	    if (!parseheader(artnum))
		return false;
	    /* see if it's in the header */
	    if (execute(compex,g_headbuf))	/* does it match? */
		return true;			/* say, "Eureka!" */
	    if (scope < ARTSCOPE_ARTICLE)
		return false;
	}
	if (g_parsed_art == artnum) {
	    if (!artopen(artnum,g_htype[PAST_HEADER].minpos))
		return false;
	}
	else {
	    if (!artopen(artnum,(ART_POS)0))
		return false;
	    if (!parseheader(artnum))
		return false;
	}
	/* loop through each line of the article */
	seekartbuf(g_htype[PAST_HEADER].minpos);
	while ((s = readartbuf(false)) != nullptr) {
	    if (scope == ARTSCOPE_BODY_NOSIG && *s == '-' && s[1] == '-'
	     && (s[2] == '\n' || (s[2] == ' ' && s[3] == '\n'))) {
		if (in_sig && success)
		    return true;
		in_sig = true;
	    }
	    if ((nlptr = strchr(s,'\n')) != nullptr) {
		ch = *++nlptr;
		*nlptr = '\0';
	    }
	    success = success || execute(compex,s) != nullptr;
	    if (nlptr)
		*nlptr = ch;
	    if (success && !in_sig)		/* does it match? */
		return true;			/* say, "Eureka!" */
	}
	return false;			/* out of article, so no match */
      }
    }
    return execute(compex,buf) != nullptr;
}
