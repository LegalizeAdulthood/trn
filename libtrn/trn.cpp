/* This software is copyrighted as detailed in the LICENSE file. */
/* vi: set sw=4 ts=8 ai sm noet : */

/*  trn -- threaded readnews program based on rn 4.4
 *
 *  You can request help from:  trn-users@lists.sourceforge.net
 *  Send bugs, suggestions, etc. to:  trn-workers@lists.sourceforge.net
 *
 *  Author/Maintainer of trn: trn@blorf.net (Wayne Davison)
 *  Maintainer of rn: sob@bcm.tmc.edu (Stan Barber)
 *  Original Author: lwall@sdcrdcf.UUCP (Larry Wall)
 *
 *  History:
 *      01/14/83 - rn begun
 *      04/08/83 - rn 1.0
 *      09/01/83 - rn 2.0
 *      05/01/85 - rn 4.3
 *      11/01/89 - rn/rrn integration
 *      11/25/89 - trn begun
 *      07/21/90 - trn 1.0
 *      07/04/91 - rn 4.4
 *      11/25/91 - trn 2.0
 *      07/25/93 - trn 3.0
 *      ??/??/?? - trn 4.0
 *
 *  strn -- Scan(-mode)/Scoring TRN
 *
 *  Author/Maintainer of strn: caadams@zynet.com (Clifford A. Adams)
 *
 *  Strn history:
 *      Dec.  90  - "Keyword RN" initial ideas, keyword entry prototype
 *      01/16/91  - "Scoring RN" initial design notes
 *      Late  91  - cleaned up "Semicolon mode" RN patches from Todd Day
 *      Early 92  - major additions to "STRN"
 *      Mid   93  - first strn public release (version 0.8)
 *      Sep.  94  - last beta release (version 0.9.3).
 *      Late  95  - strn code ported to trn 4.0, universal selector started
 *      May   96  - strn 1.0 release
 *
 */

#include "config/common.h"
#include "trn/trn.h"

#include "nntp/nntpclient.h"
#include "trn/List.h"
#include "trn/ngdata.h"
#include "trn/addng.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/help.h"
#include "trn/init.h"
#include "trn/kfile.h"
#include "trn/last.h"
#include "trn/ng.h"
#include "trn/ngsrch.h"
#include "trn/ngstuff.h"
#include "trn/nntp.h"
#include "trn/only.h"
#include "trn/opt.h"
#include "trn/patchlevel.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/string-algos.h"
#include "trn/sw.h"
#include "trn/terminal.h"
#include "trn/univ.h"
#include "trn/utf.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cstring>
#include <string>
#include <ctime>

std::string g_ngname;                             /* name of current newsgroup */
std::string g_ngdir;                              /* same thing in directory name form */
std::string g_patchlevel{PATCHLEVEL};             //
int         g_findlast{};                         /* -r */
bool        g_verbose{true};                      /* +t */
bool        g_use_univ_selector{};                //
bool        g_use_newsrc_selector{};              //
bool        g_use_newsgroup_selector{true};       //
int         g_use_news_selector{SELECT_INIT - 1}; //

static bool s_restore_old_newsrc{};
static bool s_go_forward{true};

static std::string getngdir(const char *ngnam);

void trn_init()
{
    g_ngdir.clear();
}

int trn_main(int argc, char *argv[])
{
    bool foundany;
    char* s;

    /* Figure out our executable's name. */
#ifdef MSDOS
    strlwr(argv[0]);
    s = std::strrchr(argv[0],'\\');
    if (s != nullptr)
    {
        *s = '/';
    }
#endif
    s = std::strrchr(argv[0],'/');
    if (s == nullptr)
    {
        s = argv[0];
    }
    else
    {
        s++;
    }
#if !THREAD_INIT
    /* Default to threaded operation if our name starts with a 't' or 's'. */
    if (*s == 't' || *s == 's')
    {
        g_use_threads = true;
    }
    else
    {
        g_use_news_selector = -1;
    }
#endif
    foundany = initialize(argc,argv);

    if (g_use_newsrc_selector)
    {
        multirc_selector();
        finalize(0);
    }

    if (find_new_groups())              /* did we add any new groups? */
    {
        foundany = true;
        g_starthere = nullptr;          /* start ng scan from the top */
    }

    if (g_maxngtodo)
    {
        g_starthere = nullptr;
    }
    else if (!foundany)                 /* nothing to do? */
    {
        if (g_verbose)
        {
            std::fputs("No unread news in subscribed-to newsgroups.  To subscribe to a new\n"
                  "newsgroup use the g<newsgroup> command.\n",
                  stdout);
            termdown(2);
        }
        g_starthere = g_last_ng;
    }

    do_multirc();

    finalize(0);
    /* NOT REACHED */
    return 0;
}

