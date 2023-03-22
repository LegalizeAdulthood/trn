/* ngstuff.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "term.h"
#include "util.h"
#include "util2.h"
#include "cache.h"
#include "bits.h"
#include "ngdata.h"
#include "ng.h"
#include "intrp.h"
#include "final.h"
#include "sw.h"
#include "rthread.h"
#include "rt-select.h"
#include "rt-wumpus.h"
#include "rt-util.h"
#include "trn.h"
#include "rcln.h"
#include "rcstuff.h"
#include "respond.h"
#include "kfile.h"
#include "addng.h"
#include "opt.h"
#include "ngstuff.h"

#ifdef MSDOS
#include <direct.h>
#endif

bool g_one_command{}; /* no ':' processing in perform() */
/* CAA: given the new and complex universal/help possibilities,
 *      the following interlock variable may save some trouble.
 *      (if true, we are currently processing options)
 */
bool g_option_sel_ilock{};

void ngstuff_init()
{
}

/* do a shell escape */

int escapade()
{
    char* s;
    bool interactive = (g_buf[1] == FINISHCMD);
    bool docd;
    char whereiam[1024];

    if (!finish_command(interactive))	/* get remainder of command */
	return -1;
    s = g_buf+1;
    docd = *s != '!';
    if (!docd) {
	s++;
    }
    else {
	trn_getwd(whereiam, sizeof(whereiam));
	if (chdir(g_cwd)) {
	    printf(g_nocd,g_cwd) FLUSH;
	    sig_catcher(0);
	}
    }
    while (*s == ' ') s++;
					/* skip leading spaces */
    interp(g_cmd_buf, (sizeof g_cmd_buf), s);/* interpret any % escapes */
    resetty();				/* make sure tty is friendly */
    doshell(nullptr,g_cmd_buf);	/* invoke the shell */
    noecho();				/* and make terminal */
    crmode();				/*   unfriendly again */
    if (docd) {
	if (chdir(whereiam)) {
	    printf(g_nocd,whereiam) FLUSH;
	    sig_catcher(0);
	}
    }
#ifdef MAILCALL
    g_mailcount = 0;			/* force recheck */
#endif
    return 0;
}

/* process & command */

int switcheroo()
{
    if (!finish_command(true)) /* get rest of command */
	return -1;	/* if rubbed out, try something else */
    if (!g_buf[1]) {
	char* prior_savedir = g_savedir;
	if (g_option_sel_ilock) {
	    g_buf[1] = '\0';
	    return 0;
	}
	g_option_sel_ilock = true;
	if (g_general_mode != 's' || g_sel_mode != SM_OPTIONS)/*$$*/
	    option_selector();
	g_option_sel_ilock = false;
	if (g_savedir != prior_savedir)
	    cwd_check();
	g_buf[1] = '\0';
    }
    else if (g_buf[1] == '&') {
	if (!g_buf[2]) {
	    page_start();
	    show_macros();
	}
	else {
	    char tmpbuf[LBUFLEN];
	    char* s;

	    for (s=g_buf+2; isspace(*s); s++);
	    mac_line(s,tmpbuf,(sizeof tmpbuf));
	}
    }
    else {
	bool docd = (in_string(g_buf,"-d", true) != nullptr);
 	char whereami[1024];
	char tmpbuf[LBUFLEN+16];

	if (docd)
	    trn_getwd(whereami, sizeof(whereami));
	if (g_buf[1] == '-' || g_buf[1] == '+') {
	    strcpy(tmpbuf,g_buf+1);
	    sw_list(tmpbuf);
	}
	else {
	    sprintf(tmpbuf,"[options]\n%s\n",g_buf+1);
	    prep_ini_data(tmpbuf,"'&' input");
	    parse_ini_section(tmpbuf+10,g_options_ini);
	    set_options(INI_VALUES(g_options_ini));
	}
	if (docd) {
	    cwd_check();
	    if (chdir(whereami)) {		/* -d does chdirs */
		printf(g_nocd,whereami) FLUSH;
		sig_catcher(0);
	    }
	}
    }
    return 0;
}

/* process range commands */

