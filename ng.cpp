/* ng.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "trn.h"
#include "term.h"
#include "final.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "cache.h"
#include "bits.h"
#include "artsrch.h"
#include "help.h"
#include "kfile.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "rcstuff.h"
#include "head.h"
#include "mime.h"
#include "art.h"
#include "artio.h"
#include "ngstuff.h"
#include "intrp.h"
#include "respond.h"
#include "backpage.h"
#include "rcln.h"
#include "rthread.h"
#include "rt-select.h"
#include "rt-wumpus.h"
#include "rt-util.h"
#include "charsubst.h"
#include "smisc.h"
#include "scanart.h"
#include "score.h"
#include "univ.h"
#include "artstate.h"
#include "color.h"
#include "ng.h"
#ifdef MSDOS
#include <direct.h>
#endif

ART_NUM g_art{};          /* current or prospective article # */
ART_NUM g_recent_art{};   /* previous article # for '-' command */
ART_NUM g_curr_art{};     /* current article # */
ARTICLE *g_recent_artp{}; /* article_ptr equivilents */
ARTICLE *g_curr_artp{};
ARTICLE *g_artp{};     /* the article ptr we use when art is 0 */
int g_checkcount{};    /* how many articles have we read in the current newsgroup since the last checkpoint? */
int g_docheckwhen{20}; /* how often to do checkpoint */
char *g_subjline{};    /* what format to use for '=' */
#ifdef MAILCALL
int g_mailcount{}; /* check for mail when 0 mod 10 */
#endif
char *g_mailcall{""};
bool g_forcelast{}; /* ought we show "End of newsgroup"? */
bool g_forcegrow{}; /* do we want to recalculate size of newsgroup, e.g. after posting? */

/* art_switch() return values */
enum
{
    AS_NORM = 0,
    AS_INP = 1,
    AS_ASK = 2,
    AS_CLEAN = 3,
    AS_SA = 4,
    AS_QUITNOW = 5,
    AS_SV = 6
};

static bool count_unCACHED_article(char *ptr, int arg);
static bool mark_all_READ(char *ptr, int leave_unread);
static bool mark_all_unREAD(char *ptr, int arg);
#ifdef DEBUG
static bool debug_article_output(char *ptr, int arg);
#endif

static int s_exit_code = NG_NORM;

void ng_init()
{
    setdfltcmd();

    open_kfile(KF_GLOBAL);
    init_compex(&g_hide_compex);
    init_compex(&g_page_compex);
}

/* do newsgroup pointed to by g_ngptr with name ngname
 *
 * The basic structure is:
 *	for each desired article
 *		for each desired page
 *			for each line on page
 *				if we need another line from file
 *					get it
 *					if it's a header line
 *						do special things
 *				for each column on page
 *					put out a character
 *				end loop
 *			end loop
 *		end loop
 *	end loop
 *
 *	(Actually, the pager is in another routine.)
 *
 * The chief problem is deciding what is meant by "desired".  Most of
 * the messiness of this routine is due to the fact that people want
 * to do unstructured things all the time.  I have used a few judicious
 * goto's where I thought it improved readability.  The rest of the messiness
 * arises from trying to be both space and time efficient.  Have fun.
 */

