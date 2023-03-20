/* art.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "trn.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "ngstuff.h"
#include "cache.h"
#include "bits.h"
#include "head.h"
#include "help.h"
#include "search.h"
#include "artio.h"
#include "ng.h"
#include "final.h"
#include "artstate.h"
#include "rcstuff.h"
#include "mime.h"
#include "term.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "utf.h"
#include "sw.h"
#include "kfile.h"
#include "backpage.h"
#include "intrp.h"
#include "rthread.h"
#include "rt-select.h"
#include "rt-util.h"
#include "rt-wumpus.h"
#include "charsubst.h"
#include "color.h"
#include "art.h"

#include <time.h>

ART_LINE g_highlight{-1}; /* next line to be highlighted */
ART_LINE g_first_view{};
ART_POS g_raw_artsize{};  /* size in bytes of raw article */
ART_POS g_artsize{};      /* size in bytes of article */
char g_art_line[LBUFLEN]; /* place for article lines */
int g_gline{0};
ART_POS g_innersearch{0};  /* artpos of end of line we want to visit */
ART_LINE g_innerlight{0};  /* highlight position for g_innersearch or 0 */
char g_hide_everything{0}; /* if set, do not write page now, ...but execute char when done with page */

bool g_reread{false};          /* consider current art temporarily unread? */
bool g_do_fseek{false};        /* should we back up in article file? */
bool g_oldsubject{false};      /* not 1st art in subject thread */
ART_LINE g_topline{-1};        /* top line of current screen */
bool g_do_hiding{true};        /* hide header lines with -h? */
bool g_is_mime{false};         /* process mime in an article? */
bool g_multimedia_mime{false}; /* images/audio to see/hear? */
bool g_rotate{false};          /* has rotation been requested? */
char *g_prompt;                /* pointer to current prompt */
char *g_firstline{nullptr};    /* special first line? */
char *g_hideline{nullptr};     /* custom line hiding? */
char *g_pagestop{nullptr};     /* custom page terminator? */
COMPEX g_hide_compex;
COMPEX g_page_compex;

#define LINE_PTR(pos) (artbuf + (pos) - htype[PAST_HEADER].minpos)
#define LINE_OFFSET(ptr) ((ptr) - artbuf + htype[PAST_HEADER].minpos)

/* page_switch() return values */

#define PS_NORM 0
#define PS_ASK 1
#define PS_RAISE 2
#define PS_TOEND 3

bool special = false;		/* is next page special length? */
int slines = 0;			/* how long to make page when special */
ART_POS restart = 0;		/* if nonzero, the place where last */
				/* line left off on line split */
ART_POS alinebeg;		/* where in file current line began */
int more_prompt_col;		/* non-zero when the more prompt is indented */

ART_LINE isrchline = 0;		/* last line to display */
COMPEX gcompex;			/* in article search pattern */

bool firstpage;			/* is this the 1st page of article? */
static bool continuation{};	/* this line/header is being continued */

void art_init()
{
    init_compex(&gcompex);
}