int numnum()
{
    ART_NUM min, max;
    char* cmdlst = nullptr;
    char* s;
    char* c;
    ART_NUM oldart = g_art;
    char tmpbuf[LBUFLEN];
    bool output_level = (!g_use_threads && g_general_mode != 's');
    bool justone = true;		/* assume only one article */

    if (!finish_command(true))	/* get rest of command */
	return NN_INP;
    if (g_lastart < 1) {
	errormsg("No articles");
	return NN_ASK;
    }
    if (g_srchahead)
	g_srchahead = -1;

    perform_status_init(g_ngptr->toread);

    for (s=g_buf; *s && (isdigit(*s) || strchr(" ,-.$",*s)); s++)
	if (!isdigit(*s))
	    justone = false;
    if (*s) {
	cmdlst = savestr(s);
	justone = false;
    }
    else if (!justone)
	cmdlst = savestr("m");
    *s++ = ',';
    *s = '\0';
    safecpy(tmpbuf,g_buf,LBUFLEN);
    if (!output_level && !justone) {
	printf("Processing...");
	fflush(stdout);
    }
    for (s = tmpbuf; (c = strchr(s,',')) != nullptr; s = ++c) {
	*c = '\0';
	if (*s == '.')
	    min = oldart;
	else
	    min = atol(s);
	if (min < g_absfirst) {
	    min = g_absfirst;
	    sprintf(g_msg,"(First article is %ld)",(long)g_absfirst);
	    warnmsg(g_msg);
	}
	if ((s=strchr(s,'-')) != nullptr) {
	    s++;
	    if (*s == '$')
		max = g_lastart;
	    else if (*s == '.')
		max = oldart;
	    else
		max = atol(s);
	}
	else
	    max = min;
	if (max>g_lastart) {
	    max = g_lastart;
	    if (min > max)
		min = max;
	    sprintf(g_msg,"(Last article is %ld)",(long)g_lastart) FLUSH;
	    warnmsg(g_msg);
	}
	if (max < min) {
	    errormsg("Bad range");
	    if (cmdlst)
		free(cmdlst);
	    return NN_ASK;
	}
	if (justone) {
	    g_art = min;
	    return NN_REREAD;
	}
	for (g_art = article_first(min); g_art <= max; g_art = article_next(g_art)) {
	    g_artp = article_ptr(g_art);
	    if (perform(cmdlst,output_level && page_line == 1) < 0) {
		if (g_verbose)
		    sprintf(g_msg,"(Interrupted at article %ld)",(long)g_art);
		else
		    sprintf(g_msg,"(Intr at %ld)",(long)g_art);
		errormsg(g_msg);
		if (cmdlst)
		    free(cmdlst);
		return NN_ASK;
	    }
	    if (!output_level)
		perform_status(g_ngptr->toread, 50);
	}
    }
    g_art = oldart;
    if (cmdlst)
	free(cmdlst);
    return NN_NORM;
}