// start_command command to fake up first
int do_newsgroup(char *start_command)
{
    char mode_save = mode;
    char gmode_save = gmode;
    int ret;
    char* whatnext = "%s%sWhat next? [%s]";
    bool ng_virtual = false;

    set_datasrc(g_ngptr->rc->datasrc);

    if (chdir(g_datasrc->spool_dir)) {
	printf(nocd,g_datasrc->spool_dir) FLUSH;
	return -1;
    }

    s_exit_code = NG_NORM;
    g_kf_state &= ~(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES
		 |KFS_NORMAL_LINES  | KFS_THREAD_LINES);
    g_killfirst = 0;

    safefree0(extractdest);
    safefree0(extractprog);

    /* initialize the newsgroup data structures */

    sel_rereading = false;
    sel_mask = AF_SEL;
    ret = access_ng();
    if (ret == -2)
	return NG_NOSERVER;
    if (ret <= 0)
	return NG_ERROR;

    g_srchahead = (scanon && !g_threaded_group	/* did they say -S? */
	      && ((ART_NUM)g_ngptr->toread) >= scanon ? -1 : 0);

    /* FROM HERE ON, RETURN THRU CLEANUP OR WE ARE SCREWED */

    g_forcelast = true;			/* if 0 unread, do not bomb out */
    g_recent_artp = g_curr_artp = nullptr;
    g_recent_art = g_curr_art = g_lastart+1;
    g_prompt = whatnext;
    g_charsubst = g_charsets;

    /* see if there are any special searches to do */

    open_kfile(KF_LOCAL);
    if (verbose)
	kill_unwanted(g_firstart,"Processing memorized commands...\n\n", true);
    else
	kill_unwanted(g_firstart,"Auto-processing...\n\n",true);

    sc_init((sa_never_initialized || sa_mode_order == 2)
            && start_command && *start_command == ';');

    if (univ_ng_virtflag) {
	univ_ng_virtual();
	goto cleanup;
    }
    if (!selected_count)
	selected_only = false;
    top_article();

    /* do they want a special top line? */

    g_firstline = get_val("FIRSTLINE",nullptr);

    /* custom line suppression, custom page ending */

    if ((g_hideline = get_val("HIDELINE",nullptr)) != nullptr)
	compile(&g_hide_compex,g_hideline,true,true);
    if ((g_pagestop = get_val("PAGESTOP",nullptr)) != nullptr)
	compile(&g_page_compex,g_pagestop,true,true);

    /* now read each unread article */

    /* CAA: if we are going directly to an article, set things up here */
    if (g_ng_go_artnum) {
	ng_virtual = true;
	if (g_ng_go_artnum >= g_absfirst) {
	    g_art = g_ng_go_artnum;
	    g_artp = article_ptr(g_art);
	}
	g_ng_go_artnum = 0;
    }
    else if (g_ng_go_msgid) {
	/* not implemented yet */
	ng_virtual = true;
	g_ng_go_msgid = 0;
    }

    g_doing_ng = true;			/* enter the twilight zone */
    g_ngptr->rc->flags |= RF_RCCHANGED;
    if (!unsafe_rc_saves)
	g_checkcount = 0;			/* do not checkpoint for a while */
    g_do_fseek = false;			/* start 1st article at top */
    for (; g_art <= g_lastart+1; ) {	/* for each article */
	set_mode('r','a');

	/* do we need to "grow" the newsgroup? */

	if ((g_art > g_lastart || g_forcegrow) && !keep_the_group_static) {
	    ART_NUM oldlast = g_lastart;
	    if (g_artsize < 0)
		nntp_finishbody(FB_SILENT);
	    if (g_datasrc->flags & DF_REMOTE) {
		if (g_datasrc->act_sf.fp || getngsize(g_ngptr) > g_lastart) {
		    if (nntp_group(ngname,g_ngptr) <= 0) {
			s_exit_code = NG_NOSERVER;
			goto cleanup;
		    }
		    grow_ng(g_ngptr->ngmax);
		}
	    }
	    else
		grow_ng(getngsize(g_ngptr));
	    if (g_forcelast && g_art > oldlast)
		g_art = g_lastart+1;
	}
	if (g_art != 0 || (g_artp && !(g_artp->flags & AF_TMPMEM)))
	    g_artp = article_find(g_art);
	if (start_command) {		/* do we have an initial command? */
	    if (start_command == "") {
		if (UseNewsSelector >= 0
		 && !ng_virtual
		 && g_ngptr->toread >= (ART_UNREAD)UseNewsSelector)
		    pushchar('+');
	    }
	    else {
		hide_pending();
		pushstring(start_command, 0);
		free(start_command);
	    }
	    start_command = nullptr;
	    if (input_pending()) {
		g_art = g_curr_art = g_lastart+1;
		g_artp = g_curr_artp = nullptr;
		goto reinp_article;
	    }
	}
	if (g_art > g_lastart) {		/* are we off the end still? */
	    g_art = g_lastart + 1;		/* keep pointer references sane */
	    if (!g_forcelast && g_ngptr->toread && selected_only && !selected_count) {
		g_art = g_curr_art;
		g_artp = g_curr_artp;
		strcpy(buf, "+");
		goto article_level;
	    }
	    count_subjects(CS_RETAIN);
	    article_walk(count_unCACHED_article, 0);
	    g_ngptr->toread = (ART_UNREAD)obj_count;
	    if (g_artp != g_curr_artp) {
		g_recent_art = g_curr_art;	/* remember last article # (for '-') */
		g_curr_art = g_art;		/* set current article # */
		g_recent_artp = g_curr_artp;
		g_curr_artp = g_artp;
		g_charsubst = g_charsets;
		g_first_view = 0;
	    }
	    if (sa_in) {
		sa_go = true;
		goto article_level;
	    }
	    if (erase_screen)
		clear();			/* clear the screen */
	    else {
		fputs("\n\n",stdout) FLUSH;
		termdown(2);
	    }
	    if (verbose)
		printf("End of newsgroup %s.",ngname);
					/* print pseudo-article */
	    else
		printf("End of %s",ngname);
	    if (obj_count) {
		if (selected_only)
		    printf("  (%ld + %ld articles still unread)",
			(long)selected_count,
			(long)obj_count-selected_count);
		else
		    printf("  (%ld article%s still unread)",
			(long)obj_count,PLURAL(obj_count));
	    }
	    if (g_redirected) {
		if (g_redirected == "")
		    printf("\n\n** This group has been disabled by your news admin **");
		else
		    printf("\n\n** Please start using %s **", g_redirected);
		termdown(2);
	    }
	    else if (!obj_count && !g_forcelast)
		goto cleanup;		/* actually exit newsgroup */
	    set_mode(gmode,'e');
	    g_prompt = whatnext;
	    g_srchahead = 0;		/* no more subject search mode */
	    fputs("\n\n",stdout) FLUSH;
	    termdown(2);
	}
	else if (!g_reread && (was_read(g_art)
		|| (selected_only && !(g_artp->flags & AF_SEL)))) {
					/* has this article been read? */
	    inc_art(selected_only,false);/* then skip it */
	    continue;
	}
	else if (!g_reread && (!(g_artp->flags & AF_EXISTS) || !parseheader(g_art))) {
	    oneless(g_artp);		/* mark deleted as read */
	    ng_skip();
	    continue;
	}
	else {				/* we have a real live article */
	    if (g_artp != g_curr_artp) {
		g_recent_art = g_curr_art;	/* remember last article # (for '-') */
		g_curr_art = g_art;		/* set current article # */
		g_recent_artp = g_curr_artp;
		g_curr_artp = g_artp;
		g_charsubst = g_charsets;
		g_first_view = 0;
		g_do_hiding = true;
		g_rotate = false;
	    }
	    if (!g_do_fseek) {		/* starting at top of article? */
		artline = 0;		/* start at the beginning */
		g_topline = -1;		/* and remember top line of screen */
					/*  (line # within article file) */
	    }
	    clear();			/* clear screen */
	    if (g_art == 0 && g_artp && g_artp->msgid && (g_datasrc->flags&DF_REMOTE)
	     && !(g_artp->flags & AF_CACHED)) {
		g_art = nntp_stat_id(g_artp->msgid);
		if (g_art < 0) {
		    s_exit_code = NG_NOSERVER;
		    goto cleanup;
		}
		if (g_art)
		    g_artp = article_find(g_art);
	    }
	    /* make sure article is found & open */
	    if (!artopen(g_art,(ART_POS)0)) {
		char tmpbuf[256];
		ART_LINE linenum;
		/* see if we have tree data for this article anyway */
		init_tree();
		sprintf(tmpbuf,"%s: article is not available.",ngname);
		if (g_artp && !(g_artp->flags & AF_CACHED)) {
		    if (g_absfirst < g_first_cached || g_last_cached < g_lastart
		     || !g_cached_all_in_range)
			sprintf(tmpbuf,"%s: article may show up in a moment.",
				ngname);
		}
		linenum = tree_puts(tmpbuf,0,0);
		vwtary(artline,(ART_POS)0);
		finish_tree(linenum);
		g_prompt = whatnext;
		g_srchahead = 0;
	    }
	    else {			/* found it, so print it */
		switch (do_article()) {
		case DA_CLEAN:		/* quit newsgroup */
		    goto cleanup;
		case DA_TOEND:		/* do not mark as read */
		    goto reask_article; 
		case DA_RAISE:		/* reparse command at end of art */
		    goto article_level;
		case DA_NORM:		/* normal end of article */
		    break;
		}
	    }
	    if (g_art >= g_absfirst)	/* don't mark non-existant articles */
		mark_as_read(g_artp);	/* mark current article as read */
	}

/* if these gotos bother you, think of this as a little state machine */

reask_article:
#ifdef MAILCALL
	setmail(false);
#endif
	setdfltcmd();
	if (erase_screen && erase_each_line)
	    erase_line(true);
	if (g_term_line >= tc_LINES) {
	    term_scrolled += g_term_line - tc_LINES + 1;
	    g_term_line = tc_LINES-1;
	}
	unflush_output();		/* disable any ^O in effect */
	/* print prompt, whatever it is */
	interp(cmd_buf, sizeof cmd_buf, g_mailcall);
	sprintf(buf,g_prompt,cmd_buf,
		current_charsubst(),
		dfltcmd);
	draw_mousebar(tc_COLS - (g_term_line == tc_LINES-1? strlen(buf)+5 : 0), true);
	color_string(COLOR_CMD,buf);
	putchar(' ');
	fflush(stdout);
	g_term_col = strlen(buf) + 1;
reinp_article:
	g_reread = false;
	g_forcelast = false;
	eat_typeahead();
#ifdef PENDING
	look_ahead();			/* see what we can do in advance */
	cache_until_key();
#endif
	g_art = g_curr_art;
	g_artp = g_curr_artp;
	getcmd(buf);
	if (errno || *buf == '\f') {
	    if (tc_LINES < 100 && !g_int_count)
		*buf = '\f';		/* on CONT fake up refresh */
	    else {
		newline();		/* but only on a crt */
		goto reask_article;
	    }
	}
article_level:
	output_chase_phrase = true;  /* Allow "Chasing Xrefs..." output */
	if (mousebar_cnt)
	    clear_rest();

	if (sa_go) {
	    switch (sa_main()) {
	      case SA_NORM:
		continue;		/* ...the article (for) loop */
	      case SA_NEXT:		/* goto next newsgroup */
		s_exit_code = NG_SELNEXT;
		goto cleanup;
	      case SA_PRIOR:		/* goto prior newsgroup */
		s_exit_code = NG_SELPRIOR;
		goto cleanup;
	      case SA_QUIT:
	      case SA_ERR:
		goto cleanup;
	      case SA_QUIT_SEL:
		s_exit_code = NG_ASK;
		goto cleanup;
	      case SA_FAKE:
		lastchar = buf[0];	/* needed for fake to work */
		break;			/* fall through to art_switch */
	    }
	}

	/* parse and process article level command */

	switch (art_switch()) {
	  case AS_INP:			/* multichar command rubbed out */
	    goto reinp_article;
	  case AS_ASK:			/* reprompt "End of article..." */
	    goto reask_article;
	  case AS_CLEAN:		/* exit newsgroup */
	    goto cleanup;
	  case AS_QUITNOW:		/* just leave, cleanup already done */
	    goto cleanup2;
	  case AS_NORM:			/* display article art */
	    break;
	  case AS_SA:			/* go to article scan mode */
	    sa_go = true;
	    goto article_level;
	}
    }					/* end of article selection loop */
    
/* shut down newsgroup */

cleanup:
    kill_unwanted(g_firstart,"\nCleaning up...\n\n",false);
					/* do cleanup from KILL file, if any */
    if (sa_initialized)
	sa_cleanup();
    if (sc_initialized)
	sc_cleanup();
    chase_xrefs(false);
    if (!univ_ng_virtflag) {
    }

    in_ng = false;			/* leave newsgroup state */
    artclose();
    if (!univ_ng_virtflag)
	newline();
    deselect_all();
    yankback();				/* do a Y command */
    bits_to_rc();			/* reconstitute .newsrc line */
cleanup2:
/* go here if already cleaned up */
    g_doing_ng = false;			/* tell sig_catcher to cool it */
    /* XXX later, make an option for faster/less-safe virtual groups */
    if (!univ_ng_virtflag &&
	!(univ_read_virtflag && !(univ_follow || univ_follow_temp))) {
	if (!unsafe_rc_saves) {
	    if (!write_newsrcs(g_multirc)) /* and update .newsrc */
		get_anything();
	    update_thread_kfile();
	}
    }

    if (g_localkfp) {
	fclose(g_localkfp);
	g_localkfp = nullptr;
    }
    set_mode(gmode_save,mode_save);
    return s_exit_code;
}					/* Whew! */

