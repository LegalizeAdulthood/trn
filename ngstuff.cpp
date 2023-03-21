/* ngstuff.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "term.h"
#include "util.h"
#include "util2.h"
#include "hash.h"
#include "cache.h"
#include "bits.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "ng.h"
#include "intrp.h"
#include "head.h"
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
#include "decode.h"
#include "addng.h"
#include "opt.h"
#include "only.h"
#include "INTERN.h"
#include "ngstuff.h"
#ifdef MSDOS
#include <direct.h>
#endif

void ngstuff_init() {
    ;
}

/* do a shell escape */

int escapade()
{
    char* s;
    bool interactive = (buf[1] == FINISHCMD);
    bool docd;
    char whereiam[1024];

    if (!finish_command(interactive))	/* get remainder of command */
	return -1;
    s = buf+1;
    docd = *s != '!';
    if (!docd) {
	s++;
    }
    else {
	trn_getwd(whereiam, sizeof(whereiam));
	if (chdir(cwd)) {
	    printf(nocd,cwd) FLUSH;
	    sig_catcher(0);
	}
    }
    while (*s == ' ') s++;
					/* skip leading spaces */
    interp(cmd_buf, (sizeof cmd_buf), s);/* interpret any % escapes */
    resetty();				/* make sure tty is friendly */
    doshell(nullptr,cmd_buf);	/* invoke the shell */
    noecho();				/* and make terminal */
    crmode();				/*   unfriendly again */
    if (docd) {
	if (chdir(whereiam)) {
	    printf(nocd,whereiam) FLUSH;
	    sig_catcher(0);
	}
    }
#ifdef MAILCALL
    mailcount = 0;			/* force recheck */
#endif
    return 0;
}

/* process & command */