int thread_perform()
{
    SUBJECT* sp;
    ARTICLE* ap;
    bool want_unread;
    char* cmdstr;
    int len;
    int bits;
    bool output_level = (!g_use_threads && g_general_mode != 's');
    bool one_thread = false;

    if (!finish_command(true))	/* get rest of command */
	return 0;
    if (!g_buf[1])
	return -1;
    len = 1;
    if (g_buf[1] == ':') {
	bits = 0;
	len++;
    }
    else
	bits = SF_VISIT;
    if (g_buf[len] == '.') {
	if (!g_artp)
	    return -1;
	one_thread = true;
	len++;
    }
    cmdstr = savestr(g_buf+len);
    want_unread = !g_sel_rereading && *cmdstr != 'm';

    perform_status_init(g_ngptr->toread);
    len = strlen(cmdstr);

    if (!output_level && !one_thread) {
	printf("Processing...");
	fflush(stdout);
    }
    /* A few commands can just loop through the subjects. */
    if ((len == 1 && (*cmdstr == 't' || *cmdstr == 'J'))
     || (len == 2
      && (((*cmdstr == '+' || *cmdstr == '-') && cmdstr[0] == cmdstr[1])
       || *cmdstr == 'T' || *cmdstr == 'A'))) {
        g_performed_article_loop = false;
	if (one_thread)
	    sp = (g_sel_mode==SM_THREAD? g_artp->subj->thread->subj : g_artp->subj);
	else
	    sp = next_subj((SUBJECT*)nullptr,bits);
	for ( ; sp; sp = next_subj(sp,bits)) {
	    if ((!(sp->flags & g_sel_mask) ^ !bits) || !sp->misc)
		continue;
	    g_artp = first_art(sp);
	    if (g_artp) {
		g_art = article_num(g_artp);
		if (perform(cmdstr, 0) < 0) {
		    errormsg("Interrupted");
		    goto break_out;
		}
	    }
	    if (one_thread)
		break;
	}
#if 0
    } else if (!strcmp(cmdstr, "E")) {
	/* The 'E'nd-decode command doesn't do any looping at all. */
	if (decode_fp)
	    decode_end();
#endif
    } else if (*cmdstr == 'p') {
	ART_NUM oldart = g_art;
	g_art = g_lastart+1;
	followup();
	g_forcegrow = true;
	g_art = oldart;
	page_line++; /*$$*/
    } else {
	/* The rest loop through the articles. */
	/* Use the explicit article-order if it exists */
	if (g_artptr_list) {
	    ARTICLE** app;
	    ARTICLE** limit = g_artptr_list + g_artptr_list_size;
	    sp = (g_sel_mode==SM_THREAD? g_artp->subj->thread->subj : g_artp->subj);
	    for (app = g_artptr_list; app < limit; app++) {
		ap = *app;
		if (one_thread && ap->subj->thread != sp->thread)
		    continue;
		if ((!(ap->flags & AF_UNREAD) ^ want_unread)
		 && !(ap->flags & g_sel_mask) ^ !!bits) {
		    g_art = article_num(ap);
		    g_artp = ap;
		    if (perform(cmdstr, output_level && page_line == 1) < 0) {
			errormsg("Interrupted");
			goto break_out;
		    }
		}
		if (!output_level)
		    perform_status(g_ngptr->toread, 50);
	    }
	} else {
	    if (one_thread)
		sp = (g_sel_mode==SM_THREAD? g_artp->subj->thread->subj : g_artp->subj);
	    else
		sp = next_subj((SUBJECT*)nullptr,bits);
	    for ( ; sp; sp = next_subj(sp,bits)) {
		for (ap = first_art(sp); ap; ap = next_art(ap))
		    if ((!(ap->flags & AF_UNREAD) ^ want_unread)
		     && !(ap->flags & g_sel_mask) ^ !!bits) {
			g_art = article_num(ap);
			g_artp = ap;
			if (perform(cmdstr,output_level && page_line==1) < 0) {
			    errormsg("Interrupted");
			    goto break_out;
			}
		    }
		if (one_thread)
		    break;
		if (!output_level)
		    perform_status(g_ngptr->toread, 50);
	    }
	}
    }
  break_out:
    free(cmdstr);
    return 1;
}