/* decide what to do at the end of an article */

int art_switch()
{
    setdef(buf,dfltcmd);
    printcmd();

    buf[2] = '\0';
    switch (*buf) {
      case Ctl('v'):		/* verify signature */
	verify_sig();
	return AS_ASK;
      case ';':			/* enter ScanArticle mode */
	sa_go_explicit = true;
	return AS_SA;
      case '"':			/* append to local SCORE file */
	buf[0] = ':';		/* enter command on next line */
	buf[1] = FINISHCMD;
	printf("\nEnter score append command or type RETURN for a menu\n");
	termdown(2);
	fflush(stdout);
	if (finish_command(true))	/* command entered successfully */
	    sc_append(buf+1);
	return AS_ASK;
      case '\'':		/* execute scoring command */
	buf[0] = ':';
	buf[1] = FINISHCMD;
	printf("\nEnter scoring command or type RETURN for a menu\n");
	termdown(2);
	fflush(stdout);
	if (finish_command(true))	/* command entered successfully */
	    sc_score_cmd(buf+1);
	return AS_ASK;
      case '<':			/* goto previous subject/thread */
	visit_prev_thread();
	return AS_NORM;
      case '>':			/* goto next subject/thread */
	visit_next_thread();
	return AS_NORM;
      case 'U': {		/* unread some articles */
	const char* u_prompt;
	const char* u_help_thread;

	if (!g_artp) {
	    u_help_thread = "";
	    if (verbose)
		u_prompt = "\nUnkill: +select or all?";
	    else
		u_prompt = "\nUnkill?";
	    dfltcmd = "+anq";
	}
	else {
	    if (verbose) {
		u_prompt = "\n\
Unkill: +select, thread, subthread, or all?";
		u_help_thread = "\
Type t or SP to mark this thread's articles as unread.\n\
Type s to mark the current article and its descendants as unread.\n";
	    }
	    else
	    {
		u_prompt = "\nUnkill?";
		u_help_thread = "\
t or SP to mark thread unread.\n\
s to mark subthread unread.\n";
	    }
	    dfltcmd = "+tsanq";
	}
      reask_unread:
	in_char(u_prompt,'u',dfltcmd);
	printcmd();
	newline();
	if (*buf == 'h') {
	    if (verbose)
	    {
		fputs("\
Type + to enter select thread mode using all the already-read articles.\n\
(The selected threads will be marked as unread and displayed as usual.)\n\
",stdout) FLUSH;
		fputs(u_help_thread,stdout);
		fputs("\
Type a to mark all articles in this group as unread.\n\
Type n or q to change nothing.\n\
",stdout) FLUSH;
		termdown(6);
	    }
	    else
	    {
		fputs("\
+ to select threads from the unread.\n\
",stdout) FLUSH;
		fputs(u_help_thread,stdout);
		fputs("\
a to mark all articles unread.\n\
n or q to change nothing.\n\
",stdout) FLUSH;
		termdown(5);
	    }
	    goto reask_unread;
	}
	else if (*buf == 'n' || *buf == 'q')
	    return AS_ASK;
	else if (*buf == 't' && *u_help_thread) {
	    if (g_artp->subj->thread)
		unkill_thread(g_artp->subj->thread);
	    else
		unkill_subject(g_artp->subj);
	    if ((g_artp = first_art(g_artp->subj)) != nullptr)
		g_art = article_num(g_artp);
	}
	else if (*buf == 's' && *u_help_thread)
	    unkill_subthread(g_artp);
	else if (*buf == 'a') {
	    check_first(g_absfirst);
	    article_walk(mark_all_unREAD, 0);
	    count_subjects(CS_NORM);
	    g_ngptr->toread = (ART_UNREAD)obj_count;
	}
	else if (*buf == '+') {
	    *buf = 'U';
	    goto run_the_selector;
	}
	else {
	    fputs(hforhelp,stdout) FLUSH;
	    termdown(1);
	    settle_down();
	    goto reask_unread;
	}
	return AS_NORM;
      }
      case '[':			/* goto parent article */
      case '{':			/* goto thread's root article */
	if (g_artp && g_threaded_group) {
	    if (!find_parent(*buf == '{')) {
		char* cp = (*buf=='['?"parent":"root");
		if (verbose)
		    printf("\nThere is no %s article prior to this one.\n",
			cp) FLUSH;
		else
		    printf("\nNo prior %s.\n",cp) FLUSH;
		termdown(2);
		return AS_ASK;
	    }
	    g_reread = true;
	    s_follow_temp = true;
	    univ_follow_temp = true;
	    return AS_NORM;
	}
not_threaded:
	if (!g_artp) {
	    if (verbose)
		fputs("\nYou're at the end of the group.\n",stdout) FLUSH;
	    else
		fputs("\nEnd of group.\n",stdout) FLUSH;
	    termdown(2);
	    return AS_ASK;
	}
	if (verbose)
	    fputs("\nThis group is not threaded.\n",stdout) FLUSH;
	else
	    fputs("\nUnthreaded group.\n",stdout) FLUSH;
	termdown(2);
	return AS_ASK;
      case ']':			/* goto child article */
      case '}':			/* goto thread's leaf article */
	if (g_artp && g_threaded_group) {
	    if (!find_leaf(*buf == '}')) {
		if (verbose)
		    fputs("\n\
This is the last leaf in this tree.\n",stdout) FLUSH;
		else
		    fputs("\nLast leaf.\n",stdout) FLUSH;
		termdown(2);
		return AS_ASK;
	    }
	    g_reread = true;
	    s_follow_temp = true;
	    univ_follow_temp = true;
	    return AS_NORM;
	}
	goto not_threaded;
      case '(':			/* goto previous sibling */
      case ')':			/* goto next sibling */
	if (g_artp && g_threaded_group) {
	    if (!(*buf == '(' ? find_prev_sib() : find_next_sib())) {
		char* cp = (*buf == '(' ? "previous" : "next");
		if (verbose)
		    printf("\nThis article has no %s sibling.\n",cp) FLUSH;
		else
		    printf("\nNo %s sibling.\n",cp) FLUSH;
		termdown(2);
		return AS_ASK;
	    }
	    g_reread = true;
	    s_follow_temp = true;
	    univ_follow_temp = true;
	    return AS_NORM;
	}
	goto not_threaded;
      case 'T':
	if (!g_threaded_group)
	    goto not_threaded;
	/* FALL THROUGH */
      case 'A':
	if (!g_artp)
	    goto not_threaded;
	switch (ask_memorize(*buf)) {
	  case ',':  case 'J': case 'K': case 'j':
	    return AS_NORM;
	}
	return AS_ASK;
    case 'K':
	if (!g_artp)
	    goto not_threaded;
	/* first, write kill-subject command */
	(void)art_search(buf, (sizeof buf), true);
	g_art = g_curr_art;
	g_artp = g_curr_artp;
	kill_subject(g_artp->subj,AFFECT_ALL);/* take care of any prior subjects */
	if (sa_in && !(sa_follow || s_follow_temp))
	    return AS_SA;
	return AS_NORM;
      case ',':		/* kill this node and all descendants */
	if (!g_artp)
	    goto not_threaded;
	if (g_threaded_group)
	    kill_subthread(g_artp,AFFECT_ALL);
	else if (g_art >= g_absfirst && g_art <= g_lastart)
	    mark_as_read(g_artp);
	if (sa_in && !(sa_follow || s_follow_temp))
	    return AS_SA;
	return AS_NORM;
      case 'J':		/* Junk all nodes in this thread */
	if (!g_artp)
	    goto not_threaded;
	if (g_threaded_group) {
	    kill_thread(g_artp->subj->thread,AFFECT_ALL);
	    if (sa_in)
		return AS_SA;
	    return AS_NORM;
	}
	/* FALL THROUGH */
      case 'k':		/* kill current subject */
	if (!g_artp)
	    goto not_threaded;
	kill_subject(g_artp->subj,AFFECT_ALL);
	if (!g_threaded_group || g_last_cached < g_lastart) {
	    *buf = 'k';
	    goto normal_search;
	}
	if (sa_in && !(sa_follow || s_follow_temp))
	    return AS_SA;
	return AS_NORM;
      case 't':
	erase_line(erase_screen && erase_each_line);
	page_line = 1;
	entire_tree(g_curr_artp);
	return AS_ASK;
      case ':':			/* execute command on selected articles */
	page_line = 1;
	if (!thread_perform())
	    return AS_INP;
	carriage_return();
	perform_status_end(g_ngptr->toread, "article");
	fputs(msg, stdout);
	newline();
	g_art = g_curr_art;
	g_artp = g_curr_artp;
	return AS_ASK;
      case 'p':			/* find previous unread article */
	s_follow_temp = true;	/* keep going until change req. */
	univ_follow_temp = true;
	do {
	    dec_art(selected_only,false);
	} while (g_art >= g_firstart && (was_read(g_art) || !parseheader(g_art)));
	g_srchahead = 0;
	if (g_art >= g_firstart)
	    return AS_NORM;
	g_art = g_absfirst;	
	/* FALL THROUGH */
      case 'P':		/* goto previous article */
	s_follow_temp = true;	/* keep going until change req. */
	univ_follow_temp = true;
	dec_art(false,true);
      check_dec_art:
	if (g_art < g_absfirst) {
	    if (verbose)
		printf("\nThere are no%s%s articles prior to this one.\n",
			*buf=='P'?"":" unread",
			selected_only?" selected":"") FLUSH;
	    else
		printf("\nNo previous%s%s articles\n",
			*buf=='P'?"":" unread",
			selected_only?" selected":"") FLUSH;
	    termdown(2);
	    g_art = g_curr_art;
	    g_artp = g_curr_artp;
	    return AS_ASK;
	}
	g_reread = true;
	g_srchahead = 0;
	return AS_NORM;
      case '-':
      case '\b':  case '\177':
	if (g_recent_art >= 0) {
	    g_art = g_recent_art;
	    g_artp = g_recent_artp;
	    g_reread = true;
	    g_forcelast = true;
	    g_srchahead = -(g_srchahead != 0);
	    return AS_NORM;
	}
	s_exit_code = NG_MINUS;
	return AS_CLEAN;
      case 'n':		/* find next unread article? */
	if (sa_in && s_default_cmd && !(sa_follow || s_follow_temp))
	    return AS_SA;
        if (univ_read_virtflag && univ_default_cmd && !(sa_in && (sa_follow || s_follow_temp)) &&
            !(univ_follow || univ_follow_temp))
        {
	    s_exit_code = NG_NEXT;
	    return AS_CLEAN;
	}
	if (!univ_default_cmd)
	    univ_follow_temp = true;
	if (!s_default_cmd)
	    s_follow_temp = true;	/* keep going until change req. */
	if (g_art > g_lastart) {
	    if (!g_ngptr->toread)
		return AS_CLEAN;
	    top_article();
	    if (sa_in)
		return AS_SA;
	}
	else if (scanon && !g_threaded_group && g_srchahead) {
	    *buf = Ctl('n');
	    if (!next_art_with_subj())
		goto normal_search;
	    return AS_NORM;
	}
	else {
	    /* $$ will this work with 4.0? CAA */
	    if (sa_in && g_threaded_group) {
		ARTICLE* old_artp = g_artp;
		inc_art(selected_only,false);
		if (!g_artp || !old_artp)
		    return AS_SA;
		switch (sel_mode) {
		  case SM_ARTICLE:
		    if (s_default_cmd)
			return AS_SA;
		    break;
		  case SM_SUBJECT:
		    if (old_artp->subj != g_artp->subj)
			return AS_SA;
		    break;
		  case SM_THREAD:
		    if (old_artp->subj->thread != g_artp->subj->thread)
			return AS_SA;
		    break;
		  default:
		    /* HUH?  Just hope for the best */
		    break;
		}
	    } else
		inc_art(selected_only,false);
	    if (g_art > g_lastart)
		top_article();
	}
	g_srchahead = 0;
	return AS_NORM;
      case 'N':			/* goto next article */
	if (sa_in && s_default_cmd && !(sa_follow || s_follow_temp))
	    return AS_SA;
        if (univ_read_virtflag && univ_default_cmd && !(sa_in && (sa_follow || s_follow_temp)) &&
            !(univ_follow || univ_follow_temp))
        {
	    s_exit_code = NG_NEXT;
	    return AS_CLEAN;
	}
	if (!univ_default_cmd)
	    univ_follow_temp = true;
	if (!s_default_cmd)
	    s_follow_temp = true;	/* keep going until change req. */
	if (g_art > g_lastart) {
	    if (!g_first_subject) {
		g_art = g_absfirst;
		g_artp = article_ptr(g_art);
	    } else {
		g_artp = g_first_subject->articles;
		if (g_artp->flags & AF_EXISTS)
		    g_art = article_num(g_artp);
		else
		    inc_art(false,true);
	    }
	}
	else
	    inc_art(false,true);
	if (g_art <= g_lastart)
	    g_reread = true;
	else
	    g_forcelast = true;
	g_srchahead = 0;
	return AS_NORM;
      case '$':
	g_art = g_lastart+1;
	g_artp = nullptr;
	g_forcelast = true;
	g_srchahead = 0;
	return AS_NORM;
      case '0':
      case '1': case '2': case '3':	/* goto specified article */
      case '4': case '5': case '6':	/* or do something with a range */
      case '7': case '8': case '9': case '.':
	g_forcelast = true;
	switch (numnum()) {
	  case NN_INP:
	    return AS_INP;
	  case NN_ASK:
	    return AS_ASK;
	  case NN_REREAD:
	    g_reread = true;
	    if (g_srchahead)
		g_srchahead = -1;
	    break;
	  case NN_NORM:
	    if (use_threads) {
		erase_line(false);
		perform_status_end(g_ngptr->toread, "article");
		fputs(msg, stdout) FLUSH;
	    }
	    newline();
	    return AS_ASK;
	}
	return AS_NORM;
      case Ctl('k'):
	edit_kfile();
	return AS_ASK;
      case Ctl('n'):	/* search for next article with same subject */
      case Ctl('p'):	/* search for previous article with same subject */
        if (sa_in && s_default_cmd && *buf == Ctl('n') && !(sa_follow || s_follow_temp))
            return AS_SA;
        if (univ_read_virtflag && univ_default_cmd && (*buf == Ctl('n')) && !(sa_in && (sa_follow || s_follow_temp)) &&
            !(univ_follow || univ_follow_temp))
        {
	    s_exit_code = NG_NEXT;
	    return AS_CLEAN;
	}
	if (!univ_default_cmd)
	    univ_follow_temp = true;
	if (!s_default_cmd)
	    s_follow_temp = true;	/* keep going until change req. */
	if (*buf == Ctl('n')? next_art_with_subj() : prev_art_with_subj())
	    return AS_NORM;
      case '/': case '?':
normal_search:
      {		/* search for article by pattern */
	char cmd = *buf;
	
	g_reread = true;		/* assume this */
	page_line = 1;
	switch (art_search(buf, (sizeof buf), true)) {
	  case SRCH_ERROR:
	    g_art = g_curr_art;
	    return AS_ASK;
	  case SRCH_ABORT:
	    g_art = g_curr_art;
	    return AS_INP;
	  case SRCH_INTR:
	    if (verbose)
		printf("\n(Interrupted at article %ld)\n",(long)g_art) FLUSH;
	    else
		printf("\n(Intr at %ld)\n",(long)g_art) FLUSH;
	    termdown(2);
	    g_art = g_curr_art;	    /* restore to current article */
	    return AS_ASK;
	  case SRCH_DONE:
	    if (use_threads) {
		erase_line(false);
		perform_status_end(g_ngptr->toread, "article");
		printf("%s\n",msg) FLUSH;
	    }
	    else
		fputs("done\n",stdout) FLUSH;
	    termdown(1);
	    pad(just_a_sec/3);	/* 1/3 second */
	    if (!g_srchahead) {
		g_art = g_curr_art;
		return AS_ASK;
	    }
	    top_article();
	    g_reread = false;
	    return AS_NORM;
	  case SRCH_SUBJDONE:
	    if (sa_in)
		return AS_SA;
	    top_article();
	    g_reread = false;
	    return AS_NORM;
	  case SRCH_NOTFOUND:
	    fputs("\n\n\n\nNot found.\n",stdout) FLUSH;
	    termdown(5);
	    g_art = g_curr_art;  /* restore to current article */
	    if (sa_in)
		return AS_SA;
	    return AS_ASK;
	  case SRCH_FOUND:
	    if (cmd == Ctl('n') || cmd == Ctl('p')) {
		g_oldsubject = true;
		g_reread = false;
	    }
	    break;
	}
	return AS_NORM;
      }
      case 'u':			/* unsubscribe from this newsgroup? */
	newline();
	printf(unsubto,ngname) FLUSH;
	termdown(1);
	g_ngptr->subscribechar = NEGCHAR;
	g_ngptr->rc->flags |= RF_RCCHANGED;
	g_newsgroup_toread--;
	return AS_CLEAN;
      case 'M':
	if (g_art <= g_lastart) {
	    delay_unmark(g_artp);
	    oneless(g_artp);
	    printf("\nArticle %ld will return.\n",(long)g_art) FLUSH;
	    termdown(2);
	}
	return AS_ASK;
      case 'm':
	if (g_art >= g_absfirst && g_art <= g_lastart) {
	    unmark_as_read(g_artp);
	    printf("\nArticle %ld marked as still unread.\n",(long)g_art) FLUSH;
	    termdown(2);
	}
	return AS_ASK;
      case 'c':			/* catch up */
	switch (ask_catchup()) {
	case 'n':
	    return AS_ASK;
	case 'u':
	    return AS_CLEAN;
	}
	g_art = g_lastart+1;
	g_artp = nullptr;
	g_forcelast = false;
	return AS_NORM;
      case 'Q':  case '`':
	s_exit_code = NG_ASK;
	return AS_CLEAN;
      case 'q':			/* go back up to newsgroup level? */
	s_exit_code = NG_NEXT;
	return AS_CLEAN;
      case 'i':
	if ((g_auto_view_inline = !g_auto_view_inline) != 0)
	    g_first_view = 0;
	printf("\nAuto-View inlined mime is %s\n", g_auto_view_inline? "on" : "off");
	termdown(2);
	break;
      case 'j':
	newline();
	if (g_art >= g_absfirst && g_art <= g_lastart)
	    mark_as_read(g_artp);
	return AS_ASK;
      case 'h':
	univ_help(UHELP_ART);
	return AS_ASK;
      case 'H':			/* help? */
	help_art();
	return AS_ASK;
      case '&':
	if (switcheroo()) /* get rest of command */
	    return AS_INP;	/* if rubbed out, try something else */
	return AS_ASK;
      case '#':
	if (verbose)
	    printf("\nThe last article is %ld.\n",(long)g_lastart) FLUSH;
	else
	    printf("\n%ld\n",(long)g_lastart) FLUSH;
	termdown(2);
	return AS_ASK;
      case '+':			/* enter selection mode */
run_the_selector:
	if (art_sel_ilock) {
	    printf("\nAlready inside article selector!\n") FLUSH;
	    termdown(2);
	    return AS_ASK;
	}
	/* modes do not mix very well, so turn off the SA mode */
	sa_in = false;
	/* turn on temporary follow */
	s_follow_temp = true;
	univ_follow_temp = true;
	art_sel_ilock = true;
	*buf = article_selector(*buf);
	art_sel_ilock = false;
	switch (*buf) {
	  case '+':
	    newline();
	    term_scrolled = tc_LINES;
	    g_term_line = tc_LINES-1;
	    return AS_ASK;
	  case 'Q':
	    s_exit_code = NG_ASK;
	    break;
	  case 'q':
	    s_exit_code = NG_NEXT;
	    break;
	  case 'N':
	    s_exit_code = NG_SELNEXT;
	    break;
	  case 'P':
	    s_exit_code = NG_SELPRIOR;
	    break;
	  case ';':
	    sa_do_selthreads = true;
	    sa_go_explicit = true;
	    return AS_SA;
	  default:
	    if (g_ngptr->toread)
		return AS_NORM;
	    break;
	}
	return AS_CLEAN;
      case '=': {		/* list subjects */
	ART_NUM oldart = g_art;
	page_start();
	article_walk(output_subject, AF_UNREAD);
	g_int_count = 0;
	g_subjline = nullptr;
	g_art = oldart;
	return AS_ASK;
      }
      case '^':
	top_article();
	g_srchahead = 0;
	return AS_NORM;
#ifdef DEBUG
      case 'D':
	printf("\nFirst article: %ld\n",(long)g_firstart) FLUSH;
	termdown(2);
	article_walk(debug_article_output, 0);
	g_int_count = 0;
	return AS_ASK;
#endif
      case 'v':
	if (g_art <= g_lastart) {
	    g_reread = true;
	    g_do_hiding = false;
	}
	return AS_NORM;
      case Ctl('r'):
	g_do_hiding = true;
	g_rotate = false;
	if (g_art <= g_lastart)
	    g_reread = true;
	else
	    g_forcelast = true;
	return AS_NORM;
      case 'x':
      case Ctl('x'):
	/* In the future the behavior of 'x' may change back to a
	 * filter-select mechanism.
	 * Currently, both keys do ROT-13 translation.
	 */
	g_rotate = true;
	if (g_art <= g_lastart)
	    g_reread = true;
	else
	    g_forcelast = true;
	return AS_NORM;
      case 'X':
	g_rotate = !g_rotate;
	/* FALL THROUGH */
      case 'l': case Ctl('l'):		/* refresh screen */
      refresh_screen:
	if (g_art <= g_lastart) {
	    g_reread = true;
	    clear();
	    g_do_fseek = true;
	    artline = g_topline;
	    if (artline < 0)
		artline = 0;
	}
	return AS_NORM;
      case Ctl('^'):
	erase_line(false);		/* erase the prompt */
#ifdef MAILCALL
	setmail(true);		/* force a mail check */
#endif
	return AS_ASK;
      case Ctl('e'):
	if (g_art <= g_lastart) {
	    if (g_artsize < 0) {
		nntp_finishbody(FB_OUTPUT);
		g_raw_artsize = nntp_artsize();
		g_artsize = g_raw_artsize-artbuf_seek+artbuf_len+g_htype[PAST_HEADER].minpos;
	    }
	    if (g_do_hiding) {
		seekartbuf(g_artsize);
		seekartbuf(artpos);
	    }
	    g_reread = true;
	    g_do_fseek = true;
	    g_topline = artline;
	    g_innerlight = artline - 1;
	    g_innersearch = g_artsize;
	    g_gline = 0;
	    g_hide_everything = 'b';
	}
	return AS_NORM;
      case 'B':				/* back up one line */
      case 'b': case Ctl('b'):		/* back up a page */
	if (g_art <= g_lastart) {
	    ART_LINE target;

	    g_reread = true;
	    clear();
	    g_do_fseek = true;
	    if (*buf == 'B')
		target = g_topline - 1;
	    else {
		target = g_topline - (tc_LINES - 2);
		if (marking && (marking_areas & BACKPAGE_MARKING)) {
		    g_highlight = g_topline;
		}
	    }
	    artline = g_topline;
	    if (artline >= 0) do {
		artline--;
	    } while(artline >= 0 && artline > target && vrdary(artline-1) >= 0);
	    g_topline = artline;
	    if (artline < 0)
		artline = 0;
	}
	return AS_NORM;
      case '!':			/* shell escape */
	if (escapade())
	    return AS_INP;
	return AS_ASK;
      case 'C':
	cancel_article();
	return AS_ASK;
      case 'Z':
      case 'z':
	supersede_article();	/* supersedes */
	return AS_ASK;
      case 'R':
      case 'r': {		/* reply? */
	reply();
	return AS_ASK;
      }
      case 'F':
      case 'f': {		/* followup command */
	followup();
	g_forcegrow = true;		/* recalculate g_lastart */
	return AS_ASK;
      }
      case Ctl('f'): {			/* forward? */
	forward();
	return AS_ASK;
      }
      case '|':
      case 'w': case 'W':
      case 's': case 'S':	/* save command */
      case 'e':			/* extract command */
	if (save_article() == SAVE_ABORT)
	    return AS_INP;
	g_int_count = 0;
	return AS_ASK;
#if 0
      case 'E':
	if (decode_fp)
	    decode_end();
	else
	    newline();
	return AS_ASK;
#endif
      case 'a':			/* attachment-view command */
	newline();
	if (view_article() == SAVE_ABORT)
	    return AS_INP;
	g_int_count = 0;
	return AS_ASK;
      case 'Y':				/* yank back M articles */
	yankback();
	top_article();			/* from the beginning */
	return AS_NORM;			/* pretend nothing happened */
#ifdef STRICTCR
      case '\n':   case '\r':
	fputs(badcr,stdout) FLUSH;
	return AS_ASK;
#endif
      case '_':
	if (!finish_dblchar())
	    return AS_INP;
	switch (buf[1] & 0177) {
	  case 'P':
	    g_art--;
	    goto check_dec_art;
	  case 'N':
	    if (g_art > g_lastart)
		g_art = g_absfirst;
	    else
		g_art++;
	    if (g_art <= g_lastart)
		g_reread = true;
	    g_srchahead = 0;
	    return AS_NORM;
	  case '+':
	    if (!g_artp)
		goto not_threaded;
	    if (g_threaded_group) {
		select_arts_thread(g_artp, 0);
		printf("\nSelected all articles in this thread.\n");
	    } else {
		select_arts_subject(g_artp, 0);
		printf("\nSelected all articles in this subject.\n");
	    }
	    termdown(2);
	    if ((g_artp = first_art(g_artp->subj)) != nullptr) {
		if (g_art == article_num(g_artp))
		    return AS_ASK;
		g_art = article_num(g_artp);
	    }
	    return AS_NORM;
	  case '-':
	    if (!g_artp)
		goto not_threaded;
	    if (sel_mode == SM_THREAD) {
		deselect_arts_thread(g_artp);
		printf("\nDeselected all articles in this thread.\n");
	    } else {
		deselect_arts_subject(g_artp);
		printf("\nDeselected all articles in this subject.\n");
	    }
	    termdown(2);
	    return AS_ASK;
	  case 'C':
	    if (!*(++g_charsubst))
		g_charsubst = g_charsets;
	    goto refresh_screen;
	  case 'a':  case 's':  case 't':  case 'T':
	    *buf = buf[1];
	    goto run_the_selector;
	  case 'm':
	    if (!g_artp)
		goto not_threaded;
	    kill_subthread(g_artp, SET_TORETURN | AFFECT_ALL);
	    return AS_NORM;
	  case 'M':
	    if (!g_artp)
		goto not_threaded;
	    kill_arts_thread(g_artp, SET_TORETURN | AFFECT_ALL);
	    return AS_NORM;
	}
	/* FALL THROUGH */
      default:
	printf("\n%s",hforhelp) FLUSH;
	termdown(2);
	settle_down();
	break;
    }
    return AS_ASK;
}

