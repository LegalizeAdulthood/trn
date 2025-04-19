// This file Copyright 1992 by Clifford A. Adams
/* sacmd.c
 *
 * main command loop
 */

#include "config/common.h"
#include "trn/sacmd.h"

#include "trn/list.h"
#include "trn/ngdata.h" // for g_threaded_group
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/ng.h"
#include "trn/respond.h"
#include "trn/samain.h"
#include "trn/samisc.h"
#include "trn/sathread.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/scmd.h"
#include "trn/score.h"
#include "trn/sdisp.h"
#include "trn/smisc.h" // needed?
#include "trn/sorder.h"
#include "trn/spage.h"
#include "trn/terminal.h" // for LINES
#include "trn/util.h"
#include "util/util2.h"

#include <cstdio>
#include <cstring>

static char *s_sa_extract_dest{}; // use this command on an extracted file
static bool  s_sa_extract_junk{}; // junk articles after extracting them

// several basic commands are already done by s_docmd (Scan level)
// interprets command in g_buf, returning 0 to continue looping,
// a condition code (negative #s) or an art# to read.  Also responsible
// for setting refresh flags if necessary.
//
int sa_do_cmd()
{
    long    b; // misc. artnum
    int     i; // for misc. purposes

    long    a = (long)g_page_ents[g_s_ptr_page_line].ent_num;
    ArticleNum artnum = g_sa_ents[a].artnum;

    switch (*g_buf)
    {
    case '+': // enter thread selector
        if (!g_threaded_group)
        {
            s_beep();
            return 0;
        }
        g_buf[0] = '+'; // fake up command for return
        g_buf[1] = '\0';
        g_sa_art = artnum; // give it somewhere to point
        s_save_context();       // for possible later changes
        return SA_FAKE; // fake up the command.

    case 'K': // kill below a threshold
        *g_buf = ' ';                           // for finish_cmd()
        if (!s_finish_cmd("Kill below or equal score:"))
        {
            break;
        }
        // make **sure** that there is a number here
        i = std::atoi(g_buf+1);
        if (i == 0)
        {
            // it might not be a number
            char *s = g_buf + 1;
            if (*s != '0' && ((*s != '+' && *s != '-') || s[1] != '0'))
            {
                // text was not a numeric 0
                s_beep();
                return 0;                       // beep but do nothing
            }
        }
        sc_kill_threshold(i);
        g_s_refill = true;
        g_s_ref_top = true;     // refresh # of articles
        break;

    case 'D': // kill unmarked "on" page
        for (int j = 0; j <= g_s_bot_ent; j++)
        {
            // This is a difficult decision, with no obviously good behavior.
            // Do not kill threads with the first article marked, as it is probably
            // not what the user wanted.
            if (!sa_marked(g_page_ents[j].ent_num) || !g_sa_mode_fold)
            {
                (void)sa_art_cmd(g_sa_mode_fold,SA_KILL_UNMARKED,
                                 g_page_ents[j].ent_num);
            }
        }
        // consider: should it start reading?
        b = sa_read_marked_elig();
        if (b)
        {
            sa_clear_mark(b);
            return b;
        }
        g_s_ref_top = true;     // refresh # of articles
        g_s_refill = true;
        break;

    case 'J': // kill marked "on" page
        // If in "fold" mode, kill threads with the first article marked
        if (g_sa_mode_fold)
        {
            for (int j = 0; j <= g_s_bot_ent; j++)
            {
                if (sa_marked(g_page_ents[j].ent_num))
                {
                    (void) sa_art_cmd(true, SA_KILL, g_page_ents[j].ent_num);
                }
            }
        }
        else
        {
            for (int j = 0; j <= g_s_bot_ent; j++)
            {
                (void) sa_art_cmd(false, SA_KILL_MARKED, g_page_ents[j].ent_num);
            }
        }
        g_s_refill = true;
        g_s_ref_top = true;     // refresh # of articles
        break;

    case 'X': // kill unmarked (basic-eligible) in group
        *g_buf = '?';                           // for finish_cmd()
        if (!s_finish_cmd("Junk all unmarked articles"))
        {
            break;
        }
        if ((g_buf[1] != 'Y') && (g_buf[1] != 'y'))
        {
            break;
        }
        i = s_first();
        if (!i)
        {
            break;
        }
        if (!sa_basic_elig(i))
        {
            while ((i = s_next(i)) != 0 && !sa_basic_elig(i))
            {
            }
        }
        // New action: if in fold mode, will not delete an article which
        // has a thread-prior marked article.
        for (; i; i = s_next(i))
        {
            if (!sa_basic_elig(i))
            {
                continue;
            }
            if (!sa_marked(i) && !was_read(g_sa_ents[i].artnum))
            {
                if (g_sa_mode_fold)             // new semantics
                {
                    long j;
                    // make j == 0 if no prior marked articles in the thread.
                    for (j = i; j; j = sa_subj_thread_prev(j))
                    {
                        if (sa_marked(j))
                        {
                            break;
                        }
                    }
                    if (j)      // there was a marked article
                    {
                        continue;       // article selection loop
                    }
                }
                one_less_art_num(g_sa_ents[i].artnum);
            }
        }
        b = sa_read_marked_elig();
        if (b)
        {
            sa_clear_mark(b);
            return b;
        }
        g_s_refill = true;
        g_s_ref_top = true;     // refresh # of articles
        break;

    case 'c': // catchup
        s_go_bot();
        ask_catchup();
        g_s_refill = true;
        g_s_ref_all = true;
        break;

    case 'o': // toggle between score and arrival orders
        s_rub_ptr();
        if (g_sa_mode_order==SA_ORDER_ARRIVAL)
        {
            g_sa_mode_order = SA_ORDER_DESCENDING;
        }
        else
        {
            g_sa_mode_order = SA_ORDER_ARRIVAL;
        }
        if (g_sa_mode_order == SA_ORDER_DESCENDING && g_sc_delay)
        {
            g_sc_delay = false;
            sc_init(true);
        }
        if (g_sa_mode_order == SA_ORDER_DESCENDING && !g_sc_initialized) // score order
        {
            g_sa_mode_order = SA_ORDER_ARRIVAL; // nope... (maybe allow later)
        }
        // if we go into score mode, make sure score is displayed
        if (g_sa_mode_order == SA_ORDER_DESCENDING && !g_sa_mode_desc_score)
        {
            g_sa_mode_desc_score = true;
        }
        s_sort();
        s_go_top_ents();
        g_s_refill = true;
        g_s_ref_bot = true;
        break;

    case 'O': // change article sorting order
        if (g_sa_mode_order != SA_ORDER_DESCENDING)   // not in score order
        {
            s_beep();
            break;
        }
        g_score_new_first = !g_score_new_first;
        s_sort();
        s_go_top_ents();
        g_s_refill = true;
        break;

    case 'R': // rescore articles
        if (!g_sc_initialized)
        {
            break;
        }
        // clear to end of screen
        clear_rest();
        g_s_ref_all = true;     // refresh everything
        std::printf("\nRescoring articles...\n");
        sc_rescore();
        s_sort();
        s_go_top_ents();
        g_s_refill = true;
        eat_typeahead();        // stay in control.
        break;

    case Ctl('e'):            // edit scorefile for group
          // clear to end of screen
          clear_rest();
        g_s_ref_all = true;  // refresh everything
        sc_score_cmd("e"); // edit scorefile
        eat_typeahead();   // stay in control.
        break;

    case '\t':        // TAB: toggle threadcount display
        g_sa_mode_desc_thread_count = !g_sa_mode_desc_thread_count;
        g_s_ref_desc = 0;
        break;

    case 'a': // toggle author display
        g_sa_mode_desc_author = !g_sa_mode_desc_author;
        g_s_ref_desc = 0;
        break;

    case '%': // toggle article # display
        g_sa_mode_desc_art_num = !g_sa_mode_desc_art_num;
        g_s_ref_desc = 0;
        break;

    case 'U': // toggle unread/unread+read mode
        g_sa_mode_read_elig = !g_sa_mode_read_elig;
// maybe later use the flag to not do this more than once per newsgroup
        for (int j = 1; j < g_sa_num_ents; j++)
        {
            s_order_add(j);             // duplicates ignored
        }
        if (sa_eligible(s_first()) || s_next_elig(s_first()))
        {
#ifdef PENDING
            if (g_sa_mode_read_elig)
            {
                g_sc_fill_read = true;
                g_sc_fill_max = ArticleNum{g_abs_first - 1};
            }
            if (!g_sa_mode_read_elig)
            {
                g_sc_fill_read = false;
            }
#endif
            g_s_ref_top = true;
            s_rub_ptr();
            s_go_top_ents();
            g_s_refill = true;
        }
        else // quit out: no articles
        {
            return SA_QUIT;
        }
        break;

    case 'F': // Fold
        g_sa_mode_fold = !g_sa_mode_fold;
        g_s_refill = true;
        g_s_ref_top = true;
        break;

    case 'f': // follow
        g_sa_follow = !g_sa_follow;
        g_s_ref_top = true;
        break;

    case 'Z': // Zero (wipe) selections...
        for (int j = 1; j < g_sa_num_ents; j++)
        {
            sa_clear_select1(j);
        }
        g_s_ref_status = 0;
        if (!g_sa_mode_zoom)
        {
            break;
        }
        g_s_ref_all = true;     // otherwise won't be refreshed
        // if in zoom mode, turn it off...
        // FALL THROUGH

    case 'z': // zoom mode toggle
        g_sa_mode_zoom = !g_sa_mode_zoom;
        if (g_sa_unzoom_refold && !g_sa_mode_zoom)
        {
            g_sa_mode_fold = true;
        }
        // toggle mode again if no elibible articles left
        if (sa_eligible(s_first()) || s_next_elig(s_first()))
        {
            g_s_ref_top = true;
            s_go_top_ents();
            g_s_refill = true;
        }
        else
        {
            s_beep();
            g_sa_mode_zoom = !g_sa_mode_zoom;   // toggle it right back
        }
        break;

    case 'j': // junk just this article
        (void)sa_art_cmd(false,SA_KILL,a);
        // later refill the page
        g_s_refill = true;
        g_s_ref_top = true;     // refresh # of articles
        break;

    case 'n': // next art
    case ']':
        s_rub_ptr();
        if (g_s_ptr_page_line<(g_s_bot_ent)) // more on page...
        {
            g_s_ptr_page_line +=1;
        }
        else
        {
            if (!s_next_elig(g_page_ents[g_s_bot_ent].ent_num))
            {
                s_beep();
                return 0;
            }
            s_go_next_page();   // will jump to top too...
        }
        break;

    case 'k': // kill subject thread
    case ',': // might later clone TRN ','
        (void)sa_art_cmd(true,SA_KILL,a);
        g_s_refill = true;
        g_s_ref_top = true;     // refresh # of articles
        break;

    case Ctl('n'):    // follow subject forward
          i = sa_subj_thread(a);
        for (b = s_next_elig(a); b; b = s_next_elig(b))
        {
            if (i == sa_subj_thread(b))
            {
                break;
            }
        }
        if (!b)         // no next w/ same subject
        {
            s_beep();
            return 0;
        }
        for (int j = g_s_ptr_page_line+1; j <= g_s_bot_ent; j++)
        {
            if (g_page_ents[j].ent_num == b)     // art is on same page
            {
                s_rub_ptr();
                g_s_ptr_page_line = j;
                return 0;
            }
        }
        // article is not on page...
        (void)s_fill_page_forward(b);  // fill forward (*must* work)
        g_s_ptr_page_line = 0;
        break;

    case Ctl('a'):    // next article with same author...
        // good for scoring
        b = sa_wrap_next_author(a);
        // rest of code copied from next-subject case above
        if (!b || (a == b))     // no next w/ same subject
        {
            s_beep();
            return 0;
        }
        for (int j = 0; j <= g_s_bot_ent; j++)
        {
            if (g_page_ents[j].ent_num == b)     // art is on same page
            {
                s_rub_ptr();
                g_s_ptr_page_line = j;
                return 0;
            }
        }
        // article is not on page...
        (void)s_fill_page_forward(b);  // fill forward (*must* work)
        g_s_ptr_page_line = 0;
        break;

    case Ctl('p'):    // follow subject backwards
          i = sa_subj_thread(a);
        for (b = s_prev_elig(a); b; b = s_prev_elig(b))
        {
            if (i == sa_subj_thread(b))
            {
                break;
            }
        }
        if (!b)         // no next w/ same subject
        {
            s_beep();
            return 0;
        }
        for (int j = g_s_ptr_page_line-1; j >= 0; j--)
        {
            if (g_page_ents[j].ent_num == b)     // art is on same page
            {
                s_rub_ptr();
                g_s_ptr_page_line = j;
                return 0;
            }
        }
        // article is not on page...
        (void)s_fill_page_backward(b);  // fill backward (*must* work)
        g_s_ptr_page_line = g_s_bot_ent;  // go to bottom of page
        (void)s_refill_page();   // make sure page is full
        g_s_ref_all = true;             // make everything redrawn...
        break;

    case 'G': // go to article number
        *g_buf = ' ';                           // for finish_cmd()
        if (!s_finish_cmd("Goto article:"))
        {
            break;
        }
        // make **sure** that there is a number here
        i = std::atoi(g_buf+1);
        if (i == 0)
        {
            // it might not be a number
            char *s = g_buf + 1;
            if (*s != '0' && ((*s != '+' && *s != '-') || s[1] != '0'))
            {
                // text was not a numeric 0
                s_beep();
                return 0;                       // beep but do nothing
            }
        }
        g_sa_art.num = i;
        return SA_READ;                 // special code to really return

    case 'N': // next newsgroup
        return SA_NEXT;

    case 'P': // previous newsgroup
        return SA_PRIOR;

    case '`':
        return SA_QUIT_SEL;

    case 'q': // quit newsgroup
        return SA_QUIT;

    case 'Q': // start reading and quit SA mode
        g_sa_in = false;
        // FALL THROUGH

    case '\n':
    case ' ':
        b = sa_read_marked_elig();
        if (b)
        {
            sa_clear_mark(b);
            return b;
        }
        // FALL THROUGH

    case 'r': // read article...
        // everything needs to be refreshed...
        g_s_ref_all = true;
        return a;

    case 'm': // toggle mark on one article
    case 'M': // toggle mark on thread
        s_rub_ptr();
        (void)sa_art_cmd((*g_buf == 'M'),SA_MARK,a);
        if (!g_sa_mark_stay)
        {
            // go to next art on page or top of page if at bottom
            if (g_s_ptr_page_line < g_s_bot_ent)        // more on page
            {
                g_s_ptr_page_line +=1;
            }
            else
            {
                s_go_top_page();        // wrap around to top
            }
        }
        break;

    case 's': // toggle select1 on one article
    case 'S': // toggle select1 on a thread
        if (!g_sa_mode_zoom)
        {
            s_rub_ptr();
        }
        (void)sa_art_cmd((*g_buf == 'S'),SA_SELECT,a);
        // if in zoom mode, selection will remove article(s) from the
        // page, so that moving the cursor down is unnecessary
        if (!g_sa_mark_stay && !g_sa_mode_zoom)
        {
            // go to next art on page or top of page if at bottom
            if (g_s_ptr_page_line<(g_s_bot_ent))        // more on page
            {
                g_s_ptr_page_line +=1;
            }
            else
            {
                s_go_top_page();        // wrap around to top
            }
        }
        break;

    case 'e': // extract marked articles
#if 0
        if (!sa_extract_start())
        {
            break;              // aborted
        }
        if (!decode_fp)
        {
            *decode_dest = '\0';        // wipe old name
        }
        a = s_first();
        if (!s_eligible(a))
        {
            a = s_next_elig(a);
        }
        flag = false;           // have we found a marked one?
        for ( ; a; a = s_next_elig(a))
        {
            if (sa_marked(a))
            {
                flag = true;
                (void)sa_art_cmd(false,SA_EXTRACT,a);
            }
        }
        if (!flag)                      // none were marked
        {
            a = g_page_ents[g_s_ptr_page_line].entnum;
            (void)sa_art_cmd(false,SA_EXTRACT,a);
        }
        g_s_refill = true;
        g_s_ref_top = true;     // refresh # of articles
        (void)get_anything();
        eat_typeahead();
#endif
        break;

#if 0
    case 'E': // end extraction, do command on image
        g_s_ref_all = true;
        s_go_bot();
        if (decode_fp)
        {
            std::printf("\nIncomplete file: %s\n",decode_dest);
            std::printf("Continue with command? [ny]");
            std::fflush(stdout);
            getcmd(g_buf);
            std::printf("\n");
            if (*g_buf == 'n' || *g_buf == ' ' || *g_buf == '\n')
            {
                break;
            }
            std::printf("Remove this file? [ny]");
            std::fflush(stdout);
            getcmd(g_buf);
            std::printf("\n");
            if (*g_buf == 'y' || *g_buf == 'Y')
            {
                decode_end();   // will remove file
                break;
            }
            std::fclose(decode_fp);
            decode_fp = 0;
        }
        if (!sa_extracted_use)
        {
            sa_extracted_use = safe_malloc(LINE_BUF_LEN);
// later consider a variable for the default command
            *sa_extracted_use = '\0';
        }
        if (!*decode_dest)
        {
            std::printf("\nTrn doesn't remember an extracted file name.\n");
            *g_buf = ' ';
            if (!s_finish_cmd("Please enter a file to use:"))
            {
                break;
            }
            if (!g_buf[1])      // user just typed return
            {
                break;
            }
            safecpy(decode_dest,g_buf+1,MAXFILENAME);
            std::printf("\n");
        }
        if (s_sa_extract_dest == nullptr)
        {
            s_sa_extract_dest = (char*)safe_malloc(LINE_BUF_LEN);
            safecpy(s_sa_extract_dest,filexp("%p"),LINE_BUF_LEN);
        }
        if (*decode_dest != '/' && *decode_dest != '~' && *decode_dest != '%')
        {
            std::sprintf(g_buf,"%s/%s",s_sa_extract_dest,decode_dest);
            safecpy(decode_dest,g_buf,MAXFILENAME);
        }
        if (*sa_extracted_use)
        {
            std::printf("Use command (default %s):\n",sa_extracted_use);
        }
        else
        {
            std::printf("Use command (no default):\n");
        }
        *g_buf = ':';                   // cosmetic
        if (!s_finish_cmd(nullptr))
        {
            break;      // command rubbed out
        }
        if (g_buf[1] != '\0')           // typed in a command
        {
            safecpy(sa_extracted_use,g_buf+1,LINE_BUF_LEN);
        }
        if (*sa_extracted_use == '\0')  // no command
        {
            break;
        }
        std::sprintf(g_buf,"!%s %s",sa_extracted_use,decode_dest);
        std::printf("\n%s\n",g_buf+1);
        (void)escapade();
        (void)get_anything();
        eat_typeahead();
        break;
#endif

    case '"':                 // append to local SCORE file
        s_go_bot();
        g_s_ref_all = true;
        std::printf("Enter score append command or type RETURN for a menu\n");
        g_buf[0] = ':';
        g_buf[1] = FINISH_CMD;
        if (!finish_command(false))
        {
            break;
        }
        std::printf("\n");
        sa_go_art(artnum);
        sc_append(g_buf+1);
        (void)get_anything();
        eat_typeahead();
        break;

    case '\'':                        // execute scoring command
        s_go_bot();
        g_s_ref_all = true;
        std::printf("\nEnter scoring command or type RETURN for a menu\n");
        g_buf[0] = ':';
        g_buf[1] = FINISH_CMD;
        if (!finish_command(false))
        {
            break;
        }
        std::printf("\n");
        sa_go_art(artnum);
        sc_score_cmd(g_buf+1);
        g_s_ref_all = true;
        (void)get_anything();
        eat_typeahead();
        break;

    default:
        s_beep();
        return 0;
    } // switch
    return 0;
}