void do_multirc()
{
    bool special = false;       /* allow newsgroup with no unread news? */
    MinorMode mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;

    if (g_use_univ_selector)
    {
        univ_startup();         /* load startup file */
        char ch = universal_selector();
        if (ch != 'Q')
        {
            /* section copied from bug_out below */
            /* now write the newsrc(s) back out */
            if (!write_newsrcs(g_multirc))
            {
                s_restore_old_newsrc = true; /* TODO: ask to retry! */
            }
            if (s_restore_old_newsrc)
            {
                get_old_newsrcs(g_multirc);
            }
            finalize(0);
        }
    }

    if (g_use_newsgroup_selector)
    {
      ng_start_sel:
        switch (newsgroup_selector())
        {
        case Ctl('n'):
            use_next_multirc(g_multirc);
            end_only();
            goto ng_start_sel;

        case Ctl('p'):
            use_prev_multirc(g_multirc);
            end_only();
            goto ng_start_sel;

        case 'q':
            goto bug_out;
        }
        g_starthere = g_ngptr;
        g_use_newsgroup_selector = false;
    }

    /* loop through all unread news */
  restart:
    g_current_ng = g_first_ng;
    while (true)
    {
        bool retry = false;
        if (g_findlast > 0)
        {
            g_findlast = -1;
            g_starthere = nullptr;
            if (!g_lastngname.empty())
            {
                g_ngptr = find_ng(g_lastngname.c_str());
                if (g_ngptr == nullptr)
                {
                    g_ngptr = g_first_ng;
                }
                else
                {
                    set_ngname(g_lastngname.c_str());
                    set_toread(g_ngptr, ST_LAX);
                    if (g_ngptr->toread <= TR_NONE)
                    {
                        g_ngptr = g_first_ng;
                    }
                }
            }
        }
        else if (g_starthere)
        {
            g_ngptr = g_starthere;
            g_starthere = nullptr;
        }
        else
        {
            g_ngptr = g_first_ng;
        }
        while (true)                        /* for each newsgroup */
        {
            if (g_ngptr == nullptr)     /* after the last newsgroup? */
            {
                set_mode(GM_READ,MM_FINISH_NEWSGROUP_LIST);
                if (g_maxngtodo)
                {
                    if (retry)
                    {
                        if (g_verbose)
                        {
                            std::printf("\nRestriction %s%s still in effect.\n",
                                   g_ngtodo[0], g_maxngtodo > 1 ? ", etc." : "");
                        }
                        else
                        {
                            std::fputs("\n(\"Only\" mode.)\n",stdout);
                        }
                        termdown(2);
                    }
                    else
                    {
                        if (g_verbose)
                        {
                            std::fputs("\nNo articles under restriction.", stdout);
                        }
                        else
                        {
                            std::fputs("\nNo \"only\" articles.",stdout);
                        }
                        termdown(2);
                        end_only();     /* release the restriction */
                        std::printf("\n%s\n", g_msg);
                        termdown(2);
                        retry = true;
                    }
                }
            }
            else
            {
                bool shoe_fits; /* newsgroup matches restriction? */

                set_mode(GM_READ,MM_NEWSGROUP_LIST);
                if (g_ngptr->toread >= TR_NONE)         /* recalc toread? */
                {
                    set_ngname(g_ngptr->rcline);
                    shoe_fits = inlist(g_ngname.c_str());
                    if (shoe_fits)
                    {
                        set_toread(g_ngptr, ST_LAX);
                    }
                    if (g_paranoid)
                    {
                        g_recent_ng = g_current_ng;
                        g_current_ng = g_ngptr;
                        cleanup_newsrc(g_ngptr->rc); /* this may move newsgroups around */
                        set_ng(g_current_ng);
                    }
                }
                else
                {
                    shoe_fits = true;
                }
                if (g_ngptr->toread < (special ? TR_NONE : g_ng_min_toread) //
                    || !shoe_fits)                                          /* unwanted newsgroup? */
                {
                    if (s_go_forward)
                    {
                        g_ngptr = g_ngptr->next;
                    }
                    else
                    {
                        g_ngptr = g_ngptr->prev;
                        if (g_ngptr == nullptr)
                        {
                            g_ngptr = g_first_ng->next;
                            s_go_forward = true;
                        }
                    }
                    continue;
                }
            }
            special = false;    /* go back to normal mode */
            if (g_ngptr != g_current_ng)
            {
                g_recent_ng = g_current_ng;     /* remember previous newsgroup */
                g_current_ng = g_ngptr; /* remember current newsgroup */
            }
    reask_newsgroup:
            unflush_output();   /* disable any ^O in effect */
                if (g_ngptr == nullptr)
                {
                g_dfltcmd = retry ? "npq" : "qnp";
                if (g_verbose)
                {
                    std::printf("\n****** End of newsgroups -- what next? [%s] ",
                           g_dfltcmd.c_str());
                }
                else
                {
                    std::printf("\n**** End -- next? [%s] ", g_dfltcmd.c_str());
                }
                termdown(1);
            }
            else
            {
                g_threaded_group = (g_use_threads && !(g_ngptr->flags&NF_UNTHREADED));
                g_dfltcmd =
                    (g_use_news_selector >= 0 && g_ngptr->toread >= (ART_UNREAD) g_use_news_selector ? "+ynq" : "ynq");
                if (g_verbose)
                {
                    std::printf("\n%s %3ld unread article%s in %s -- read now? [%s] ",
                           g_threaded_group? "======" : "******",
                           (long)g_ngptr->toread, plural(g_ngptr->toread),
                           g_ngname.c_str(), g_dfltcmd.c_str());
                }
                else
                {
                    std::printf("\n%s %3ld in %s -- read? [%s] ",
                           g_threaded_group? "====" : "****",
                           (long)g_ngptr->toread,g_ngname.c_str(),g_dfltcmd.c_str());
                }
                termdown(1);
            }
            std::fflush(stdout);
    reinp_newsgroup:
            if (special || (g_ngptr && g_ngptr->toread > 0))
            {
                retry = true;
            }
            switch (input_newsgroup())
            {
            case ING_ASK:
                goto reask_newsgroup;

            case ING_INPUT:
            case ING_ERASE:
                goto reinp_newsgroup;

            case ING_ERROR:
                std::printf("\n%s",g_hforhelp);
                termdown(2);
                settle_down();
                goto reask_newsgroup;

            case ING_QUIT:
                goto bug_out;

            case ING_BREAK:
                goto loop_break;

            case ING_RESTART:
                goto restart;

            case ING_NOSERVER:
                if (g_multirc)
                {
                    goto restart;
                }
                goto bug_out;

            case ING_SPECIAL:
                special = true;
                break;

            case ING_NORM:
                break;

            case ING_DISPLAY:
                newline();
                break;

            case ING_MESSAGE:
                std::printf("\n%s\n", g_msg);
                termdown(2);
                break;
            }
        }
    loop_break:;
        check_active_refetch(false);
    }

bug_out:
    /* now write the newsrc(s) back out */
    if (!write_newsrcs(g_multirc))
    {
        s_restore_old_newsrc = true; /* TODO: ask to retry! */
    }
    if (s_restore_old_newsrc)
    {
        get_old_newsrcs(g_multirc);
    }

    set_mode(gmode_save,mode_save);
}