/* see if there is any mail */

#ifdef MAILCALL
void setmail(bool force)
{
    if (force)
	g_mailcount = 0;
    if (!(g_mailcount++)) {
	char* mailfile = filexp(get_val("MAILFILE",MAILFILE));
	
	if (stat(mailfile,&filestat) < 0 || !filestat.st_size
	    || filestat.st_atime > filestat.st_mtime)
	    g_mailcall = "";
	else
	    g_mailcall = get_val("MAILCALL","(Mail) ");
    }
    g_mailcount %= 5;			/* check every 5 articles */
}
#endif

void setdfltcmd()
{
    if (!g_ngptr || !g_ngptr->toread)
	dfltcmd = "npq";
    else {
#if 0
	if (multimedia_mime == true) {
	    multimedia_mime++;
	    dfltcmd = "anpq";
	} else
#endif
	if (g_srchahead)
	    dfltcmd = "^Nnpq";
	else
	    dfltcmd = "npq";
    }
}

/* Ask the user about catching-up the current group.  Returns 'y' if yes,
** 'n' or 'N' if no ('N' means we used one line when in the selector),
** or 'u' for yes with unsubscribe.  Actually performs the catchup and
** unsubscription as needed.
*/
char ask_catchup()
{
    char ch;
    bool use_one_line = (gmode == 's');
    int leave_unread = 0;

    if (!use_one_line)
	newline();
reask_catchup:
    if (verbose)
	sprintf(buf,"Mark everything in %s as read?",ngname);
    else
	sprintf(buf,"Catchup %s?",ngname);
    in_char(buf,'C',"yn#h");
    printcmd();
    if ((ch = *buf) == 'h' || ch == 'H') {
	use_one_line = false;
	if (verbose)
	    fputs("\n\
Type y or SP to mark all articles as read.\n\
Type n to leave articles marked as they are.\n\
The # means enter a number to mark all but the last # articles as read.\n\
Type u to mark everything read and unsubscribe.\n\n\
",stdout) FLUSH;
	else
	    fputs("\n\
y or SP to mark all read.\n\
n to forget it.\n\
# means enter a number to leave unread.\n\
u to mark all and unsubscribe.\n\n\
",stdout) FLUSH;
	termdown(6);
	goto reask_catchup;
    }
    if (ch == 'n' || ch == 'q') {
	if (use_one_line)
	    return 'N';
	newline();
	return 'n';
    }
    if (ch == '#') {
	use_one_line = false;
	in_char("\nEnter approx. number of articles to leave unread: ", 'C', "0");
	if ((ch = *buf) == '0')
	    ch = 'y';
    }
    if (isdigit(ch)) {
	buf[1] = FINISHCMD;
	if (!finish_command(false)) {
	    use_one_line = false;
	    newline();
	    goto reask_catchup;
	}
	else {
	    leave_unread = atoi(buf);
	    ch = 'y';
	}
    }
    if (ch != 'y' && ch != 'u') {
	use_one_line = false;
	printf("\n%s\n", hforhelp) FLUSH;
	termdown(3);
	settle_down();
	goto reask_catchup;
    }
    if (in_ng) {
	article_walk(mark_all_READ, leave_unread);
	if (leave_unread) {
	    count_subjects(CS_NORM);
	    g_ngptr->toread = (ART_UNREAD)obj_count;
	}
	else {
	    selected_count = selected_subj_cnt = selected_only = false;
	    g_ngptr->toread = 0;
	    if (g_dmcount)
		yankback();
	}
	newline();
    }
    else {
	newline();
	catch_up(g_ngptr, leave_unread, 1);
    }
    if (ch == 'u') {
	g_ngptr->subscribechar = NEGCHAR;
	g_ngptr->rc->flags |= RF_RCCHANGED;
	g_newsgroup_toread--;
	newline();
	printf(unsubto,ngname);
	printf("(If you meant to hit 'y' instead of 'u', press '-'.)\n") FLUSH;
	termdown(2);
    }
    return ch;
}