do_article_result do_article()
{
    char* s;
    bool hide_this_line = false;	/* hidden header line? */
    bool restart_color;
    ART_LINE linenum;			/* line # on page, 1 origin */
    bool under_lining = false;		/* are we underlining a word? */
    char* bufptr = g_art_line;	/* pointer to input buffer */
    int outpos;		/* column position of output */
    static char prompt_buf[64];		/* place to hold prompt */
    bool notesfiles = false;		/* might there be notesfiles junk? */
    char oldmode = mode;
    bool outputok = true;

    if (datasrc->flags & DF_REMOTE)
	g_artsize = g_raw_artsize = nntp_artsize();
    else
    {
	if (fstat(fileno(artfp),&filestat))	/* get article file stats */
	    return DA_CLEAN;
	if (!S_ISREG(filestat.st_mode))
	    return DA_NORM;
	g_artsize = g_raw_artsize = filestat.st_size;
    }
    sprintf(prompt_buf, mousebar_cnt>3? "%%sEnd of art %ld (of %ld) %%s[%%s]"
	: "%%sEnd of article %ld (of %ld) %%s-- what next? [%%s]",
	(long)art,(long)lastart);	/* format prompt string */
    g_prompt = prompt_buf;
    int_count = 0;		/* interrupt count is 0 */
    if ((firstpage = (g_topline < 0)) != 0) {
	parseheader(art);
	mime_SetArticle();
	clear_artbuf();
	seekart(artbuf_seek = htype[PAST_HEADER].minpos);
    }
    term_scrolled = 0;

    for (;;) {			/* for each page */
	if (ThreadedGroup && max_tree_lines)
	    init_tree();	/* init tree display */
	assert(art == openart);
	if (g_do_fseek) {
	    parseheader(art);		/* make sure header is ours */
	    if (!*artbuf) {
		mime_SetArticle();
		artbuf_seek = htype[PAST_HEADER].minpos;
	    }
	    artpos = vrdary(artline);
	    if (artpos < 0)
		artpos = -artpos;	/* labs(), anyone? */
	    if (firstpage)
		artpos = (ART_POS)0;
	    if (artpos < htype[PAST_HEADER].minpos) {
		in_header = SOME_LINE;
		seekart(htype[PAST_HEADER].minpos);
		seekartbuf(htype[PAST_HEADER].minpos);
	    }
	    else {
		seekart(artbuf_seek);
		seekartbuf(artpos);
	    }
	    g_do_fseek = false;
	    restart = 0;
	}
	linenum = 1;
#if 0 /* This causes a bug (headers displayed twice sometimes when you press v then ^R) */
	if (!g_do_hiding)
	    g_is_mime = false;
#endif
	if (firstpage) {
	    if (g_firstline) {
		interp(g_art_line,sizeof g_art_line,g_firstline);
		linenum += tree_puts(g_art_line,linenum+g_topline,0);
	    } else {
		ART_NUM i;
		int selected, unseen;

		selected = (curr_artp->flags & AF_SEL);
		unseen = article_unread(art)? 1 : 0;
		sprintf(g_art_line,"%s%s #%ld",ngname,moderated,(long)art);
		if (selected_only) {
		    i = selected_count - (unseen && selected);
		    sprintf(g_art_line+strlen(g_art_line)," (%ld + %ld more)",
			    (long)i,(long)ngptr->toread - selected_count
					- (!selected && unseen));
		}
		else if ((i = (ART_NUM)(ngptr->toread - unseen)) != 0
		       || (!ThreadedGroup && dmcount)) {
		    sprintf(g_art_line+strlen(g_art_line),
			    " (%ld more)",(long)i);
		}
		if (!ThreadedGroup && dmcount)
		    sprintf(g_art_line+strlen(g_art_line)-1,
			    " + %ld Marked to return)",(long)dmcount);
		linenum += tree_puts(g_art_line,linenum+g_topline,0);
	    }
	    start_header(art);
	    forcelast = false;		/* we will have our day in court */
	    restart = 0;
	    artline = 0;		/* start counting lines */
	    artpos = 0;
	    vwtary(artline,artpos);	/* remember pos in file */
	}
	for (restart_color = true;			/* linenum already set */
	  g_innersearch? (in_header || innermore())
	   : special? (linenum < slines)
	   : (firstpage && !in_header)? (linenum < initlines)
	   : (linenum < tc_LINES);
	     linenum++) {		/* for each line on page */
	    if (int_count) {	/* exit via interrupt? */
		newline();	/* get to left margin */
		int_count = 0;	/* reset interrupt count */
		set_mode(gmode,oldmode);
		special = false;
		return DA_NORM;	/* skip out of loops */
	    }
	    if (restart) {		/* did not finish last line? */
		bufptr = LINE_PTR(restart);/* then start again here */
		restart = 0;		/* and reset the flag */
		continuation = true;
		if (restart_color && g_do_hiding && !in_header)
		    maybe_set_color(bufptr, true);
	    }
	    else if (in_header && *(bufptr = headbuf + artpos))
		continuation = *bufptr == ' ' || *bufptr == '\t';
	    else {
		if ((bufptr = readartbuf(auto_view_inline)) == nullptr) {
		    special = false;
		    if (g_innersearch)
			(void)innermore();
		    break;
		}
		if (g_do_hiding && !in_header)
		    continuation = maybe_set_color(bufptr, restart_color);
		else
		    continuation = false;
	    }
	    alinebeg = artpos;	/* remember where we began */
	    restart_color = false;
	    if (in_header) {
		hide_this_line = parseline(bufptr,g_do_hiding,hide_this_line);
		if (!in_header) {
		    linenum += finish_tree(linenum+g_topline);
		    end_header();
		    seekart(artbuf_seek);
		}
	    } else if (notesfiles && g_do_hiding && !continuation
		    && *bufptr == '#' && isupper(bufptr[1])
		    && bufptr[2] == ':' ) {
		if ((bufptr = readartbuf(auto_view_inline)) == nullptr)
		    break;
		for (s = bufptr; *s && *s != '\n' && *s != '!'; s++) ;
		if (*s != '!')
		    readartbuf(auto_view_inline);
		mime_SetArticle();
		clear_artbuf();		/* exclude notesfiles droppings */
		artbuf_seek = htype[PAST_HEADER].minpos = tellart();
		hide_this_line = true;	/* and do not print either */
		notesfiles = false;
	    }
	    if (g_hideline && !continuation && execute(&g_hide_compex,bufptr))
		hide_this_line = true;
	    if (in_header && g_do_hiding && (htype[in_header].flags & HT_MAGIC)) {
		switch (in_header) {
		  case NGS_LINE:
		    if ((s = strchr(bufptr,'\n')) != nullptr)
			*s = '\0';
		    hide_this_line = (strchr(bufptr,',') == nullptr)
			&& !strcmp(bufptr+12,ngname);
		    if (s != nullptr)
			*s = '\n';
		    break;
		  case EXPIR_LINE:
		    if (!(htype[EXPIR_LINE].flags & HT_HIDE)) {
			s = bufptr + htype[EXPIR_LINE].length + 1;
			hide_this_line = *s != ' ' || s[1] == '\n';
		    }
		    break;
		 case FROM_LINE:
		    if ((s = strchr(bufptr,'\n')) != nullptr
		     && s-bufptr < sizeof g_art_line)
			safecpy(g_art_line,bufptr,s-bufptr+1);
		    else
			safecpy(g_art_line,bufptr,sizeof g_art_line);
		    if ((s = extract_name(g_art_line+6)) != nullptr) {
			strcpy(g_art_line+6,s);
			bufptr = g_art_line;
		    }
		    break;
		  case DATE_LINE:
		    if (curr_artp->date != -1) {
			strncpy(g_art_line,bufptr,6);
			strftime(g_art_line+6, (sizeof g_art_line)-6,
				 get_val("LOCALTIMEFMT", LOCALTIMEFMT),
				 localtime(&curr_artp->date));
			bufptr = g_art_line;
		    }
		    break;
		}
	    }
	    if (in_header == SUBJ_LINE && g_do_hiding
	     && (htype[SUBJ_LINE].flags & HT_MAGIC)) { /* handle the subject */
		s = get_cached_line(artp, SUBJ_LINE, false);
		if (s && continuation) {
		    /* continuation lines were already output */
		    linenum--;
		}
		else {
		    int length = strlen(bufptr+1);
		    notesfiles = in_string(&bufptr[length-10]," - (nf", true)!=nullptr;
		    artline++;
		    if (!s)
			bufptr += (continuation? 0 : 9);
		    else
			bufptr = s;
		    /* tree_puts(, ,1) underlines subject */
		    linenum += tree_puts(bufptr,linenum+g_topline,1)-1;
		}
	    }
	    else if (hide_this_line && g_do_hiding) {   /* do not print line? */
		linenum--;			/* compensate for linenum++ */
		if (!in_header)
		    hide_this_line = false;
	    }
	    else if (in_header) {
		artline++;
		linenum += tree_puts(bufptr,linenum+g_topline,0)-1;
	    }
	    else {			/* just a normal line */
		if (outputok && erase_each_line)
		    erase_line(false);
		if (g_highlight == artline) {	/* this line to be highlit? */
		    if (marking == STANDOUT) {
#ifdef NOFIREWORKS
			if (erase_screen)
			    no_sofire();
#endif
			standout();
		    }
		    else {
#ifdef NOFIREWORKS
			if (erase_screen)
			    no_ulfire();
#endif
			underline();
			carriage_return();
		    }
		    if (*bufptr == '\n')
			putchar(' ');
		}
		outputok = !g_hide_everything; /* registerize it, hopefully */
		if (g_pagestop && !continuation && execute(&g_page_compex,bufptr))
		    linenum = 32700;
		for (outpos = 0; outpos < tc_COLS; ) { /* while line has room */
		    if (at_norm_char(bufptr)) {	    /* normal char? */
			if (*bufptr == '_') {
			    if (bufptr[1] == '\b') {
				if (outputok && !under_lining
				 && g_highlight != artline) {
				    under_lining++;
				    if (tc_UG) {
					if (bufptr != buf && bufptr[-1]==' ') {
					    outpos--;
					    backspace();
					}
				    }
				    underline();
				}
				bufptr += 2;
			    }
			}
			else {
			    if (under_lining) {
				under_lining = false;
				un_underline();
				if (tc_UG) {
				    outpos++;
				    if (*bufptr == ' ')
					goto skip_put;
				}
			    }
			}
			/* handle rot-13 if wanted */
			if (g_rotate && !in_header && isalpha(*bufptr)) {
			    if (outputok) {
				if ((*bufptr & 31) <= 13)
				    putchar(*bufptr+13);
				else
				    putchar(*bufptr-13);
			    }
			    outpos++;
			}
			else {
			    int i;
#ifdef USE_UTF_HACK
			    if (outpos + visual_width_at(bufptr) > tc_COLS) { /* will line overflow? */
				newline();
				outpos = 0;
				linenum++;
			    }
			    i = put_char_adv(&bufptr, outputok);
			    bufptr--;
#else /* !USE_UTF_HACK */
			    i = putsubstchar(*bufptr, tc_COLS - outpos, outputok);
#endif /* USE_UTF_HACK */
			    if (i < 0) {
				outpos += -i - 1;
				break;
			    }
			    outpos += i;
			}
                        if (*tc_UC && ((g_highlight == artline && marking == STANDOUT) || under_lining))
                        {
			    backspace();
			    underchar();
			}
		    skip_put:
			bufptr++;
		    }
		    else if (AT_NL(*bufptr) || !*bufptr) {    /* newline? */
			if (under_lining) {
			    under_lining = false;
			    un_underline();
			}
#ifdef DEBUG
			if (debug & DEB_INNERSRCH && outpos < tc_COLS - 6) {
			    standout();
			    printf("%4d",artline); 
			    un_standout();
			}
#endif
			if (outputok)
			    newline();
			restart = 0;
			outpos = 1000;	/* signal normal \n */
		    }
		    else if (*bufptr == '\t') {	/* tab? */
			int incpos =  8 - outpos % 8;
			if (outputok) {
			    if (tc_GT)
				putchar(*bufptr);
			    else
				while (incpos--) putchar(' ');
			}
			bufptr++;
			outpos += 8 - outpos % 8;
		    }
		    else if (*bufptr == '\f') {	/* form feed? */
			if (outpos+2 > tc_COLS)
			    break;
			if (outputok)
			    fputs("^L",stdout);
			if (bufptr == LINE_PTR(alinebeg) && g_highlight != artline)
			    linenum = 32700;
			    /* how is that for a magic number? */
			bufptr++;
			outpos += 2;
		    }
		    else {		/* other control char */
			if (dont_filter_control) {
			    if (outputok)
				putchar(*bufptr);
			    outpos++;
			}
			else if (*bufptr != '\r' || bufptr[1] != '\n') {
			    if (outpos+2 > tc_COLS)
				break;
			    if (outputok) {
				putchar('^');
				if (g_highlight == artline && *tc_UC
				 && marking == STANDOUT) {
				    backspace();
				    underchar();
				    putchar((*bufptr & 0x7F) ^ 0x40);
				    backspace();
				    underchar();
				}
				else
				    putchar((*bufptr & 0x7F) ^ 0x40);
			    }
			    outpos += 2;
			}
			bufptr++;
		    }
		    
		} /* end of column loop */

		if (outpos < 1000) {	/* did line overflow? */
		    restart = LINE_OFFSET(bufptr);/* restart here next time */
		    if (outputok) {
			if (!tc_AM || tc_XN || outpos < tc_COLS)
			    newline();
			else
			    g_term_line++;
		    }
		    if (AT_NL(*bufptr))		/* skip the newline */
			restart = 0;
		}

		/* handle normal end of output line formalities */

		if (g_highlight == artline) {
					/* were we highlighting line? */
		    if (marking == STANDOUT)
			un_standout();
		    else
			un_underline();
		    carriage_return();
		    g_highlight = -1;	/* no more we are */
		    /* in case terminal highlighted rest of line earlier */
		    /* when we did an eol with highlight turned on: */
		    if (erase_each_line)
			erase_eol();
		}
		artline++;	/* count the line just printed */
		if (artline - tc_LINES + 1 > g_topline)
			    /* did we just scroll top line off? */
		    g_topline = artline - tc_LINES + 1;
			    /* then recompute top line # */
	    }

	    /* determine actual position in file */

	    if (restart)	/* stranded somewhere in the buffer? */
		artpos += restart - alinebeg;
	    else if (in_header)
		artpos = strchr(headbuf+artpos,'\n') - headbuf + 1;
	    else
		artpos = artbuf_pos + htype[PAST_HEADER].minpos;
	    vwtary(artline,artpos);	/* remember pos in file */
	} /* end of line loop */

	g_innersearch = 0;
	if (g_hide_everything) {
	    *buf = g_hide_everything;
	    g_hide_everything = 0;
	    goto fake_command;
	}
	if (linenum >= 32700)	/* did last line have formfeed? */
	    vwtary(artline-1,-vrdary(artline-1));
				/* remember by negating pos in file */

	special = false;	/* end of page, so reset page length */
	firstpage = false;	/* and say it is not 1st time thru */
	g_highlight = -1;

	/* extra loop bombout */

	if (g_artsize < 0 && (g_raw_artsize = nntp_artsize()) >= 0)
	    g_artsize = g_raw_artsize-artbuf_seek+artbuf_len+htype[PAST_HEADER].minpos;
recheck_pager:
	if (g_do_hiding && artbuf_pos == artbuf_len) {
	    /* If we're filtering we need to figure out if any
	     * remaining text is going to vanish or not. */
	    long seekpos = artbuf_pos + htype[PAST_HEADER].minpos;
	    readartbuf(false);
	    seekartbuf(seekpos);
	}
	if (artpos == g_artsize) {/* did we just now reach EOF? */
	    color_default();
	    set_mode(gmode,oldmode);
	    return DA_NORM;	/* avoid --MORE--(100%) */
	}

/* not done with this article, so pretend we are a pager */

reask_pager:		    
	if (g_term_line >= tc_LINES) {
	    term_scrolled += g_term_line - tc_LINES + 1;
	    g_term_line = tc_LINES-1;
	}
	more_prompt_col = g_term_col;

	unflush_output();	/* disable any ^O in effect */
 	maybe_eol();
	color_default();
	if (g_artsize < 0)
	    strcpy(cmd_buf,"?");
	else
	    sprintf(cmd_buf,"%ld",(long)(artpos*100/g_artsize));
	sprintf(buf,"%s--MORE--(%s%%)",current_charsubst(),cmd_buf);
	outpos = g_term_col + strlen(buf);
	draw_mousebar(tc_COLS - (g_term_line == tc_LINES-1? outpos+5 : 0), true);
	color_string(COLOR_MORE,buf);
	fflush(stdout);
	g_term_col = outpos;
	eat_typeahead();
#ifdef DEBUG
	if (debug & DEB_CHECKPOINTING) {
	    printf("(%d %d %d)",checkcount,linenum,artline);
	    fflush(stdout);
	}
#endif
	if (checkcount >= docheckwhen && linenum == tc_LINES
	 && (artline > 40 || checkcount >= docheckwhen+10)) {
			    /* while he is reading a whole page */
			    /* in an article he is interested in */
	    checkcount = 0;
	    checkpoint_newsrcs();	/* update all newsrcs */
	    update_thread_kfile();
	}
	cache_until_key();
	if (g_artsize < 0 && (g_raw_artsize = nntp_artsize()) >= 0) {
	    g_artsize = g_raw_artsize-artbuf_seek+artbuf_len+htype[PAST_HEADER].minpos;
	    goto_xy(more_prompt_col,g_term_line);
	    goto recheck_pager;
	}
	set_mode(gmode,'p');
	getcmd(buf);
	if (errno) {
	    if (tc_LINES < 100 && !int_count)
		*buf = '\f';/* on CONT fake up refresh */
	    else {
		*buf = 'q';	/* on INTR or paper just quit */
	    }
	}
	erase_line(erase_screen && erase_each_line);

    fake_command:		/* used by g_innersearch */
	color_default();
	output_chase_phrase = true;

	/* parse and process pager command */

	if (mousebar_cnt)
	    clear_rest();
	switch (page_switch()) {
	  case PS_ASK:	/* reprompt "--MORE--..." */
	    goto reask_pager;
	  case PS_RAISE:	/* reparse on article level */
	    set_mode(gmode,oldmode);
	    return DA_RAISE;
	  case PS_TOEND:	/* fast pager loop exit */
	    set_mode(gmode,oldmode);
	    return DA_TOEND;
	  case PS_NORM:		/* display more article */
	    break;
	}
    } /* end of page loop */
}

