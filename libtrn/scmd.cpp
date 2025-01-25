/* This file is Copyright 1993 by Clifford A. Adams */
/* scmd.c
 *
 * Scan command loop.
 * Does some simple commands, and passes the rest to context-specific routines.
 */

#include "common.h"
#include "scmd.h"

#include "final.h"
#include "help.h"
#include "ng.h"
#include "ngstuff.h"
#include "sacmd.h" /* sa_docmd */
#include "samain.h"
#include "scan.h"
#include "sdisp.h"
#include "smisc.h"
#include "sorder.h"
#include "spage.h"
#include "string-algos.h"
#include "terminal.h"
#include "univ.h"

void s_go_bot()
{
    g_s_ref_bot = true;                 /* help uses whole screen */
    s_goxy(0,g_tc_LINES-g_s_bot_lines); /* go to bottom bar */
    erase_eol();                        /* erase to end of line */
    s_goxy(0,g_tc_LINES-g_s_bot_lines); /* go (back?) to bottom bar */
}

/* finishes a command on the bottom line... */
/* returns true if command entered, false if wiped out... */
int s_finish_cmd(const char *string)
{
    s_go_bot();
    if (string && *string) {
        printf("%s",string);
        fflush(stdout);
    }
    g_buf[1] = FINISHCMD;
    return finish_command(false);       /* do not echo newline */
}

/* returns an entry # selected, S_QUIT, or S_ERR */
int s_cmdloop()
{
    int i;

    /* initialization stuff for entry into s_cmdloop */
    g_s_ref_all = true;
    eat_typeahead();    /* no typeahead before entry */
    while(true) {
        s_refresh();
        s_place_ptr();          /* place article pointer */
        g_bos_on_stop = true;
        s_lookahead();          /* do something useful while waiting */
        getcmd(g_buf);
        g_bos_on_stop = false;
        eat_typeahead();        /* stay in control. */
        /* check for window resizing and refresh */
        /* if window is resized, refill and redraw */
        if (g_s_resized) {
            char ch = *g_buf;
            i = s_fillpage();
            if (i == -1 || i == 0)      /* can't fillpage */
                return S_QUIT;
            *g_buf = Ctl('l');
            (void)s_docmd();
            *g_buf = ch;
            g_s_resized = false;                /* dealt with */
        }
        i = s_docmd();
        if (i == S_NOTFOUND) {  /* command not in common set */
            switch (g_s_cur_type) {
              case S_ART:
                i = sa_docmd();
                break;
              default:
                i = 0;  /* just keep looping */
                break;
            }
        }
        if (i != 0)     /* either an entry # or a return code */
            return i;
        if (g_s_refill) {
            i = s_fillpage();
            if (i == -1 || i == 0)      /* can't fillpage */
                return S_QUIT;
        }
        /* otherwise just keep on looping... */
    }
}

void s_lookahead()
{
    switch (g_s_cur_type) {
      case S_ART:
        sa_lookahead();
        break;
      default:
        break;
    }
}

/* Do some simple, common Scan commands for any mode */
/* Interprets command in g_buf, returning 0 to continue looping or
 * a condition code (negative #s).  Responsible for setting refresh flags
 * if necessary.
 */