static bool count_unCACHED_article(char *ptr, int arg)
{
    ARTICLE* ap = (ARTICLE*)ptr;
    if ((ap->flags & (AF_UNREAD|AF_CACHED)) == AF_UNREAD)
	obj_count++;
    return false;
}

static bool mark_all_READ(char *ptr, int leave_unread)
{
    ARTICLE* ap = (ARTICLE*)ptr;
    if (article_num(ap) > g_lastart - leave_unread)
	return true;
    ap->flags &= ~(sel_mask|AF_UNREAD);
    return false;
}

static bool mark_all_unREAD(char *ptr, int arg)
{
    ARTICLE* ap = (ARTICLE*)ptr;
    if ((ap->flags & (AF_UNREAD|AF_EXISTS)) == AF_EXISTS) {
	ap->flags |= AF_UNREAD;		/* mark as unread */
	obj_count++;
    }
    return false;
}

bool output_subject(char *ptr, int flag)
{
    ARTICLE* ap;
    ART_NUM i;
    char tmpbuf[256];
    int len;
    char* s;

    if (g_int_count)
	return true;

    if (!g_subjline) {
	g_subjline = get_val("SUBJLINE",nullptr);
	if (!g_subjline)
	    g_subjline = "";
    }

    ap = (ARTICLE*)ptr;
    if (flag && !(ap->flags & flag))
	return false;
    i = article_num(ap);
    if ((s = fetchsubj(i,false)) != nullptr) {
	sprintf(tmpbuf,"%-5ld ", i);
	len = strlen(tmpbuf);
	if (g_subjline != "") {
	    g_art = i;
	    interp(tmpbuf + len, sizeof tmpbuf - len, g_subjline);
	}
	else
	    safecpy(tmpbuf + len, s, sizeof tmpbuf - len);
	if (mode == 'k')
	    page_line = 1;
	if (print_lines(tmpbuf, NOMARKING) != 0)
	    return true;
    }
    return false;
}