bool maybe_set_color(const char *cp, bool backsearch)
{
    const char ch = (cp == artbuf || cp == g_art_line? 0 : cp[-1]);
    if (ch == '\001')
	color_object(COLOR_MIMEDESC, false);
    else if (ch == '\002')
	color_object(COLOR_MIMESEP, false);
    else if (ch == WRAPPED_NL) {
	if (backsearch) {
	    while (cp > artbuf && cp[-1] != '\n') cp--;
	    maybe_set_color(cp, false);
	}
	return true;
    }
    else {
	while (*cp == ' ' || *cp == '\t') cp++;
	if (strchr(">}]#!:|", *cp))
	    color_object(COLOR_CITEDTEXT, false);
	else
	    color_object(COLOR_BODYTEXT, false);
    }
    return false;
}

/* process pager commands */

int page_switch()
{
    char* s;

    switch (*buf) {
      case '!':			/* shell escape */
	escapade();
	return PS_ASK;
      case Ctl('i'): {
	ART_LINE i = artline;
	ART_POS pos;
	g_gline = 3;
	s = LINE_PTR(alinebeg);
	while (AT_NL(*s) && i >= g_topline) {
	    pos = vrdary(--i);
	    if (pos < 0)
		pos = -pos;
	    if (pos < htype[PAST_HEADER].minpos)
		break;
	    seekartbuf(pos);
	    if ((s = readartbuf(false)) == nullptr) {
		s = LINE_PTR(alinebeg);
		break;
	    }
	}
	sprintf(cmd_buf,"^[^%c\n]",*s);
	compile(&gcompex,cmd_buf,true,true);
	goto caseG;
      }
      case Ctl('g'):
	g_gline = 3;
	compile(&gcompex,"^Subject:",true,true);
	goto caseG;
      case 'g':		/* in-article search */
	if (!finish_command(false))/* get rest of command */
	    return PS_ASK;
	s = buf+1;
	if (isspace(*s)) s++;
	if ((s = compile(&gcompex,s,true,true)) != nullptr) {
			    /* compile regular expression */
	    printf("\n%s\n",s) FLUSH;
	    termdown(2);
	    return PS_ASK;
	}
	erase_line(false);	/* erase the prompt */
	/* FALL THROUGH */
      caseG:
      case 'G': {
	ART_POS start_where;
	bool success;
	char* nlptr;
	char ch;

	if (g_gline < 0 || g_gline > tc_LINES-2)
	    g_gline = tc_LINES-2;
#ifdef DEBUG
	if (debug & DEB_INNERSRCH) {
	    printf("Start here? %d  >=? %d\n",g_topline + g_gline + 1,artline)
	      FLUSH;
	    termdown(1);
	}
#endif
	if (*buf == Ctl('i') || g_topline+g_gline+1 >= artline)
	    start_where = artpos;
			/* in case we had a line wrap */
	else {
	    start_where = vrdary(g_topline+g_gline+1);
	    if (start_where < 0)
		start_where = -start_where;
	}
	if (start_where < htype[PAST_HEADER].minpos)
	    start_where = htype[PAST_HEADER].minpos;
	seekartbuf(start_where);
	g_innerlight = 0;
	g_innersearch = 0; /* assume not found */
	while ((s = readartbuf(false)) != nullptr) {
	    if ((nlptr = strchr(s,'\n')) != nullptr) {
		ch = *++nlptr;
		*nlptr = '\0';
	    }
#ifdef DEBUG
	    if (debug & DEB_INNERSRCH)
		printf("Test %s\n",s) FLUSH;
#endif
	    success = execute(&gcompex,s) != nullptr;
	    if (nlptr)
		*nlptr = ch;
	    if (success) {
		g_innersearch = artbuf_pos + htype[PAST_HEADER].minpos;
		break;
	    }
	}
	if (!g_innersearch) {
	    seekartbuf(artpos);
	    fputs("(Not found)",stdout) FLUSH;
	    g_term_col = 11;
	    return PS_ASK;
	}
#ifdef DEBUG
	if (debug & DEB_INNERSRCH) {
	    printf("On page? %ld <=? %ld\n",(long)g_innersearch,(long)artpos)
	      FLUSH;
	    termdown(1);
	}
#endif
	if (g_innersearch <= artpos) {	/* already on page? */
	    if (g_innersearch < artpos) {
		artline = g_topline+1;
		while (vrdary(artline) < g_innersearch)
		    artline++;
	    }
	    g_highlight = artline - 1;
#ifdef DEBUG
	    if (debug & DEB_INNERSRCH) {
		printf("@ %d\n",g_highlight) FLUSH;
		termdown(1);
	    }
#endif
	    g_topline = g_highlight - g_gline;
	    if (g_topline < -1)
		g_topline = -1;
	    *buf = '\f';		/* fake up a refresh */
	    g_innersearch = 0;
	    return page_switch();
	}
	else {				/* who knows how many lines it is? */
	    g_do_fseek = true;
	    g_hide_everything = '\f';
	}
	return PS_NORM;
      }
      case '\n':		/* one line down */
      case '\r':
	special = true;
	slines = 2;
	return PS_NORM;
      case 'X':
	g_rotate = !g_rotate;
	/* FALL THROUGH */
      case 'l':
      case '\f':		/* refresh screen */
      refresh_screen:
#ifdef DEBUG
	if (debug & DEB_INNERSRCH) {
	    printf("Topline = %d",g_topline) FLUSH;
	    fgets(buf, sizeof buf, stdin);
	}
#endif
	clear();
	g_do_fseek = true;
	artline = g_topline;
	if (artline < 0)
	    artline = 0;
	firstpage = (g_topline < 0);
	return PS_NORM;
      case Ctl('e'):
	if (g_artsize < 0) {
	    nntp_finishbody(FB_OUTPUT);
	    g_raw_artsize = nntp_artsize();
	    g_artsize = g_raw_artsize-artbuf_seek+artbuf_len+htype[PAST_HEADER].minpos;
	}
	if (g_do_hiding) {
	    seekartbuf(g_artsize);
	    seekartbuf(artpos);
	}
	g_topline = artline;
	g_innerlight = artline - 1;
	g_innersearch = g_artsize;
	g_gline = 0;
	g_hide_everything = 'b';
	return PS_NORM;
      case 'B':		/* one line up */
	if (g_topline < 0)
	    break;
	if (*tc_IL && *tc_HO) {
	    ART_POS pos;
	    home_cursor();
	    insert_line();
	    carriage_return();
	    pos = vrdary(g_topline-1);
	    if (pos < 0)
		pos = -pos;
	    if (pos >= htype[PAST_HEADER].minpos) {
		seekartbuf(pos);
		if ((s = readartbuf(false)) != nullptr) {
		    artpos = vrdary(g_topline);
		    if (artpos < 0)
			artpos = -artpos;
		    maybe_set_color(s, true);
		    for (pos = artpos - pos; pos-- && !AT_NL(*s); s++)
			putchar(*s);
		    color_default();
		    putchar('\n') FLUSH;
		    g_topline--;
		    artpos = vrdary(--artline);
		    if (artpos < 0)
			artpos = -artpos;
		    seekartbuf(artpos);
		    alinebeg = vrdary(artline-1);
		    if (alinebeg < 0)
			alinebeg = -alinebeg;
		    goto_xy(0,artline-g_topline);
		    erase_line(false);
		    return PS_ASK;
		}
	    }
	}
	/* FALL THROUGH */
      case 'b':
      case Ctl('b'): {	/* back up a page */
	ART_LINE target;

	if (erase_each_line)
	    home_cursor();
	else
	    clear();

	g_do_fseek = true;	/* reposition article file */
	if (*buf == 'B')
	    target = g_topline - 1;
	else {
	    target = g_topline - (tc_LINES - 2);
	    if (marking && (marking_areas & BACKPAGE_MARKING))
		g_highlight = g_topline;
	}
	artline = g_topline;
	if (artline >= 0) do {
	    artline--;
	} while(artline >= 0 && artline > target && vrdary(artline-1) >= 0);
	g_topline = artline;	/* remember top line of screen */
				/*  (line # within article file) */
	if (artline < 0)
	    artline = 0;
	firstpage = (g_topline < 0);
	return PS_NORM;
    }
      case 'H':		/* help */
	help_page();
	return PS_ASK;
      case 't':		/* output thread data */
	page_line = 1;
	entire_tree(curr_artp);
	return PS_ASK;
      case '_':
	if (!finish_dblchar())
	    return PS_ASK;
	switch (buf[1] & 0177) {
	  case 'C':
	    if (!*(++charsubst))
		charsubst = charsets;
	    goto refresh_screen;
	  default:
	    break;
	}
	goto leave_pager;
      case '\0':		/* treat break as 'n' */
	*buf = 'n';
	/* FALL THROUGH */
      case 'a': case 'A':
      case 'e':
      case 'k': case 'K': case 'J':
      case 'n': case 'N': case Ctl('n'):
		case 'F':
		case 'R':
      case 's': case 'S':
		case 'T':
      case 'u':
      case 'w':	case 'W':
      case '|':
	mark_as_read(artp);	/* mark article as read */
	/* FALL THROUGH */
      case 'U': case ',':
      case '<': case '>':
      case '[': case ']':
      case '{': case '}':
      case '(': case ')':
      case ':':
      case '+':
      case Ctl('v'):		/* verify crypto signature */
      case ';':			/* enter article scan mode */
      case '"':			/* append to local scorefile */
      case '\'':		/* score command */
      case '#':
      case '$':
      case '&':
      case '-':
      case '.':
      case '/':
      case '1': case '2': case '3': case '4': case '5':
      case '6': case '7': case '8': case '9':
      case '=':
      case '?':
      case 'c': case 'C':
#ifdef DEBUG
		case 'D':
#endif
      case 'f':		  case Ctl('f'):
      case 'h':
      case 'j':
			  case Ctl('k'):
      case 'm': case 'M':	
      case 'p': case 'P': case Ctl('p'):	
      case '`': case 'Q':
      case 'r':		  case Ctl('r'):
      case 'v':
      case 'x':		  case Ctl('x'):
		case 'Y':
      case 'z': case 'Z':
      case '^':		  case Ctl('^'):
	       case '\b': case '\177':
leave_pager:
	g_reread = false;
	if (strchr("nNpP\016\020",*buf) == nullptr
	 && strchr("wWsSe:!&|/?123456789.",*buf) != nullptr) {
	    setdfltcmd();
	    color_object(COLOR_CMD, true);
	    interpsearch(cmd_buf, sizeof cmd_buf, mailcall, buf);
	    printf(g_prompt,cmd_buf,
		   current_charsubst(),
		   dfltcmd);	/* print prompt, whatever it is */
	    color_pop();	/* of COLOR_CMD */
	    putchar(' ');
	    fflush(stdout);
	}
	return PS_RAISE;	/* and pretend we were at end */
      case 'd':		/* half page */
      case Ctl('d'):
	special = true;
	slines = tc_LINES / 2 + 1;
	/* no divide-by-zero, thank you */
	if (tc_LINES > 2 && (tc_LINES & 1) && artline % (tc_LINES-2) >= tc_LINES/2 - 1)
	    slines++;
	goto go_forward;
      case 'y':
      case ' ':	/* continue current article */
	if (erase_screen) {	/* -e? */
	    if (erase_each_line)
		home_cursor();
	    else
		clear();	/* clear screen */
	    fflush(stdout);
	}
	else {
	    special = true;
	    slines = tc_LINES;
	}
      go_forward:
          if (*LINE_PTR(alinebeg) != '\f' && (!g_pagestop || continuation || !execute(&g_page_compex, LINE_PTR(alinebeg))))
          {
	    if (!special
	     || (marking && (*buf!='d' || (marking_areas&HALFPAGE_MARKING)))) {
		restart = alinebeg;
		artline--;	 /* restart this line */
		artpos = alinebeg;
		if (special)
		    up_line();
		else
		    g_topline = artline;
		if (marking)
		    g_highlight = artline;
	    }
	    else
		slines--;
	}
	return PS_NORM;
      case 'i':
	if ((auto_view_inline = !auto_view_inline) != 0)
	    g_first_view = 0;
	printf("\nAuto-View inlined mime is %s\n", auto_view_inline? "on" : "off");
	termdown(2);
	break;
      case 'q':	/* quit this article? */
	return PS_TOEND;
      default:
	fputs(hforhelp,stdout) FLUSH;
	termdown(1);
	settle_down();
	return PS_ASK;
    }
    return PS_ASK;
}