int s_docmd()
{
    bool flag; /* misc */

    long a = g_page_ents[g_s_ptr_page_line].entnum;
    if (*g_buf == '\f') /* map form feed to ^l */
        *g_buf = Ctl('l');
    switch(*g_buf) {
      case 'j':         /* vi mode */
        if (!g_s_mode_vi)
            return S_NOTFOUND;
        /* FALL THROUGH */
      case 'n':         /* next art */
      case ']':
        s_rub_ptr();
        if (g_s_ptr_page_line < g_s_bot_ent)    /* more on page... */
            g_s_ptr_page_line +=1;
        else {
            if (!s_next_elig(g_page_ents[g_s_bot_ent].entnum)) {
                s_beep();
                g_s_refill = true;
                break;
            }
            s_go_next_page();   /* will jump to top too... */
        }
        break;
      case 'k': /* vi mode */
        if (!g_s_mode_vi)
            return S_NOTFOUND;
        /* FALL THROUGH */
      case 'p': /* previous art */
      case '[':
        s_rub_ptr();
        if (g_s_ptr_page_line > 0)      /* more on page... */
            g_s_ptr_page_line = g_s_ptr_page_line - 1;
        else {
            if (s_prev_elig(g_page_ents[0].entnum)) {
                s_go_prev_page();
                g_s_ptr_page_line = g_s_bot_ent; /* go to page bot. */
            } else {
                g_s_refill = true;
                s_beep();
            }
        }
        break;
      case 't': /* top of page */
        s_rub_ptr();
        s_go_top_page();
        break;
      case 'b': /* bottom of page */
        s_rub_ptr();
        s_go_bot_page();
        break;
      case '>': /* next page */
        s_rub_ptr();
        a = s_next_elig(g_page_ents[g_s_bot_ent].entnum);
        if (!a) {               /* at end of articles */
            s_beep();
            break;
        }
        s_go_next_page();               /* will beep if no next page */
        break;
      case '<': /* previous page */
        s_rub_ptr();
        if (!s_prev_elig(g_page_ents[0].entnum)) {
            s_beep();
            break;
        }
        s_go_prev_page();               /* will beep if no prior page */
        break;
      case 'T':         /* top of ents */
      case '^':
        s_rub_ptr();
        flag = s_go_top_ents();
        if (!flag)              /* failure */
            return S_QUIT;
        break;
      case 'B': /* bottom of ents */
      case '$':
        s_rub_ptr();
        flag = s_go_bot_ents();
        if (!flag)
            return S_QUIT;
        break;
      case Ctl('r'):    /* refresh screen */
      case Ctl('l'):
          g_s_ref_all = true;
        break;
      case Ctl('f'):    /* refresh (mail) display */
#ifdef MAILCALL
        setmail(true);
#endif
        g_s_ref_bot = true;
        break;
      case 'h': /* universal help */
        s_go_bot();
        g_s_ref_all = true;
        univ_help(UHELP_SCANART);
        eat_typeahead();
        break;
      case 'H': /* help */
        s_go_bot();
        g_s_ref_all = true;
        /* any commands typed during help are unused. (might change) */
        switch (g_s_cur_type) {
          case S_ART:
            (void)help_scanart();
            break;
          default:
            printf("No help available for this mode (yet).\n") FLUSH;
            printf("Press any key to continue.\n");
            break;
        }
        (void)get_anything();
        eat_typeahead();
        break;
      case '!': /* shell command */
        s_go_bot();
        g_s_ref_all = true;                     /* will need refresh */
        if (!escapade())
            (void)get_anything();
        eat_typeahead();
        break;
#if 0
      case '&':         /* see/set switches... */
/* CAA 05/29/95: The new option stuff makes this potentially recursive.
 * Something similar to the 'H' (extended help) code needs to be done.
 * It may be necessary for this code to do the context saving.
 */
        s_go_bot();
        g_s_ref_all = true;                     /* will need refresh */
        if (!switcheroo())              /* XXX same semantics in trn4? */
            (void)get_anything();
        eat_typeahead();
        break;
#endif
      case '/':
      case '?':
      case 'g':         /* goto (search for) group */
        s_search();
        break;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        s_jumpnum(*g_buf);
        break;
      case '#':         /* Toggle item numbers */
        if (g_s_itemnum) {
            /* turn off item numbers */
            g_s_desc_cols += g_s_itemnum_cols;
            g_s_itemnum_cols = 0;
            g_s_itemnum = 0;
        } else {
            /* turn on item numbers */
            g_s_itemnum_cols = 3;
            g_s_desc_cols -= g_s_itemnum_cols;
            g_s_itemnum = 1;
        }
        g_s_ref_all = true;
        break;
      default:
        return S_NOTFOUND;              /* not one of the simple commands */
    } /* switch */
    return 0;           /* keep on looping! */
}

static char s_search_text[LBUFLEN]{};
static char s_search_init{};

bool s_match_description(long ent)
{
    static char lbuf[LBUFLEN];

    int lines = s_ent_lines(ent);
    for (int i = 1; i <= lines; i++) {
        strncpy(lbuf,s_get_desc(ent,i,false),LBUFLEN);
        for (char *s = lbuf; *s; s++)
            if (isupper(*s))
                *s = tolower(*s);               /* convert to lower case */
        if (strstr(lbuf,s_search_text))
            return true;
    }
    return false;
}