#ifdef DEBUG
static bool debug_article_output(char *ptr, int arg)
{
    register ARTICLE* ap = (ARTICLE*)ptr;
    if (g_int_count)
	return 1;
    if (article_num(ap) >= g_firstart && ap->subj) {
	printf("%5ld %c %s\n", article_num(ap),
	       (ap->flags & AF_UNREAD)? 'y' : 'n', ap->subj->str) FLUSH;
	termdown(1);
    }
    return 0;
}
#endif

char ask_memorize(char_int ch)
{
    bool thread_cmd = (ch == 'T');
    bool use_one_line = (gmode == 's');
    bool global_save = false;
    char* mode_string = (thread_cmd? "thread" : "subject");
    char* mode_phrase = (thread_cmd? "replies to this article" :
				     "this subject and all replies");
    ART_NUM art_hold = g_art;
    ARTICLE* artp_hold = g_artp;

    if (!use_one_line)
	newline();
reask_memorize:
    sprintf(cmd_buf,"%sMemorize %s command:", global_save?"Global-" : "",
	    mode_string);
    in_char(cmd_buf, 'm', thread_cmd? "+S.mJK,jcC" : "+S.mJK,jcCfg");
    printcmd();
    ch = *buf;
    if (!thread_cmd && ch == 'f') {
	mode_string = *mode_string == 'a'? "subject" : "author";
	erase_line(false);
	goto reask_memorize;
    }
    if (!thread_cmd && ch == 'g') {
	global_save = !global_save;
	erase_line(false);
	goto reask_memorize;
    }
    if (ch == 'h' || ch == 'H') {
	use_one_line = false;
	if (verbose) {
	    printf("\n\
Type + or SP to auto-select this %s (i.e. includes future articles).\n\
Type S to auto-select the current subject.\n\
Type . to auto-select %s.\n\
Type m to auto-select the current article.\n\
Type J to auto-kill (junk) this %s.\n\
Type K to auto-kill the current subject.\n\
Type , to auto-kill %s.\n\
Type j to auto-kill the current article.\n\
Type C to clear all selection/killing on %s.\n\
Type c to clear all selection/killing on this %s.\n\
Type q to abort the operation.\n\
",mode_string,mode_phrase,mode_string,mode_phrase,mode_phrase,mode_string) FLUSH;
	    if (!thread_cmd) {
		printf("\
Type f to toggle author (from-line) searching.\n\
Type g to toggle global memorization.\n") FLUSH;
		termdown(2);
	    }
	}
	else
	{
	    printf("\n\
+ or SP auto-selects this %s.\n\
S auto-selects the subject.\n\
. auto-selects %s.\n\
m auto-selects this article.\n\
J auto-kills this %s.\n\
K auto-kills the subject.\n\
, auto-kills %s.\n\
j auto-kills the current article.\n\
C clears auto-commands for %s.\n\
c clears auto-commands for this %s.\n\
q aborts.\n\
",mode_string,mode_phrase,mode_string,mode_phrase,mode_phrase,mode_string) FLUSH;
	    if (!thread_cmd) {
		printf("\
f toggles author (from) mode.\n\
g toggles global memorization.\n");
		termdown(2);
	    }
	}
	newline();
	termdown(9);
	goto reask_memorize;
    }
    if (ch == 'q') {
	if (use_one_line)
	    return 'Q';
	newline();
	return 'q';
    }
    if (!thread_cmd) {
	buf[1] = *mode_string == 'a'? 'f' : 's';
	buf[2] = global_save? 'g' : 'l';
    }
    if (ch == '+') {
	if (!thread_cmd) {
	    (void)art_search(buf, (sizeof buf), true);
	    g_art = art_hold;
	    g_artp = artp_hold;
	    ch = '.';
	} else {
	    select_arts_thread(g_artp, AUTO_SEL_THD);
	    ch = (use_one_line? '+' : '.');
	}
	if (gmode != 's') {
	    printf("\nSelection memorized.\n");
	    termdown(2);
	}
    }
    else if (ch == 'S') {
	select_arts_subject(g_artp, AUTO_SEL_SBJ);
	ch = (use_one_line? '+' : '.');
	if (gmode != 's') {
	    printf("\nSelection memorized.\n");
	    termdown(2);
	}
    }
    else if (ch == '.') {
	if (!thread_cmd) {
	    (void)art_search(buf, (sizeof buf), true);
	    g_art = art_hold;
	    g_artp = artp_hold;
	} else {
	    select_subthread(g_artp, AUTO_SEL_FOL);
	    ch = (use_one_line? '+' : '.');
	}
	if (gmode != 's') {
	    printf("\nSelection memorized.\n");
	    termdown(2);
	}
    }
    else if (ch == 'm') {
	if (g_artp) {
	    change_auto_flags(g_artp, AUTO_SEL_1);
	    ch = (use_one_line? '+' : '.');
	    if (gmode != 's') {
		printf("\nSelection memorized.\n");
		termdown(2);
	    }
	}
    }
    else if (ch == 'J') {
	if (!thread_cmd) {
	    *buf = 'K';
	    (void)art_search(buf, (sizeof buf), true);
	    g_art = art_hold;
	    g_artp = artp_hold;
	}
	else
	    kill_thread(g_artp->subj->thread,AFFECT_ALL|AUTO_KILL_THD);
	if (gmode != 's') {
	    printf("\nKill memorized.\n");
	    termdown(2);
	}
    }
    else if (ch == 'j') {
	if (g_artp) {
	    mark_as_read(g_artp);
	    change_auto_flags(g_artp, AUTO_KILL_1);
	    if (gmode != 's') {
		printf("\nKill memorized.\n");
		termdown(2);
	    }
	}
    }
    else if (ch == 'K') {
	kill_subject(g_artp->subj,AFFECT_ALL|AUTO_KILL_SBJ);
	if (gmode != 's') {
	    printf("\nKill memorized.\n");
	    termdown(2);
	}
    }
    else if (ch == ',') {
	if (!thread_cmd) {
	    (void)art_search(buf, (sizeof buf), true);
	    g_art = art_hold;
	    g_artp = artp_hold;
	}
	else
	    kill_subthread(g_artp,AFFECT_ALL|AUTO_KILL_FOL);
	if (gmode != 's') {
	    printf("\nKill memorized.\n");
	    termdown(2);
	}
    }
    else if (ch == 'C') {
	if (thread_cmd)
	    clear_thread(g_artp->subj->thread);
	else
	    clear_subject(g_artp->subj);
    }
    else if (ch == 'c') {
	clear_subthread(g_artp);
    }
#if 0
    else if (ch == 's') {
	buf[1] = FINISHCMD;
	finish_command(1);
	(void)art_search(buf, (sizeof buf), true);
	g_art = art_hold;
	g_artp = artp_hold;
    }
#endif
    else {
	use_one_line = false;
	printf("\n%s\n", hforhelp) FLUSH;
	termdown(3);
	settle_down();
	goto reask_memorize;
    }
    if (!use_one_line)
	newline();
    return ch;
}