bool innermore()
{
    if (artpos < g_innersearch) {		/* not even on page yet? */
#ifdef DEBUG
	if (debug & DEB_INNERSRCH)
	    printf("Not on page %ld < %ld\n",(long)artpos,(long)g_innersearch)
	      FLUSH;
#endif
	return true;
    }
    if (artpos == g_innersearch) {	/* just got onto page? */
	isrchline = artline;		/* remember first line after */
	if (g_innerlight)
	    g_highlight = g_innerlight;
	else
	    g_highlight = artline - 1;
#ifdef DEBUG
	if (debug & DEB_INNERSRCH) {
	    printf("There it is %ld = %ld, %d @ %d\n",(long)artpos,
		(long)g_innersearch,g_hide_everything,g_highlight) FLUSH;
	    termdown(1);
	}
#endif
	if (g_hide_everything) {		/* forced refresh? */
	    g_topline = artline - g_gline - 1;
	    if (g_topline < -1)
		g_topline = -1;
	    return false;		/* let refresh do it all */
	}
    }
#ifdef DEBUG
    if (debug & DEB_INNERSRCH) {
	printf("Not far enough? %d <? %d + %d\n",artline,isrchline,g_gline)
	  FLUSH;
	termdown(1);
    }
#endif
    if (artline < isrchline + g_gline)
	return true;
    return false;
}