int perform(char *cmdlst, int output_level)
{
    int ch;
    int savemode = 0;
    char tbuf[LBUFLEN+1];

    /* A quick fix to avoid reuse of g_buf and cmdlst by shell commands. */
    safecpy(tbuf, cmdlst, sizeof tbuf);
    cmdlst = tbuf;

    if (output_level == 1) {
	printf("%-6ld ",g_art);
	fflush(stdout);
    }

    g_perform_cnt++;
    for (; (ch = *cmdlst) != 0; cmdlst++) {
	if (isspace(ch) || ch == ':')
	    continue;
	if (ch == 'j') {
	    if (savemode) {
		mark_as_read(g_artp);
		change_auto_flags(g_artp, AUTO_KILL_1);
	    }
	    else if (!was_read(g_art)) {
		mark_as_read(g_artp);
		if (output_level && g_verbose)
		    fputs("\tJunked",stdout);
	    }
	    if (g_sel_rereading)
		deselect_article(g_artp, output_level? ALSO_ECHO : 0);
	} else if (ch == '+') {
	    if (savemode || cmdlst[1] == '+') {
		if (g_sel_mode == SM_THREAD)
		    select_arts_thread(g_artp, savemode? AUTO_SEL_THD : 0);
		else
		    select_arts_subject(g_artp, savemode? AUTO_SEL_SBJ : 0);
		if (cmdlst[1] == '+')
		    cmdlst++;
	    } else
		select_article(g_artp, output_level? ALSO_ECHO : 0);
	} else if (ch == 'S') {
	    select_arts_subject(g_artp, AUTO_SEL_SBJ);
	} else if (ch == '.') {
	    select_subthread(g_artp, savemode? AUTO_SEL_FOL : 0);
	} else if (ch == '-') {
	    if (cmdlst[1] == '-') {
		if (g_sel_mode == SM_THREAD)
		    deselect_arts_thread(g_artp);
		else
		    deselect_arts_subject(g_artp);
		cmdlst++;
	    } else
		deselect_article(g_artp, output_level? ALSO_ECHO : 0);
	} else if (ch == ',') {
	    kill_subthread(g_artp, AFFECT_ALL | (savemode? AUTO_KILL_FOL : 0));
	} else if (ch == 'J') {
	    if (g_sel_mode == SM_THREAD)
		kill_arts_thread(g_artp,AFFECT_ALL|(savemode? AUTO_KILL_THD:0));
	    else
		kill_arts_subject(g_artp,AFFECT_ALL|(savemode? AUTO_KILL_SBJ:0));
	} else if (ch == 'K' || ch == 'k') {
	    kill_arts_subject(g_artp, AFFECT_ALL|(savemode? AUTO_KILL_SBJ : 0));
	} else if (ch == 'x') {
	    if (!was_read(g_art)) {
		oneless(g_artp);
		if (output_level && g_verbose)
		    fputs("\tKilled",stdout);
	    }
	    if (g_sel_rereading)
		deselect_article(g_artp, 0);
	} else if (ch == 't') {
	    entire_tree(g_artp);
	} else if (ch == 'T') {
	    savemode = 1;
	} else if (ch == 'A') {
	    savemode = 2;
	} else if (ch == 'm') {
	    if (savemode)
		change_auto_flags(g_artp, AUTO_SEL_1);
	    else if ((g_artp->flags & (AF_UNREAD|AF_EXISTS)) == AF_EXISTS) {
		unmark_as_read(g_artp);
		if (output_level && g_verbose)
		    fputs("\tMarked unread",stdout);
	    }
	}
	else if (ch == 'M') {
	    delay_unmark(g_artp);
	    oneless(g_artp);
	    if (output_level && g_verbose)
		fputs("\tWill return",stdout);
	}
	else if (ch == '=') {
	    carriage_return();
	    output_subject((char*)g_artp,0);
	    output_level = 0;
	}
	else if (ch == 'C') {
	    int ret = cancel_article();
	    if (output_level && g_verbose)
		printf("\t%sanceled",ret? "Not c" : "C");
	}
	else if (ch == '%') {
	    char tmpbuf[512];

	    if (g_one_command)
		interp(tmpbuf, (sizeof tmpbuf), cmdlst);
	    else
		cmdlst = dointerp(tmpbuf,sizeof tmpbuf,cmdlst,":",nullptr) - 1;
	    g_perform_cnt--;
	    if (perform(tmpbuf,output_level?2:0) < 0)
		return -1;
	}
	else if (strchr("!&sSwWae|",ch)) {
	    if (g_one_command)
		strcpy(g_buf,cmdlst);
	    else
		cmdlst = cpytill(g_buf,cmdlst,':') - 1;
	    /* we now have the command in g_buf */
	    if (ch == '!') {
		escapade();
		if (output_level && g_verbose)
		    fputs("\tShell escaped",stdout);
	    }
	    else if (ch == '&') {
		switcheroo();
		if (output_level && g_verbose)
		    if (g_buf[1] && g_buf[1] != '&')
			fputs("\tSwitched",stdout);
	    }
	    else {
		if (output_level != 1) {
		    erase_line(false);
		    printf("%-6ld ",g_art);
		}
		if (ch == 'a')
		    view_article();
		else
		    save_article();
		newline();
		output_level = 0;
	    }
	}
	else {
	    sprintf(g_msg,"Unknown command: %s",cmdlst);
	    errormsg(g_msg);
	    return -1;
	}
	if (output_level && g_verbose)
	    fflush(stdout);
	if (g_one_command)
	    break;
    }
    if (output_level && g_verbose)
	newline();
    if (g_int_count) {
	g_int_count = 0;
	return -1;
    }
    return 1;
}

int ngsel_perform()
{
    char* cmdstr;
    int len;
    int bits;
    bool one_group = false;

    if (!finish_command(true))	/* get rest of command */
	return 0;
    if (!g_buf[1])
	return -1;
    len = 1;
    if (g_buf[1] == ':') {
	bits = 0;
	len++;
    }
    else
	bits = NF_INCLUDED;
    if (g_buf[len] == '.') {
	if (!g_ngptr)
	    return -1;
	one_group = true;
	len++;
    }
    cmdstr = savestr(g_buf+len);

    perform_status_init(g_newsgroup_toread);
    len = strlen(cmdstr);

    if (one_group) {
	ng_perform(cmdstr, 0);
	goto break_out;
    }

    for (g_ngptr = g_first_ng; g_ngptr; g_ngptr = g_ngptr->next) {
	if (g_sel_rereading? g_ngptr->toread != TR_NONE
			 : g_ngptr->toread < g_ng_min_toread)
	    continue;
	set_ng(g_ngptr);
	if ((g_ngptr->flags & bits) == bits
	 && (!(g_ngptr->flags & g_sel_mask) ^ !!bits)) {
	    if (ng_perform(cmdstr, 0) < 0)
		break;
	}
	perform_status(g_newsgroup_toread, 50);
    }

  break_out:
    free(cmdstr);
    return 1;
}