InputNewsgroupResult input_newsgroup()
{
    char* s;

    eat_typeahead();
    getcmd(g_buf);
    if (errno || *g_buf == '\f')
    {
        newline();              /* if return from stop signal */
        return ING_ASK;
    }
    g_buf[2] = *g_buf;
    setdef(g_buf,g_dfltcmd.c_str());
    printcmd();
    if (g_ngptr != nullptr)
    {
        *g_buf = g_buf[2];
    }

  do_command:
    s_go_forward = true;                /* default to forward motion */
    switch (*g_buf)
    {
    case 'P':                           /* goto previous newsgroup */
    case 'p':                           /* find previous unread newsgroup */
        if (!g_ngptr)
        {
            g_ngptr = g_last_ng;
        }
        else if (g_ngptr != g_first_ng)
        {
            g_ngptr = g_ngptr->prev;
        }
        s_go_forward = false;           /* go backward in the newsrc */
        if (*g_buf == 'P')
        {
            return ING_SPECIAL;
        }
        break;

    case '-':
        g_ngptr = g_recent_ng;          /* recall previous newsgroup */
        if (g_ngptr)
        {
            if (!get_ng(g_ngptr->rcline,GNG_NONE))
            {
                set_ng(g_current_ng);
            }
            g_addnewbydefault = ADDNEW_ASK;
        }
        return ING_SPECIAL;

    case 'x':
        newline();
        in_char("Confirm: exit and abandon newsrc changes?", MM_ADD_NEWSGROUP_PROMPT, "yn");
        newline();
        if (*g_buf != 'y')
        {
            break;
        }
        std::printf("\nThe abandoned changes are in %s.new.\n",
               multirc_name(g_multirc));
        termdown(2);
        s_restore_old_newsrc = true;
        return ING_QUIT;

    case 'q': case 'Q':       /* quit? */
        newline();
        return ING_QUIT;

    case '^':
        if (g_general_mode != GM_SELECTOR)
        {
            newline();
        }
        g_ngptr = g_first_ng;
        break;

    case 'N':                 /* goto next newsgroup */
    case 'n':                 /* find next unread newsgroup */
        if (g_ngptr == nullptr)
        {
            newline();
            return ING_BREAK;
        }
        g_ngptr = g_ngptr->next;
        if (*g_buf == 'N')
        {
            return ING_SPECIAL;
        }
        break;

    case '1':                 /* goto 1st newsgroup */
        g_ngptr = g_first_ng;
        return ING_SPECIAL;

    case '$':
        g_ngptr = nullptr;              /* go past last newsgroup */
        break;

    case 'L':
        list_newsgroups();
        return ING_ASK;

    case '/': case '?':       /* scan for newsgroup pattern */
        switch (ng_search(g_buf,true))
        {
        case NGS_ERROR:
            set_ng(g_current_ng);
            return ING_ASK;

        case NGS_ABORT:
            set_ng(g_current_ng);
            return ING_INPUT;

        case NGS_INTR:
            if (g_verbose)
            {
                std::fputs("\n(Interrupted)\n",stdout);
            }
            else
            {
                std::fputs("\n(Intr)\n",stdout);
            }
            termdown(2);
            set_ng(g_current_ng);
            return ING_ASK;

        case NGS_FOUND:
            return ING_SPECIAL;

        case NGS_NOTFOUND:
            if (g_verbose)
            {
                std::fputs("\n\nNot found -- use a or g to add newsgroups\n",
                      stdout);
            }
            else
            {
                std::fputs("\n\nNot found\n",stdout);
            }
            termdown(3);
            return ING_ASK;

        case NGS_DONE:
            return ING_ASK;
        }
        break;

    case 'm':
        notincl("m");
        break;

    case 'g': /* goto named newsgroup */
        if (!finish_command(false))
        {
            return ING_INPUT;
        }
        s = skip_eq(g_buf+1, ' '); /* skip leading spaces */
        if (!*s && *g_buf == 'm' && !g_ngname.empty() && g_ngptr)
        {
            std::strcpy(s,g_ngname.c_str());
        }
        {
            char* _s = skip_digits(s);
            if (*_s)
            {
                /* found non-digit before hitting end */
                set_ngname(s);
            }
            else
            {
                int rcnum = std::atoi(s);
                for (g_ngptr = g_first_ng; g_ngptr; g_ngptr = g_ngptr->next)
                {
                    if (g_ngptr->num == rcnum)
                    {
                        break;
                    }
                }
                if (!g_ngptr)
                {
                    g_ngptr = g_current_ng;
                    std::printf("\nOnly %d groups. Try again.\n", g_newsgroup_cnt);
                    termdown(2);
                    return ING_ASK;
                }
                set_ngname(g_ngptr->rcline);
            }
        }
        /* try to find newsgroup */
        if (!get_ng(g_ngname.c_str(), (*g_buf == 'm' ? GNG_RELOC : GNG_NONE) | GNG_FUZZY))
        {
            g_ngptr = g_current_ng;     /* if not found, go nowhere */
        }
        g_addnewbydefault = ADDNEW_ASK;
        return ING_SPECIAL;

#ifdef DEBUG
    case 'D':
        return ING_ASK;
#endif

    case '!':                 /* shell escape */
        if (escapade())         /* do command */
        {
            return ING_INPUT;
        }
        return ING_ASK;

    case Ctl('k'):            /* edit global KILL file */
        edit_kfile();
        return ING_ASK;

    case Ctl('n'):            /* next newsrc list */
        end_only();
        newline();
        use_next_multirc(g_multirc);
        goto display_multirc;

    case Ctl('p'):            /* prev newsrc list */
        end_only();
        newline();
        use_next_multirc(g_multirc);
      display_multirc:
        {
            Newsrc* rp;
            int len;
            for (rp = g_multirc->first, len = 0; rp && len < 66; rp = rp->next)
            {
                if (rp->flags & RF_ACTIVE)
                {
                    std::sprintf(g_buf+len, ", %s", rp->datasrc->name);
                    len += std::strlen(g_buf+len);
                }
            }
            if (rp)
            {
                std::strcpy(g_buf+len, ", ...");
            }
            std::printf("\nUsing newsrc group #%d: %s.\n",g_multirc->num,g_buf+2);
            termdown(3);
            return ING_RESTART;
        }

    case 'c':                 /* catch up */
        if (g_ngptr)
        {
            ask_catchup();
            if (g_ngptr->toread == TR_NONE)
            {
                g_ngptr = g_ngptr->next;
            }
        }
        break;

    case 't':
        if (!g_use_threads)
        {
            std::printf("\n\nNot running in thread mode.\n");
        }
        else if (g_ngptr && g_ngptr->toread >= TR_NONE)
        {
            bool read_unthreaded = !(g_ngptr->flags&NF_UNTHREADED);
            g_ngptr->flags ^= NF_UNTHREADED;
            std::printf("\n\n%s will be read %sthreaded.\n",
                   g_ngptr->rcline, read_unthreaded? "un" : "");
            set_toread(g_ngptr, ST_LAX);
        }
        termdown(3);
        return ING_SPECIAL;

    case 'u':                 /* unsubscribe */
        if (g_ngptr && g_ngptr->toread >= TR_NONE)  /* unsubscribable? */
        {
            newline();
            std::printf(g_unsubto,g_ngptr->rcline);
            termdown(1);
            g_ngptr->subscribechar = NEGCHAR;   /* unsubscribe it */
            g_ngptr->toread = TR_UNSUB;         /* and make line invisible */
            g_ngptr->rc->flags |= RF_RCCHANGED;
            g_ngptr = g_ngptr->next;            /* do an automatic 'n' */
            g_newsgroup_toread--;
        }
        break;

    case 'h':
        univ_help(UHELP_NG);
        return ING_ASK;

    case 'H':                 /* help */
        help_ng();
        return ING_ASK;

    case 'A':
        if (!g_ngptr)
        {
            break;
        }
reask_abandon:
        if (g_verbose)
        {
            in_char("\nAbandon changes to current newsgroup?", MM_CONFIRM_ABANDON_PROMPT, "yn");
        }
        else
        {
            in_char("\nAbandon?", MM_CONFIRM_ABANDON_PROMPT, "ynh");
        }
        printcmd();
        newline();
        if (*g_buf == 'h')
        {
            std::printf("Type y or SP to abandon the changes to this group since you started trn.\n");
            std::printf("Type n to leave the group as it is.\n");
            termdown(2);
            goto reask_abandon;
        }
        else if (*g_buf != 'y' && *g_buf != 'n' && *g_buf != 'q')
        {
            std::fputs(g_hforhelp,stdout);
            termdown(1);
            settle_down();
            goto reask_abandon;
        }
        else if (*g_buf == 'y')
        {
            abandon_ng(g_ngptr);
        }
        return ING_SPECIAL;

    case 'a':
        /* FALL THROUGH */

    case 'o':
    case 'O':
    {
        bool doscan = (*g_buf == 'a');
        if (!finish_command(true)) /* get rest of command */
        {
            return ING_INPUT;
        }
        *g_msg = '\0';
        end_only();
        if (g_buf[1])
        {
            bool minusd = in_string(g_buf+1,"-d", true) != nullptr;
            sw_list(g_buf+1);
            if (minusd)
            {
                cwd_check();
            }
            if (doscan && g_maxngtodo)
            {
                scanactive(true);
            }
            g_ng_min_toread = *g_buf == g_empty_only_char && g_maxngtodo
                          ? TR_NONE : TR_ONE;
        }
        g_ngptr = g_first_ng;   /* simulate ^ */
        if (*g_msg && !g_maxngtodo)
        {
            return ING_MESSAGE;
        }
        return ING_DISPLAY;
    }

    case '&':
        if (switcheroo())       /* get rest of command */
        {
            return ING_INPUT;   /* if rubbed out, try something else */
        }
        return ING_ASK;

    case 'l':                 /* list other newsgroups */
    {
        if (!finish_command(true)) /* get rest of command */
        {
            return ING_INPUT;   /* if rubbed out, try something else */
        }
        s = skip_eq(g_buf+1, ' '); /* skip leading spaces */
        push_only();
        if (*s)
        {
            sw_list(s);
        }
        page_start();
        scanactive(false);
        pop_only();
        return ING_ASK;
    }

    case '`':
    case '\\':
        if (g_general_mode == GM_SELECTOR)
        {
            return ING_ERASE;
        }
      ng_start_sel:
        g_use_newsgroup_selector = true;
        switch (newsgroup_selector())
        {
        case Ctl('n'):
            end_only();
            use_next_multirc(g_multirc);
            goto ng_start_sel;

        case Ctl('p'):
            end_only();
            use_prev_multirc(g_multirc);
            goto ng_start_sel;

        case 'q':
             return ING_QUIT;
        }
        g_use_newsgroup_selector = false;
        return ING_ASK;

    case ';':
    case 'U': case '+':
    case '.': case '=':
    case 'y': case 'Y': case '\t': /* do normal thing */
    case ' ': case '\r': case '\n':
        if (!g_ngptr)
        {
            std::fputs("\nNot on a newsgroup.",stdout);
            termdown(1);
            return ING_ASK;
        }
        /* *once*, the char* s was set to an illegal value
         * (it seemed to miss all the if statements below)
         * Just to be safe, make sure it is legal.
         */
        s = "";
        if (*g_buf == '.')              /* start command? */
        {
            if (!finish_command(false)) /* get rest of command */
            {
                return ING_INPUT;
            }
            s = savestr(g_buf+1);               /* do_newsgroup will free it */
        }
        else if (*g_buf == '+' || *g_buf == 'U' || *g_buf == '=' || *g_buf == ';')
        {
            *g_buf = g_lastchar; /* restore 0200 if from a macro */
            save_typeahead(g_buf+1, sizeof g_buf - 1);
            s = savestr(g_buf);
        }
        else if (*g_buf == ' ' || *g_buf == '\r' || *g_buf == '\n')
        {
            s = "";
        }
        else
        {
            s = nullptr;
        }

      redo_newsgroup:
        switch (do_newsgroup(s))
        {
        case NG_NORM:
        case NG_NEXT:
        case NG_ERROR:
            g_ngptr = g_ngptr->next;
            break;

        case NG_ASK:
            return ING_ASK;

        case NG_SELPRIOR:
            *g_buf = 'p';
            goto do_command;

        case NG_SELNEXT:
            *g_buf = 'n';
            goto do_command;

        case NG_MINUS:
            g_ngptr = g_recent_ng;      /* recall previous newsgroup */
            return ING_SPECIAL;

        case NG_NOSERVER:
            nntp_server_died(g_ngptr->rc->datasrc);
            return ING_NOSERVER;

        /* extensions */
        case NG_GO_ARTICLE:
            g_ngptr = g_ng_go_ngptr;
            s = savestr("y");           /* enter with minimal fuss */
            goto redo_newsgroup;
          /* later: possible go-to-newsgroup */
        }
        break;

    case ':':         /* execute command on selected groups */
        if (!ngsel_perform())
        {
            return ING_INPUT;
        }
        g_page_line = 1;
        newline();
        set_ng(g_current_ng);
        return ING_ASK;

    case 'v':
        newline();
        trn_version();
        return ING_ASK;

    default:
        if (*g_buf == g_erase_char || *g_buf == g_kill_char)
        {
            return ING_ERASE;
        }
        return ING_ERROR;
    }
    return ING_NORM;
}