/* On click:
 *    btn = 0 (left), 1 (middle), or 2 (right) + 4 if double-clicked;
 *    x = 0 to tc_COLS-1; y = 0 to tc_LINES-1;
 *    btn_clk = 0, 1, or 2 (no 4); x_clk = x; y_clk = y.
 * On release:
 *    btn = 3; x = released x; y = released y;
 *    btn_clk = click's 0, 1, or 2; x_clk = clicked x; y_clk = clicked y.
 */
void pager_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk)
{
    ARTICLE* ap;

    if (check_mousebar(btn, x,y, btn_clk, x_clk,y_clk))
	return;

    if (btn != 3)
	return;

    ap = get_tree_artp(x_clk,y_clk+g_topline+1+term_scrolled);
    if (ap && ap != get_tree_artp(x,y+g_topline+1+term_scrolled))
	return;

    switch (btn_clk) {
      case 0:
	if (ap) {
	    if (ap == artp)
		return;
	    artp = ap;
	    art = article_num(ap);
	    g_reread = true;
	    pushchar(Ctl('r'));
	}
	else if (y > tc_LINES/2)
	    pushchar(' ');
	else if (g_topline != -1)
	    pushchar('b');
	break;
      case 1:
	if (ap) {
	    select_subthread(ap, 0);
	    special = true;
	    slines = 1;
	    pushchar(Ctl('r'));
	}
	else if (y > tc_LINES/2)
	    pushchar('\n');
	else if (g_topline != -1)
	    pushchar('B');
	break;
      case 2:
	if (ap) {
	    kill_subthread(ap, 0);
	    special = true;
	    slines = 1;
	    pushchar(Ctl('r'));
	}
	else if (y > tc_LINES/2)
	    pushchar('n');
	else
	    pushchar(Ctl('r'));
	break;
    }
}