int switcheroo()
{
    if (!finish_command(true)) /* get rest of command */
	return -1;	/* if rubbed out, try something else */
    if (!buf[1]) {
	char* prior_savedir = savedir;
	if (option_sel_ilock) {
	    buf[1] = '\0';
	    return 0;
	}
	option_sel_ilock = true;
	if (gmode != 's' || sel_mode != SM_OPTIONS)/*$$*/
	    option_selector();
	option_sel_ilock = false;
	if (savedir != prior_savedir)
	    cwd_check();
	buf[1] = '\0';
    }
    else if (buf[1] == '&') {
	if (!buf[2]) {
	    page_start();
	    show_macros();
	}
	else {
	    char tmpbuf[LBUFLEN];
	    char* s;

	    for (s=buf+2; isspace(*s); s++);
	    mac_line(s,tmpbuf,(sizeof tmpbuf));
	}
    }
    else {
	bool docd = (in_string(buf,"-d", true) != nullptr);
 	char whereami[1024];
	char tmpbuf[LBUFLEN+16];

	if (docd)
	    trn_getwd(whereami, sizeof(whereami));
	if (buf[1] == '-' || buf[1] == '+') {
	    strcpy(tmpbuf,buf+1);
	    sw_list(tmpbuf);
	}
	else {
	    sprintf(tmpbuf,"[options]\n%s\n",buf+1);
	    prep_ini_data(tmpbuf,"'&' input");
	    parse_ini_section(tmpbuf+10,options_ini);
	    set_options(INI_VALUES(options_ini));
	}
	if (docd) {
	    cwd_check();
	    if (chdir(whereami)) {		/* -d does chdirs */
		printf(nocd,whereami) FLUSH;
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
    ART_NUM oldart = art;
    char tmpbuf[LBUFLEN];
    bool output_level = (!use_threads && gmode != 's');
    bool justone = true;		/* assume only one article */

    if (!finish_command(true))	/* get rest of command */
	return NN_INP;
    if (lastart < 1) {
	errormsg("No articles");
	return NN_ASK;
    }
    if (g_srchahead)
	g_srchahead = -1;

    perform_status_init(ngptr->toread);

    for (s=buf; *s && (isdigit(*s) || strchr(" ,-.$",*s)); s++)
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
    safecpy(tmpbuf,buf,LBUFLEN);
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
	if (min < absfirst) {
	    min = absfirst;
	    sprintf(msg,"(First article is %ld)",(long)absfirst);
	    warnmsg(msg);
	}
	if ((s=strchr(s,'-')) != nullptr) {
	    s++;
	    if (*s == '$')
		max = lastart;
	    else if (*s == '.')
		max = oldart;
	    else
		max = atol(s);
	}
	else
	    max = min;
	if (max>lastart) {
	    max = lastart;
	    if (min > max)
		min = max;
	    sprintf(msg,"(Last article is %ld)",(long)lastart) FLUSH;
	    warnmsg(msg);
	}
	if (max < min) {
	    errormsg("Bad range");
	    if (cmdlst)
		free(cmdlst);
	    return NN_ASK;
	}
	if (justone) {
	    art = min;
	    return NN_REREAD;
	}
	for (art = article_first(min); art <= max; art = article_next(art)) {
	    artp = article_ptr(art);
	    if (perform(cmdlst,output_level && page_line == 1) < 0) {
		if (verbose)
		    sprintf(msg,"(Interrupted at article %ld)",(long)art);
		else
		    sprintf(msg,"(Intr at %ld)",(long)art);
		errormsg(msg);
		if (cmdlst)
		    free(cmdlst);
		return NN_ASK;
	    }
	    if (!output_level)
		perform_status(ngptr->toread, 50);
	}
    }
    art = oldart;
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
    bool output_level = (!use_threads && gmode != 's');
    bool one_thread = false;

    if (!finish_command(true))	/* get rest of command */
	return 0;
    if (!buf[1])
	return -1;
    len = 1;
    if (buf[1] == ':') {
	bits = 0;
	len++;
    }
    else
	bits = SF_VISIT;
    if (buf[len] == '.') {
	if (!artp)
	    return -1;
	one_thread = true;
	len++;
    }
    cmdstr = savestr(buf+len);
    want_unread = !sel_rereading && *cmdstr != 'm';

    perform_status_init(ngptr->toread);
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
        performed_article_loop = false;
	if (one_thread)
	    sp = (sel_mode==SM_THREAD? artp->subj->thread->subj : artp->subj);
	else
	    sp = next_subj((SUBJECT*)nullptr,bits);
	for ( ; sp; sp = next_subj(sp,bits)) {
	    if ((!(sp->flags & sel_mask) ^ !bits) || !sp->misc)
		continue;
	    artp = first_art(sp);
	    if (artp) {
		art = article_num(artp);
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
	ART_NUM oldart = art;
	art = lastart+1;
	followup();
	forcegrow = true;
	art = oldart;
	page_line++; /*$$*/
    } else {
	/* The rest loop through the articles. */
	/* Use the explicit article-order if it exists */
	if (g_artptr_list) {
	    ARTICLE** app;
	    ARTICLE** limit = g_artptr_list + g_artptr_list_size;
	    sp = (sel_mode==SM_THREAD? artp->subj->thread->subj : artp->subj);
	    for (app = g_artptr_list; app < limit; app++) {
		ap = *app;
		if (one_thread && ap->subj->thread != sp->thread)
		    continue;
		if ((!(ap->flags & AF_UNREAD) ^ want_unread)
		 && !(ap->flags & sel_mask) ^ !!bits) {
		    art = article_num(ap);
		    artp = ap;
		    if (perform(cmdstr, output_level && page_line == 1) < 0) {
			errormsg("Interrupted");
			goto break_out;
		    }
		}
		if (!output_level)
		    perform_status(ngptr->toread, 50);
	    }
	} else {
	    if (one_thread)
		sp = (sel_mode==SM_THREAD? artp->subj->thread->subj : artp->subj);
	    else
		sp = next_subj((SUBJECT*)nullptr,bits);
	    for ( ; sp; sp = next_subj(sp,bits)) {
		for (ap = first_art(sp); ap; ap = next_art(ap))
		    if ((!(ap->flags & AF_UNREAD) ^ want_unread)
		     && !(ap->flags & sel_mask) ^ !!bits) {
			art = article_num(ap);
			artp = ap;
			if (perform(cmdstr,output_level && page_line==1) < 0) {
			    errormsg("Interrupted");
			    goto break_out;
			}
		    }
		if (one_thread)
		    break;
		if (!output_level)
		    perform_status(ngptr->toread, 50);
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

    /* A quick fix to avoid reuse of buf and cmdlst by shell commands. */
    safecpy(tbuf, cmdlst, sizeof tbuf);
    cmdlst = tbuf;

    if (output_level == 1) {
	printf("%-6ld ",art);
	fflush(stdout);
    }

    g_perform_cnt++;
    for (; (ch = *cmdlst) != 0; cmdlst++) {
	if (isspace(ch) || ch == ':')
	    continue;
	if (ch == 'j') {
	    if (savemode) {
		mark_as_read(artp);
		change_auto_flags(artp, AUTO_KILL_1);
	    }
	    else if (!was_read(art)) {
		mark_as_read(artp);
		if (output_level && verbose)
		    fputs("\tJunked",stdout);
	    }
	    if (sel_rereading)
		deselect_article(artp, output_level? ALSO_ECHO : 0);
	} else if (ch == '+') {
	    if (savemode || cmdlst[1] == '+') {
		if (sel_mode == SM_THREAD)
		    select_arts_thread(artp, savemode? AUTO_SEL_THD : 0);
		else
		    select_arts_subject(artp, savemode? AUTO_SEL_SBJ : 0);
		if (cmdlst[1] == '+')
		    cmdlst++;
	    } else
		select_article(artp, output_level? ALSO_ECHO : 0);
	} else if (ch == 'S') {
	    select_arts_subject(artp, AUTO_SEL_SBJ);
	} else if (ch == '.') {
	    select_subthread(artp, savemode? AUTO_SEL_FOL : 0);
	} else if (ch == '-') {
	    if (cmdlst[1] == '-') {
		if (sel_mode == SM_THREAD)
		    deselect_arts_thread(artp);
		else
		    deselect_arts_subject(artp);
		cmdlst++;
	    } else
		deselect_article(artp, output_level? ALSO_ECHO : 0);
	} else if (ch == ',') {
	    kill_subthread(artp, AFFECT_ALL | (savemode? AUTO_KILL_FOL : 0));
	} else if (ch == 'J') {
	    if (sel_mode == SM_THREAD)
		kill_arts_thread(artp,AFFECT_ALL|(savemode? AUTO_KILL_THD:0));
	    else
		kill_arts_subject(artp,AFFECT_ALL|(savemode? AUTO_KILL_SBJ:0));
	} else if (ch == 'K' || ch == 'k') {
	    kill_arts_subject(artp, AFFECT_ALL|(savemode? AUTO_KILL_SBJ : 0));
	} else if (ch == 'x') {
	    if (!was_read(art)) {
		oneless(artp);
		if (output_level && verbose)
		    fputs("\tKilled",stdout);
	    }
	    if (sel_rereading)
		deselect_article(artp, 0);
	} else if (ch == 't') {
	    entire_tree(artp);
	} else if (ch == 'T') {
	    savemode = 1;
	} else if (ch == 'A') {
	    savemode = 2;
	} else if (ch == 'm') {
	    if (savemode)
		change_auto_flags(artp, AUTO_SEL_1);
	    else if ((artp->flags & (AF_UNREAD|AF_EXISTS)) == AF_EXISTS) {
		unmark_as_read(artp);
		if (output_level && verbose)
		    fputs("\tMarked unread",stdout);
	    }
	}
	else if (ch == 'M') {
	    delay_unmark(artp);
	    oneless(artp);
	    if (output_level && verbose)
		fputs("\tWill return",stdout);
	}
	else if (ch == '=') {
	    carriage_return();
	    output_subject((char*)artp,0);
	    output_level = 0;
	}
	else if (ch == 'C') {
	    int ret = cancel_article();
	    if (output_level && verbose)
		printf("\t%sanceled",ret? "Not c" : "C");
	}
	else if (ch == '%') {
	    char tmpbuf[512];

	    if (one_command)
		interp(tmpbuf, (sizeof tmpbuf), cmdlst);
	    else
		cmdlst = dointerp(tmpbuf,sizeof tmpbuf,cmdlst,":",nullptr) - 1;
	    g_perform_cnt--;
	    if (perform(tmpbuf,output_level?2:0) < 0)
		return -1;
	}
	else if (strchr("!&sSwWae|",ch)) {
	    if (one_command)
		strcpy(buf,cmdlst);
	    else
		cmdlst = cpytill(buf,cmdlst,':') - 1;
	    /* we now have the command in buf */
	    if (ch == '!') {
		escapade();
		if (output_level && verbose)
		    fputs("\tShell escaped",stdout);
	    }
	    else if (ch == '&') {
		switcheroo();
		if (output_level && verbose)
		    if (buf[1] && buf[1] != '&')
			fputs("\tSwitched",stdout);
	    }
	    else {
		if (output_level != 1) {
		    erase_line(false);
		    printf("%-6ld ",art);
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
	    sprintf(msg,"Unknown command: %s",cmdlst);
	    errormsg(msg);
	    return -1;
	}
	if (output_level && verbose)
	    fflush(stdout);
	if (one_command)
	    break;
    }
    if (output_level && verbose)
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
    if (!buf[1])
	return -1;
    len = 1;
    if (buf[1] == ':') {
	bits = 0;
	len++;
    }
    else
	bits = NF_INCLUDED;
    if (buf[len] == '.') {
	if (!ngptr)
	    return -1;
	one_group = true;
	len++;
    }
    cmdstr = savestr(buf+len);

    perform_status_init(newsgroup_toread);
    len = strlen(cmdstr);

    if (one_group) {
	ng_perform(cmdstr, 0);
	goto break_out;
    }

    for (ngptr = first_ng; ngptr; ngptr = ngptr->next) {
	if (sel_rereading? ngptr->toread != TR_NONE
			 : ngptr->toread < ng_min_toread)
	    continue;
	set_ng(ngptr);
	if ((ngptr->flags & bits) == bits
	 && (!(ngptr->flags & sel_mask) ^ !!bits)) {
	    if (ng_perform(cmdstr, 0) < 0)
		break;
	}
	perform_status(newsgroup_toread, 50);
    }

  break_out:
    free(cmdstr);
    return 1;
}

int ng_perform(char *cmdlst, int output_level)
{
    int ch;
    
    if (output_level == 1) {
	printf("%s ",ngname);
	fflush(stdout);
    }

    g_perform_cnt++;
    for (; (ch = *cmdlst) != 0; cmdlst++) {
	if (isspace(ch) || ch == ':')
	    continue;
	switch (ch) {
	  case '+':
	    if (!(ngptr->flags & sel_mask)) {
		ngptr->flags = ((ngptr->flags | sel_mask) & ~NF_DEL);
		selected_count++;
	    }
	    break;
	  case 'c':
	    catch_up(ngptr, 0, 0);
	    /* FALL THROUGH */
	  case '-':
	  deselect:
	    if (ngptr->flags & sel_mask) {
		ngptr->flags &= ~sel_mask;
		if (sel_rereading)
		    ngptr->flags |= NF_DEL;
		selected_count--;
	    }
	    break;
	  case 'u':
	    if (output_level && verbose) {
		printf(unsubto,ngptr->rcline) FLUSH;
		termdown(1);
	    }
	    ngptr->subscribechar = NEGCHAR;
	    ngptr->toread = TR_UNSUB;
	    ngptr->rc->flags |= RF_RCCHANGED;
	    ngptr->flags &= ~sel_mask;
	    newsgroup_toread--;
	    goto deselect;
	  default:
	    sprintf(msg,"Unknown command: %s",cmdlst);
	    errormsg(msg);
	    return -1;
	}
	if (output_level && verbose)
	    fflush(stdout);
	if (one_command)
	    break;
    }
    if (output_level && verbose)
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
    if (!buf[1])
	return -1;
    len = 1;
    if (buf[1] == ':') {
	bits = 0;
	len++;
    }
    else
	bits = sel_mask;
    if (buf[len] == '.') {
	if (g_first_addgroup) /*$$*/
	    return -1;
	one_group = true;
	len++;
    }
    cmdstr = savestr(buf+len);

    perform_status_init(newsgroup_toread);
    len = strlen(cmdstr);

    if (one_group) {
	/*addgrp_perform(gp, cmdstr, 0);$$*/
	goto break_out;
    }

    for (gp = g_first_addgroup; gp; gp = gp->next) {
	if (!(gp->flags & sel_mask) ^ !!bits) {
	    if (addgrp_perform(gp, cmdstr, 0) < 0)
		break;
	}
	perform_status(newsgroup_toread, 50);
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
	    selected_count++;
	} else if (ch == '-') {
	    gp->flags &= ~AGF_SEL;
	    selected_count--;
	}
	else {
	    sprintf(msg,"Unknown command: %s",cmdlst);
	    errormsg(msg);
	    return -1;
	}
	if (output_level && verbose)
	    fflush(stdout);
	if (one_command)
	    break;
    }
    if (output_level && verbose)
	newline();
    if (g_int_count) {
	g_int_count = 0;
	return -1;
    }
    return 1;
}