bool sa_extract_start()
{
    if (s_sa_extract_dest == nullptr)
    {
        s_sa_extract_dest = (char*)safe_malloc(LINE_BUF_LEN);
        safe_copy(s_sa_extract_dest,file_exp("%p"),LINE_BUF_LEN);
    }
    s_go_bot();
    std::printf("To directory (default %s)\n",s_sa_extract_dest);
    *g_buf = ':';                       // cosmetic
    if (!s_finish_cmd(nullptr))
    {
        return false;           // command rubbed out
    }
    g_s_ref_all = true;
    // if the user typed something, copy it to the destination
    if (g_buf[1] != '\0')
    {
        safe_copy(s_sa_extract_dest,file_exp(g_buf+1),LINE_BUF_LEN);
    }
    // set a mode for this later?
    std::printf("\nMark extracted articles as read? [yn]");
    std::fflush(stdout);
    get_cmd(g_buf);
    std::printf("\n");
    s_sa_extract_junk = *g_buf == 'y' || *g_buf == ' ' || *g_buf == '\n';
    return true;
}


// sa_art_cmd primitive: actually does work on an article
void sa_art_cmd_prim(SaCommand cmd, long a)
{
    ArticleNum artnum = g_sa_ents[a].artnum;
// do more onpage status refreshes when in unread+read mode?
    switch (cmd)
    {
    case SA_KILL_MARKED:
        if (sa_marked(a))
        {
            sa_clear_mark(a);
            one_less_art_num(artnum);
        }
        break;

    case SA_KILL_UNMARKED:
        if (sa_marked(a))
        {
            break;              // end case early
        }
        one_less_art_num(artnum);
        break;

    case SA_KILL:               // junk this article
        sa_clear_mark(a);       // clearing should be fast
        one_less_art_num(artnum);
        break;

    case SA_MARK:               // mark this article
        if (sa_marked(a))
        {
            sa_clear_mark(a);
        }
        else
        {
            sa_mark(a);
        }
        s_ref_status_on_page(a);
        break;

    case SA_SELECT:             // select this article
        if (sa_selected1(a))
        {
            sa_clear_select1(a);
            if (g_sa_mode_zoom)
            {
                g_s_refill = true;      // article is now ineligible
            }
        }
        else
        {
            sa_select1(a);
        }
        s_ref_status_on_page(a);
        break;

    case SA_EXTRACT:
        sa_clear_mark(a);
        g_art = artnum;
        *g_buf = 'e';           // fake up the extract command
        safe_copy(g_buf+1,s_sa_extract_dest,LINE_BUF_LEN);
        (void)save_article();
        if (s_sa_extract_junk)
        {
            one_less_art_num(artnum);
        }
        break;
    } // switch
}