long s_forward_search(long ent)
{
    if (ent)
        ent = s_next_elig(ent);
    else
        ent = s_first();
    for ( ; ent; ent = s_next_elig(ent))
        if (s_match_description(ent))
            break;
    return ent;
}

long s_backward_search(long ent)
{
    if (ent)
        ent = s_prev_elig(ent);
    else
        ent = s_last();
    for ( ; ent; ent = s_prev_elig(ent))
        if (s_match_description(ent))
            break;
    return ent;
}

/* perhaps later have a wraparound search? */
void s_search()
{
    int  fill_type; /* 0: forward, 1: backward */
    char*error_msg;

    if (!s_search_init) {
        s_search_init = true;
        s_search_text[0] = '\0';
    }
    s_rub_ptr();
    g_buf[1] = '\0';
    if (!s_finish_cmd(nullptr))
        return;
    if (g_buf[1]) {     /* new text */
        /* make leading space skip an option later? */
        /* (it isn't too important because substring matching is used) */
        char *s = skip_eq(g_buf + 1, ' '); /* skip leading spaces */
        strncpy(s_search_text,s,LBUFLEN);
        for (char *t = s_search_text; *t != '\0'; t++)
            if (isupper(*t))
                *t = tolower(*t);               /* convert to lower case */
    }
    if (!*s_search_text) {
        s_beep();
        printf("\nNo previous search string.\n") FLUSH;
        (void)get_anything();
        g_s_ref_all = true;
        return;
    }
    s_go_bot();
    printf("Searching for %s",s_search_text);
    fflush(stdout);
    long ent = g_page_ents[g_s_ptr_page_line].entnum;
    switch (*g_buf) {
      case '/':
        error_msg = "No matches forward from current point.";
        ent = s_forward_search(ent);
        fill_type = 0;          /* forwards fill */
        break;
      case '?':
        error_msg = "No matches backward from current point.";
        ent = s_backward_search(ent);
        fill_type = 1;          /* backwards fill */
        break;
      case 'g':
        ent = s_forward_search(ent);
        if (!ent) {
            ent = s_forward_search(0);  /* from top */
            /* did we just loop around? */
            if (ent == g_page_ents[g_s_ptr_page_line].entnum) {
                ent = 0;
                error_msg = "No other entry matches.";
            } else
                error_msg = "No matches.";
        }
        fill_type = 0;          /* forwards fill */
        break;
      default:
        fill_type = 0;
        error_msg = "Internal error in s_search()";
        break;
    }
    if (!ent) {
        s_beep();
        printf("\n%s\n",error_msg) FLUSH;
        (void)get_anything();
        g_s_ref_all = true;
        return;
    }
    for (int i = 0; i <= g_s_bot_ent; i++)
        if (g_page_ents[i].entnum == ent) {             /* entry is on same page */
            g_s_ptr_page_line = i;
            return;
        }
    /* entry is not on page... */
    if (fill_type == 1) {
        (void)s_fillpage_backward(ent);
        s_go_bot_page();
        g_s_refill = true;
        g_s_ref_all = true;
    }
    else {
        (void)s_fillpage_forward(ent);
        s_go_top_page();
        g_s_ref_all = true;
    }
}

void s_jumpnum(char_int firstchar)
{
    bool jump_verbose = true;
    int  value = firstchar - '0';

    s_rub_ptr();
    if (jump_verbose) {
        s_go_bot();
        g_s_ref_bot = true;
        printf("Jump to item: %c",firstchar);
        fflush(stdout);
    }
    getcmd(g_buf);
    if (*g_buf == g_erase_char)
        return;
    switch (*g_buf) {
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        if (jump_verbose) {
            printf("%c",*g_buf);
            fflush(stdout);
        }
        value = value*10 + (*g_buf - '0');
        break;
      default:
        pushchar(*g_buf);
        break;
    }
    if (value == 0 || value > g_s_bot_ent+1) {
        s_beep();
        return;
    }
    g_s_ptr_page_line = value-1;
}
