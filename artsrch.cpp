/* artsrch.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "common.h"
#include "list.h"
#include "ngdata.h"
#include "search.h"
#include "term.h"
#include "util2.h"
#include "intrp.h"
#include "cache.h"
#include "bits.h"
#include "kfile.h"
#include "head.h"
#include "final.h"
#include "ng.h"
#include "ngstuff.h"
#include "artio.h"
#include "rt-util.h"
#include "rt-select.h"
#include "artsrch.h"

char *g_lastpat{""};                 /* last search pattern */
COMPEX g_sub_compex{};               /* last compiled subject search */
COMPEX g_art_compex{};               /* last compiled normal search */
COMPEX *g_bra_compex{&g_art_compex}; /* current compex with brackets */
char g_scopestr[]{"sfHhbBa"};        //
art_scope g_art_howmuch{};           /* search scope */
int g_art_srchhdr{};                 /* specific header number to search */
bool g_art_doread{};                 /* search read articles? */

static bool wanted(COMPEX *compex, ART_NUM artnum, art_scope scope);

void artsrch_init()
{
    init_compex(&g_sub_compex);
    init_compex(&g_art_compex);
}

/* search for an article containing some pattern */

/* if patbuf != g_buf, get_cmd must be set to false!!! */
art_search_result art_search(char *patbuf, int patbufsiz, int get_cmd)
{
    char* pattern;			/* unparsed pattern */
    char cmdchr = *patbuf;	/* what kind of search? */
    char* s;
    bool backward = cmdchr == '?' || cmdchr == Ctl('p');
					/* direction of search */
    COMPEX* compex;			/* which compiled expression */
    char* cmdlst = nullptr;		/* list of commands to do */
    art_search_result ret = SRCH_NOTFOUND; /* assume no commands */
    int saltaway = 0;			/* store in KILL file? */
    art_scope howmuch;			/* search scope: subj/from/Hdr/head/art */
    int srchhdr;			/* header to search if Hdr scope */
    bool topstart = false;
    bool doread;			/* search read articles? */
    bool foldcase = true;		/* fold upper and lower case? */
    int ignorethru = 0;			/* should we ignore the thru line? */
    bool output_level = (!g_use_threads && g_general_mode != 's');
    ART_NUM srchfirst;

    g_int_count = 0;
    if (cmdchr == '/' || cmdchr == '?') {	/* normal search? */
	if (get_cmd && g_buf == patbuf)
	    if (!finish_command(false))	/* get rest of command */
		return SRCH_ABORT;
	compex = &g_art_compex;
	if (patbuf[1]) {
	    howmuch = ARTSCOPE_SUBJECT;
	    srchhdr = SOME_LINE;
	    doread = false;
	}
	else {
	    howmuch = g_art_howmuch;
	    srchhdr = g_art_srchhdr;
	    doread = g_art_doread;
	}
	s = cpytill(g_buf,patbuf+1,cmdchr);/* ok to cpy g_buf+1 to g_buf */
	pattern = g_buf;
	if (*pattern) {
	    if (*g_lastpat)
		free(g_lastpat);
	    g_lastpat = savestr(pattern);
	}
	if (*s) {			/* modifiers or commands? */
	    while (*++s) {
		switch (*s) {
		case 'f':		/* scan the From line */
		    howmuch = ARTSCOPE_FROM;
		    break;
		case 'H':		/* scan a specific header */
		    howmuch = ARTSCOPE_ONEHDR;
		    s = cpytill(g_msg, s+1, ':');
		    srchhdr = get_header_num(g_msg);
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
	g_art_howmuch = howmuch;
	g_art_srchhdr = srchhdr;
	g_art_doread = doread;
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
	compex = &g_sub_compex;
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
		if (!ignorethru && g_kill_thru_kludge)
		    ignorethru = 1;
	    }
	    else if (cmdchr == '.') {
		cmdlst = savestr(".");
		if (!ignorethru && g_kill_thru_kludge)
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
		if (g_verbose)
		    sprintf(g_msg, "Current article has no %s.", finding_str);
	        else
		    sprintf(g_msg, "Null %s.", finding_str);
		errormsg(g_msg);
		ret = SRCH_ABORT;
		goto exit;
	    }
	    if (g_verbose) {
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
    if (g_general_mode == 's') {
	if (!cmdlst) {
	    if (g_sel_mode == SM_ARTICLE)/* set the selector's default command */
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
	    *s++ = g_scopestr[howmuch];
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
	if (g_use_threads)
	    newline();
	else {
	    fputs("\nSearching...\n",stdout) FLUSH;
	    termdown(2);
	}
					/* give them something to read */
    }
    if (ignorethru == 0 && g_kill_thru_kludge && cmdlst
     && (*cmdlst == '+' || *cmdlst == '.'))
	ignorethru = 1;
    srchfirst = doread || g_sel_rereading? g_absfirst
		      : (g_mode != 'k' || ignorethru > 0)? g_firstart : g_killfirst;
    if (topstart || g_art == 0) {
	g_art = g_lastart+1;
	topstart = false;
    }
    if (backward) {
	if (cmdlst && g_art <= g_lastart)
	    g_art++;			/* include current article */
    }
    else {
	if (g_art > g_lastart)
	    g_art = srchfirst-1;
	else if (cmdlst && g_art >= g_absfirst)
	    g_art--;			/* include current article */
    }
    if (g_srchahead > 0) {
	if (!backward)
	    g_art = g_srchahead - 1;
	g_srchahead = -1;
    }
    assert(!cmdlst || *cmdlst);
    perform_status_init(g_ngptr->toread);
    for (;;) {
	/* check if we're out of articles */
	if (backward? ((g_art = article_prev(g_art)) < srchfirst)
		    : ((g_art = article_next(g_art)) > g_lastart))
	    break;
	if (g_int_count) {
	    g_int_count = 0;
	    ret = SRCH_INTR;
	    break;
	}
	g_artp = article_ptr(g_art);
	if (doread || (!(g_artp->flags & AF_UNREAD) ^ !g_sel_rereading)) {
	    if (wanted(compex,g_art,howmuch)) {
				    /* does the shoe fit? */
		if (!cmdlst)
		    return SRCH_FOUND;
		if (perform(cmdlst,output_level && g_page_line == 1) < 0) {
		    free(cmdlst);
		    return SRCH_INTR;
		}
	    }
	    else if (output_level && !cmdlst && !(g_art%50)) {
		printf("...%ld",(long)g_art);
		fflush(stdout);
	    }
	}
	if (!output_level && g_page_line == 1)
	    perform_status(g_ngptr->toread, 60 / (howmuch+1));
    }
exit:
    if (cmdlst)
	free(cmdlst);
    return ret;
}

/* determine if article fits pattern */
/* returns true if it exists and fits pattern, false otherwise */

static bool wanted(COMPEX *compex, ART_NUM artnum, art_scope scope)
{
    ARTICLE* ap = article_find(artnum);

    if (!ap || !(ap->flags & AF_EXISTS))
	return false;

    switch (scope) {
      case ARTSCOPE_SUBJECT:
	strcpy(g_buf,"Subject: ");
	strncpy(g_buf+9,fetchsubj(artnum,false),256);
#ifdef DEBUG
	if (debug & DEB_SEARCH_AHEAD)
	    printf("%s\n",g_buf) FLUSH;
#endif
	break;
      case ARTSCOPE_FROM:
	strcpy(g_buf, "From: ");
	strncpy(g_buf+6,fetchfrom(artnum,false),256);
	break;
      case ARTSCOPE_ONEHDR:
	g_untrim_cache = true;
	sprintf(g_buf, "%s: %s", g_htype[g_art_srchhdr].name,
		prefetchlines(artnum,g_art_srchhdr,false));
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
    return execute(compex,g_buf) != nullptr;
}