// return value is unused for now, but may be later...
// note: refilling after a kill is the caller's responsibility
// int multiple;                // follow the thread?
// int cmd;             // what to do
// long a;              // article # to affect or start with
int sa_art_cmd(int multiple, SaCommand cmd, long a)
{
    sa_art_cmd_prim(cmd,a);     // do the first article
    if (!multiple)
    {
        return 0;               // no problem...
    }
    long b = a;
    while ((b = sa_subj_thread_next(b)) != 0)
    {
        // if this is basically eligible and the same subject thread#
        sa_art_cmd_prim(cmd,b);
    }
    return 0;
}

// XXX this needs a good long thinking session before re-activating
long sa_wrap_next_author(long a)
{
#ifdef UNDEF
    long b;
    char* s;
    char* s2;

    s = (char*)sa_desc_author(a,20);    // 20 characters should be enough
    for (b = s_next_elig(a); b; b = s_next_elig(b))
    {
        if (std::strstr(get_from_line(b),s))
        {
            break;      // out of the for loop
        }
    }
    if (b)      // found it
    {
        return b;
    }
    // search from first article (maybe return original art)
    b = s_first();
    if (!sa_eligible(b))
    {
        b = s_next_elig(b);
    }
    for ( ; b; b = s_next_elig(b))
    {
        if (std::strstr(get_from_line(b),s))
        {
            break;      // out of the for loop
        }
    }
    return b;
#else
    return a;           // feature is disabled
#endif
}