int ng_perform(char *cmdlst, int output_level)
{
    int ch;
    
    if (output_level == 1) {
	printf("%s ",g_ngname);
	fflush(stdout);
    }

    g_perform_cnt++;
    for (; (ch = *cmdlst) != 0; cmdlst++) {
	if (isspace(ch) || ch == ':')
	    continue;
	switch (ch) {
	  case '+':
	    if (!(g_ngptr->flags & g_sel_mask)) {
		g_ngptr->flags = ((g_ngptr->flags | g_sel_mask) & ~NF_DEL);
		g_selected_count++;
	    }
	    break;
	  case 'c':
	    catch_up(g_ngptr, 0, 0);
	    /* FALL THROUGH */
	  case '-':
	  deselect:
	    if (g_ngptr->flags & g_sel_mask) {
		g_ngptr->flags &= ~g_sel_mask;
		if (g_sel_rereading)
		    g_ngptr->flags |= NF_DEL;
		g_selected_count--;
	    }
	    break;
	  case 'u':
	    if (output_level && g_verbose) {
		printf(g_unsubto,g_ngptr->rcline) FLUSH;
		termdown(1);
	    }
	    g_ngptr->subscribechar = NEGCHAR;
	    g_ngptr->toread = TR_UNSUB;
	    g_ngptr->rc->flags |= RF_RCCHANGED;
	    g_ngptr->flags &= ~g_sel_mask;
	    g_newsgroup_toread--;
	    goto deselect;
	  default:
	    sprintf(g_msg,"Unknown command: %s",cmdlst);
	    errormsg(g_msg);
	    return -1;
	}
	if (output_level && g_verbose)
	    fflush(stdout);
	if (g_one_command)
	    break;
    }
    if (output_level && g_verbose)
	newline();
    if (g_int_count) {
	g_int_count = 0;
	return -1;
    }
    return 1;
}

int addgrp_sel_perform()
{
    ADDGROUP* gp;
    char* cmdstr;
    int len;
    int bits;
    bool one_group = false;

    if (!finish_command(true))	/* get rest of command */
	return 0;
    if (!g_buf[1])
	return -1;
    len = 1;
    if (g_buf[1] == ':') {
	bits = 0;
	len++;
    }
    else
	bits = g_sel_mask;
    if (g_buf[len] == '.') {
	if (g_first_addgroup) /*$$*/
	    return -1;
	one_group = true;
	len++;
    }
    cmdstr = savestr(g_buf+len);

    perform_status_init(g_newsgroup_toread);
    len = strlen(cmdstr);

    if (one_group) {
	/*addgrp_perform(gp, cmdstr, 0);$$*/
	goto break_out;
    }

    for (gp = g_first_addgroup; gp; gp = gp->next) {
	if (!(gp->flags & g_sel_mask) ^ !!bits) {
	    if (addgrp_perform(gp, cmdstr, 0) < 0)
		break;
	}
	perform_status(g_newsgroup_toread, 50);
    }

  break_out:
    free(cmdstr);
    return 1;
}

int addgrp_perform(ADDGROUP *gp, char *cmdlst, int output_level)
{
    int ch;
    
    if (output_level == 1) {
	printf("%s ",gp->name);
	fflush(stdout);
    }

    g_perform_cnt++;
    for (; (ch = *cmdlst) != 0; cmdlst++) {
	if (isspace(ch) || ch == ':')
	    continue;
	if (ch == '+') {
	    gp->flags |= AGF_SEL;
	    g_selected_count++;
	} else if (ch == '-') {
	    gp->flags &= ~AGF_SEL;
	    g_selected_count--;
	}
	else {
	    sprintf(g_msg,"Unknown command: %s",cmdlst);
	    errormsg(g_msg);
	    return -1;
	}
	if (output_level && g_verbose)
	    fflush(stdout);
	if (g_one_command)
	    break;
    }
    if (output_level && g_verbose)
	newline();
    if (g_int_count) {
	g_int_count = 0;
	return -1;
    }
    return 1;
}