void check_active_refetch(bool force)
{
    std::time_t now = std::time(nullptr);

    for (DataSource *dp = datasrc_first(); dp && !empty(dp->name); dp = datasrc_next(dp))
    {
        if (!all_bits(dp->flags, DF_OPEN | DF_ACTIVE))
        {
            continue;
        }
        if (dp->act_sf.fp && dp->act_sf.refetch_secs
         && (force || now - dp->act_sf.lastfetch > dp->act_sf.refetch_secs))
        {
            actfile_hash(dp);
        }
    }
}

void trn_version()
{
    page_start();
    std::sprintf(g_msg,
            "Trn version: %s.\nConfigured for "
#ifdef HAS_LOCAL_SPOOL
            "both NNTP and local news access.\n",
#else
            "NNTP (plus individual local access).\n",
#endif
            g_patchlevel.c_str());
    print_lines(g_msg, NOMARKING);

    if (g_multirc)
    {
        newline();
        std::sprintf(g_msg,"News source group #%d:\n\n", g_multirc->num);
        print_lines(g_msg, NOMARKING);
        for (Newsrc *rp = g_multirc->first; rp; rp = rp->next)
        {
            if (!(rp->flags & RF_ACTIVE))
            {
                continue;
            }
            std::sprintf(g_msg,"ID %s:\nNewsrc %s.\n",rp->datasrc->name,rp->name);
            print_lines(g_msg, NOMARKING);
            if (rp->datasrc->flags & DF_REMOTE)
            {
                std::sprintf(g_msg,"News from server %s.\n",rp->datasrc->newsid);
                print_lines(g_msg, NOMARKING);
                if (rp->datasrc->act_sf.fp)
                {
                    if (rp->datasrc->flags & DF_TMPACTFILE)
                    {
                        std::strcpy(g_msg,"Copy of remote active file");
                    }
                    else
                    {
                        std::sprintf(g_msg,"Local active file: %s",
                                rp->datasrc->extra_name);
                    }
                }
                else
                {
                    std::strcpy(g_msg,"Dynamic active file");
                }
                if (rp->datasrc->act_sf.refetch_secs)
                {
                    char* cp = secs2text(rp->datasrc->act_sf.refetch_secs);
                    if (*cp != 'n')
                    {
                        std::sprintf(g_msg+std::strlen(g_msg),
                                " (refetch%s %s)",*cp == 'm'? " if" : ":", cp);
                    }
                }
                std::strcat(g_msg,".\n");
            }
            else
            {
                std::sprintf(g_msg,"News from %s.\nLocal active file %s.\n",
                        rp->datasrc->spool_dir, rp->datasrc->newsid);
            }
            print_lines(g_msg, NOMARKING);
            if (rp->datasrc->grpdesc)
            {
                if (!rp->datasrc->desc_sf.fp && rp->datasrc->desc_sf.hp)
                {
                    std::strcpy(g_msg,"Dynamic group desc. file");
                }
                else if (rp->datasrc->flags & DF_TMPGRPDESC)
                {
                    std::strcpy(g_msg,"Copy of remote group desc. file");
                }
                else
                {
                    std::sprintf(g_msg,"Group desc. file: %s",rp->datasrc->grpdesc);
                }
                if (rp->datasrc->desc_sf.refetch_secs)
                {
                    char* cp = secs2text(rp->datasrc->desc_sf.refetch_secs);
                    if (*cp != 'n')
                    {
                        std::sprintf(g_msg+std::strlen(g_msg),
                                " (refetch%s %s)",*cp == 'm'? " if" : ":", cp);
                    }
                }
                std::strcat(g_msg,".\n");
                print_lines(g_msg, NOMARKING);
            }
            if (rp->datasrc->flags & DF_TRY_OVERVIEW)
            {
                std::sprintf(g_msg,"Overview files from %s.\n",
                        rp->datasrc->over_dir? rp->datasrc->over_dir
                                             : "the server");
                print_lines(g_msg, NOMARKING);
            }
            print_lines("\n", NOMARKING);
        }
    }

    print_lines("You can request help from:  trn-users@lists.sourceforge.net\n"
                "Send bug reports, suggestions, etc. to:  trn-workers@lists.sourceforge.net\n",
                NOMARKING);
}

void set_ngname(const char *what)
{
    if (what != nullptr)
    {
        if (g_ngname != what)
        {
            g_ngname = what;
        }
    }
    else
    {
        g_ngname.clear();
    }

    g_ngdir = getngdir(g_ngname.c_str());
}

static std::string getngdir(const char *ngnam)
{
    std::string dir{ngnam};
    for (char &c : dir)
    {
        if (c == '.')
        {
            c = '/';
        }
    }
    return dir;
}
