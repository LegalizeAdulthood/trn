/* ng.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/ng.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artsrch.h"
#include "trn/artstate.h"
#include "trn/backpage.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/change_dir.h"
#include "trn/charsubst.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/help.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/mime.h"
#include "trn/ngstuff.h"
#include "trn/nntp.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/respond.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rt-wumpus.h"
#include "trn/rthread.h"
#include "trn/scanart.h"
#include "trn/score.h"
#include "trn/smisc.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/univ.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

ArticleNum g_art{};             // current or prospective article #
ArticleNum g_recent_art{};      // previous article # for '-' command
ArticleNum g_curr_art{};        // current article #
Article   *g_recent_artp{};     // article_ptr equivalents
Article   *g_curr_artp{};       //
Article   *g_artp{};            // the article ptr we use when art is 0
int        g_check_count{};     // how many articles have we read in the current newsgroup since the last checkpoint?
int        g_do_check_when{20}; // how often to do checkpoint
char      *g_subj_line{};       // what format to use for '='
#ifdef MAIL_CALL
int        g_mail_count{};      // check for mail when 0 mod 10
#endif
char       *g_mail_call{""};
bool        g_force_last{};             // ought we show "End of newsgroup"?
bool        g_force_grow{};             // do we want to recalculate size of newsgroup, e.g. after posting?
int         g_scan_on{};                // -S
bool        g_use_threads{THREAD_INIT}; // -x
bool        g_unsafe_rc_saves{};        // -U
std::string g_default_cmd;              // 1st char is default command

// art_switch() return values
enum ArticleSwitchResult
{
    AS_NORM = 0,
    AS_INP = 1,
    AS_ASK = 2,
    AS_CLEAN = 3,
    AS_SA = 4,
    AS_QUITNOW = 5,
    AS_SV = 6
};

static bool count_uncached_article(char *ptr, int arg);
static bool mark_all_read(char *ptr, int leave_unread);
static bool mark_all_unread(char *ptr, int arg);
#ifdef DEBUG
static bool debug_article_output(char *ptr, int arg);
#endif
static ArticleSwitchResult art_switch();

static DoNewsgroupResult s_exit_code{NG_NORM};
static bool              s_art_sel_lock{};

void ng_init()
{
    set_default_cmd();

    open_kill_file(KF_GLOBAL);
    init_compex(&g_hide_compex);
    init_compex(&g_page_compex);
}

// do newsgroup pointed to by g_ngptr with name g_ngname
//
// The basic structure is:
//  for each desired article
//      for each desired page
//          for each line on page
//              if we need another line from file
//                  get it
//                  if it's a header line
//                      do special things
//              for each column on page
//                  put out a character
//              end loop
//          end loop
//      end loop
//  end loop
//
//  (Actually, the pager is in another routine.)
//
// The chief problem is deciding what is meant by "desired".  Most of
// the messiness of this routine is due to the fact that people want
// to do unstructured things all the time.  I have used a few judicious
// goto's where I thought it improved readability.  The rest of the messiness
// arises from trying to be both space and time efficient.  Have fun.
//

// start_command command to fake up first
DoNewsgroupResult do_newsgroup(char *start_command)
{
    MinorMode mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;
    const char*whatnext = "%s%sWhat next? [%s]";
    bool ng_virtual = false;

    set_data_source(g_newsgroup_ptr->rc->data_source);

    if (change_dir(g_data_source->spool_dir))
    {
        std::printf(g_no_cd,g_data_source->spool_dir);
        return NG_ERROR;
    }

    s_exit_code = NG_NORM;
    g_kf_state &= ~(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES
                 |KFS_NORMAL_LINES  | KFS_THREAD_LINES);
    g_kill_first = ArticleNum{};

    g_extract_dest.clear();
    g_extract_prog.clear();

    // initialize the newsgroup data structures

    g_sel_rereading = false;
    g_sel_mask = AGF_SEL;
    int ret = access_newsgroup();
    if (ret == -2)
    {
        return NG_NO_SERVER;
    }
    if (ret <= 0)
    {
        return NG_ERROR;
    }

    g_search_ahead = (g_scan_on && !g_threaded_group        // did they say -S?
              && g_newsgroup_ptr->to_read >= g_scan_on ? ArticleNum{-1} : ArticleNum{});

    // FROM HERE ON, RETURN THRU CLEANUP OR WE ARE SCREWED

    g_force_last = true;                 // if 0 unread, do not bomb out
    g_recent_artp = nullptr;
    g_curr_artp = nullptr;
    g_recent_art = article_after(g_last_art);
    g_curr_art = article_after(g_last_art);
    g_prompt = whatnext;
    g_char_subst = g_charsets.c_str();

    // see if there are any special searches to do

    open_kill_file(KF_LOCAL);
    if (g_verbose)
    {
        kill_unwanted(g_first_art, "Processing memorized commands...\n\n", true);
    }
    else
    {
        kill_unwanted(g_first_art, "Auto-processing...\n\n", true);
    }

    sc_init((g_sa_never_initialized || g_sa_mode_order == SA_ORDER_DESCENDING)
            && start_command && *start_command == ';');

    if (g_univ_ng_virt_flag)
    {
        univ_newsgroup_virtual();
        goto cleanup;
    }
    if (!g_selected_count)
    {
        g_selected_only = false;
    }
    top_article();

    // do they want a special top line?

    g_first_line = get_val("FIRSTLINE",nullptr);

    // custom line suppression, custom page ending

    g_hide_line = get_val_const("HIDELINE", nullptr);
    if (g_hide_line != nullptr)
    {
        compile(&g_hide_compex, g_hide_line, true, true);
    }
    g_page_stop = get_val_const("PAGESTOP", nullptr);
    if (g_page_stop != nullptr)
    {
        compile(&g_page_compex, g_page_stop, true, true);
    }

    // now read each unread article

    // if we are going directly to an article, set things up here
    if (g_ng_go_art_num)
    {
        ng_virtual = true;
        if (g_ng_go_art_num >= g_abs_first)
        {
            g_art = g_ng_go_art_num;
            g_artp = article_ptr(g_art);
        }
        g_ng_go_art_num = ArticleNum{};
    }

    g_doing_ng = true;                  // enter the twilight zone
    g_newsgroup_ptr->rc->flags |= RF_RC_CHANGED;
    if (!g_unsafe_rc_saves)
    {
        g_check_count = 0;               // do not checkpoint for a while
    }
    g_do_fseek = false;                 // start 1st article at top
    while (g_art <= article_after(g_last_art))     // for each article
    {
        set_mode(GM_READ,MM_ARTICLE);

        // do we need to "grow" the newsgroup?

        if ((g_art > g_last_art || g_force_grow) && !g_keep_the_group_static)
        {
            ArticleNum oldlast = g_last_art;
            if (g_art_size < 0)
            {
                nntp_finish_body(FB_SILENT);
            }
            if (g_data_source->flags & DF_REMOTE)
            {
                if (g_data_source->act_sf.fp || get_newsgroup_size(g_newsgroup_ptr) > g_last_art)
                {
                    if (nntp_group(g_newsgroup_name.c_str(), g_newsgroup_ptr) <= 0)
                    {
                        s_exit_code = NG_NO_SERVER;
                        goto cleanup;
                    }
                    grow_newsgroup(g_newsgroup_ptr->ng_max);
                }
            }
            else
            {
                grow_newsgroup(get_newsgroup_size(g_newsgroup_ptr));
            }
            if (g_force_last && g_art > oldlast)
            {
                g_art = article_after(g_last_art);
            }
        }
        if (g_art != 0 || (g_artp && !(g_artp->flags & AF_TMP_MEM)))
        {
            g_artp = article_find(g_art);
        }
        if (start_command)              // do we have an initial command?
        {
            if (empty(start_command))
            {
                if (g_use_news_selector >= 0
                 && !ng_virtual
                 && g_newsgroup_ptr->to_read >= (ArticleUnread)g_use_news_selector)
                {
                    push_char('+');
                }
            }
            else
            {
                hide_pending();
                push_string(start_command, 0);
                std::free(start_command);
            }
            start_command = nullptr;
            if (input_pending())
            {
                g_art = article_after(g_last_art);
                g_curr_art = article_after(g_last_art);
                g_artp = nullptr;
                g_curr_artp = nullptr;
                goto reinp_article;
            }
        }
        if (g_art > g_last_art)                  // are we off the end still?
        {
            g_art = article_after(g_last_art);  // keep pointer references sane
            if (!g_force_last && g_newsgroup_ptr->to_read && g_selected_only && !g_selected_count)
            {
                g_art = g_curr_art;
                g_artp = g_curr_artp;
                std::strcpy(g_buf, "+");
                goto article_level;
            }
            count_subjects(CS_RETAIN);
            article_walk(count_uncached_article, 0);
            g_newsgroup_ptr->to_read = (ArticleUnread)g_obj_count.value_of();
            if (g_artp != g_curr_artp)
            {
                g_recent_art = g_curr_art;      // remember last article # (for '-')
                g_curr_art = g_art;             // set current article #
                g_recent_artp = g_curr_artp;
                g_curr_artp = g_artp;
                g_char_subst = g_charsets.c_str();
                g_first_view = ArticleLine{};
            }
            if (g_sa_in)
            {
                g_sa_go = true;
                goto article_level;
            }
            if (g_erase_screen)
            {
                clear();                        // clear the screen
            }
            else
            {
                std::fputs("\n\n",stdout);
                term_down(2);
            }
            if (g_verbose)
            {
                std::printf("End of newsgroup %s.",g_newsgroup_name.c_str());
                                        // print pseudo-article
            }
            else
            {
                std::printf("End of %s", g_newsgroup_name.c_str());
            }
            if (g_obj_count)
            {
                if (g_selected_only)
                {
                    std::printf("  (%ld + %ld articles still unread)", (long) g_selected_count,
                           g_obj_count.value_of() - g_selected_count);
                }
                else
                {
                    std::printf("  (%ld article%s still unread)", (long) g_obj_count.value_of(), plural(g_obj_count.value_of()));
                }
            }
            if (g_redirected)
            {
                if (g_redirected_to.empty())
                {
                    std::printf("\n\n** This group has been disabled by your news admin **");
                }
                else
                {
                    std::printf("\n\n** Please start using %s **", g_redirected_to.c_str());
                }
                term_down(2);
            }
            else if (!g_obj_count && !g_force_last)
            {
                goto cleanup;               // actually exit newsgroup
            }
            set_mode(g_general_mode,MM_ARTICLE_END);
            g_prompt = whatnext;
            g_search_ahead = ArticleNum{};                // no more subject search mode
            std::fputs("\n\n",stdout);
            term_down(2);
        }
        else if (!g_reread && (was_read(g_art) //
                               || (g_selected_only && !(g_artp->flags & AF_SEL))))
        {
                                        // has this article been read?
            inc_article(g_selected_only,false);// then skip it
            continue;
        }
        else if (!g_reread && (!(g_artp->flags & AF_EXISTS) || !parse_header(g_art)))
        {
            one_less(g_artp);            // mark deleted as read
            newsgroup_skip();
            continue;
        }
        else                            // we have a real live article
        {
            if (g_artp != g_curr_artp)
            {
                g_recent_art = g_curr_art;      // remember last article # (for '-')
                g_curr_art = g_art;             // set current article #
                g_recent_artp = g_curr_artp;
                g_curr_artp = g_artp;
                g_char_subst = g_charsets.c_str();
                g_first_view = ArticleLine{};
                g_do_hiding = true;
                g_rotate = false;
            }
            if (!g_do_fseek)            // starting at top of article?
            {
                g_art_line_num = ArticleLine{}; // start at the beginning
                g_top_line = ArticleLine{-1};   // and remember top line of screen
                                        // (line # within article file)
            }
            clear();                    // clear screen
            if (g_art == 0 && g_artp && g_artp->msg_id && (g_data_source->flags & DF_REMOTE) //
                && !(g_artp->flags & AF_CACHED))
            {
                g_art = nntp_stat_id(g_artp->msg_id);
                if (g_art < 0)
                {
                    s_exit_code = NG_NO_SERVER;
                    goto cleanup;
                }
                if (g_art)
                {
                    g_artp = article_find(g_art);
                }
            }
            // make sure article is found & open
            if (!art_open(g_art, (ArticlePosition) 0))
            {
                char tmpbuf[256];
                // see if we have tree data for this article anyway
                init_tree();
                std::sprintf(tmpbuf,"%s: article is not available.",g_newsgroup_name.c_str());
                if (g_artp && !(g_artp->flags & AF_CACHED))
                {
                    if (g_abs_first < g_first_cached || g_last_cached < g_last_art
                     || !g_cached_all_in_range)
                    {
                        std::sprintf(tmpbuf, "%s: article may show up in a moment.", g_newsgroup_name.c_str());
                    }
                }
                ArticleLine linenum = tree_puts(tmpbuf, ArticleLine{}, 0);
                virtual_write(g_art_line_num, ArticlePosition{});
                finish_tree(linenum);
                g_prompt = whatnext;
                g_search_ahead = ArticleNum{};
            }
            else                        // found it, so print it
            {
                switch (do_article())
                {
                case DA_CLEAN:          // quit newsgroup
                    goto cleanup;

                case DA_TO_END:          // do not mark as read
                    goto reask_article;

                case DA_RAISE:          // reparse command at end of art
                    goto article_level;

                case DA_NORM:           // normal end of article
                    break;
                }
            }
            if (g_art >= g_abs_first)    // don't mark non-existent articles
            {
                mark_as_read(g_artp);   // mark current article as read
            }
        }

// if these gotos bother you, think of this as a little state machine

reask_article:
#ifdef MAIL_CALL
        set_mail(false);
#endif
        set_default_cmd();
        if (g_erase_screen && g_erase_each_line)
        {
            erase_line(true);
        }
        if (g_term_line >= g_tc_LINES)
        {
            g_term_scrolled += g_term_line - g_tc_LINES + 1;
            g_term_line = g_tc_LINES-1;
        }
        unflush_output();               // disable any ^O in effect
        // print prompt, whatever it is
        interp(g_cmd_buf, sizeof g_cmd_buf, g_mail_call);
        std::sprintf(g_buf,g_prompt.c_str(),g_cmd_buf,
                current_char_subst(),
                g_default_cmd.c_str());
        draw_mouse_bar(g_tc_COLS - (g_term_line == g_tc_LINES-1? std::strlen(g_buf)+5 : 0), true);
        color_string(COLOR_CMD,g_buf);
        std::putchar(' ');
        std::fflush(stdout);
        g_term_col = std::strlen(g_buf) + 1;
reinp_article:
        g_reread = false;
        g_force_last = false;
        eat_typeahead();
#ifdef PENDING
        look_ahead();                   // see what we can do in advance
        cache_until_key();
#endif
        g_art = g_curr_art;
        g_artp = g_curr_artp;
        get_cmd(g_buf);
        if (errno || *g_buf == '\f')
        {
            if (g_tc_LINES < 100 && !g_int_count)
            {
                *g_buf = '\f';          // on CONT fake up refresh
            }
            else
            {
                newline();              // but only on a crt
                goto reask_article;
            }
        }
article_level:
        g_output_chase_phrase = true;  // Allow "Chasing Xrefs..." output
        if (g_mouse_bar_cnt)
        {
            clear_rest();
        }

        if (g_sa_go)
        {
            switch (sa_main())
            {
            case SA_NORM:
                continue;               // ...the article (for) loop

            case SA_NEXT:             // goto next newsgroup
                s_exit_code = NG_SEL_NEXT;
                goto cleanup;

            case SA_PRIOR:            // goto prior newsgroup
                s_exit_code = NG_SEL_PRIOR;
                goto cleanup;

            case SA_QUIT:
            case SA_ERR:
                goto cleanup;

            case SA_QUIT_SEL:
                s_exit_code = NG_ASK;
                goto cleanup;

            case SA_FAKE:
                g_last_char = g_buf[0];  // needed for fake to work
                break;                  // fall through to art_switch
            }
        }

        // parse and process article level command

        switch (art_switch())
        {
        case AS_INP:                  // multichar command rubbed out
            goto reinp_article;

        case AS_ASK:                  // reprompt "End of article..."
            goto reask_article;

        case AS_CLEAN:                // exit newsgroup
            goto cleanup;

        case AS_QUITNOW:              // just leave, cleanup already done
            goto cleanup2;

        case AS_NORM:                 // display article art
            break;

        case AS_SA:                   // go to article scan mode
            g_sa_go = true;
            goto article_level;
        }
    }                                   // end of article selection loop

// shut down newsgroup

cleanup:
    kill_unwanted(g_first_art,"\nCleaning up...\n\n",false);
                                        // do cleanup from KILL file, if any
    if (g_sa_initialized)
    {
        sa_cleanup();
    }
    if (g_sc_initialized)
    {
        sc_cleanup();
    }
    chase_xrefs(false);
    if (!g_univ_ng_virt_flag)
    {
    }

    g_in_ng = false;                      // leave newsgroup state
    art_close();
    if (!g_univ_ng_virt_flag)
    {
        newline();
    }
    deselect_all();
    yank_back();                         // do a Y command
    bits_to_rc();                       // reconstitute .newsrc line
cleanup2:
// go here if already cleaned up
    g_doing_ng = false;                 // tell sig_catcher to cool it
    // XXX later, make an option for faster/less-safe virtual groups
    if (!g_univ_ng_virt_flag && //
        !(g_univ_read_virt_flag && !(g_univ_follow || g_univ_follow_temp)))
    {
        if (!g_unsafe_rc_saves)
        {
            if (!write_newsrcs(g_multirc)) // and update .newsrc
            {
                get_anything();
            }
            update_thread_kill_file();
        }
    }

    if (g_local_kfp)
    {
        std::fclose(g_local_kfp);
        g_local_kfp = nullptr;
    }
    set_mode(gmode_save,mode_save);
    return s_exit_code;
}                                       // Whew!

// decide what to do at the end of an article

static ArticleSwitchResult art_switch()
{
    set_def(g_buf,g_default_cmd.c_str());
    print_cmd();

    g_buf[2] = '\0';
    switch (*g_buf)
    {
    case Ctl('v'):            // verify signature
        verify_sig();
        return AS_ASK;

    case ';':                 // enter ScanArticle mode
        g_sa_go_explicit = true;
        return AS_SA;

    case '"':                 // append to local SCORE file
        g_buf[0] = ':';         // enter command on next line
        g_buf[1] = FINISH_CMD;
        std::printf("\nEnter score append command or type RETURN for a menu\n");
        term_down(2);
        std::fflush(stdout);
        if (finish_command(true))       // command entered successfully
        {
            sc_append(g_buf + 1);
        }
        return AS_ASK;

    case '\'':                // execute scoring command
        g_buf[0] = ':';
        g_buf[1] = FINISH_CMD;
        std::printf("\nEnter scoring command or type RETURN for a menu\n");
        term_down(2);
        std::fflush(stdout);
        if (finish_command(true))       // command entered successfully
        {
            sc_score_cmd(g_buf + 1);
        }
        return AS_ASK;

    case '<':                 // goto previous subject/thread
        visit_prev_thread();
        return AS_NORM;

    case '>':                 // goto next subject/thread
        visit_next_thread();
        return AS_NORM;

    case 'U':                 // unread some articles
    {
        const char* u_prompt;
        const char* u_help_thread;

        if (!g_artp)
        {
            u_help_thread = "";
            if (g_verbose)
            {
                u_prompt = "\nUnkill: +select or all?";
            }
            else
            {
                u_prompt = "\nUnkill?";
            }
            g_default_cmd = "+anq";
        }
        else
        {
            if (g_verbose)
            {
                u_prompt = "\n"
                           "Unkill: +select, thread, subthread, or all?";
                u_help_thread = "Type t or SP to mark this thread's articles as unread.\n"
                                "Type s to mark the current article and its descendants as unread.\n";
            }
            else
            {
                u_prompt = "\n"
                           "Unkill?";
                u_help_thread = "t or SP to mark thread unread.\n"
                                "s to mark subthread unread.\n";
            }
            g_default_cmd = "+tsanq";
        }
reask_unread:
        in_char(u_prompt,MM_UNKILL_PROMPT,g_default_cmd.c_str());
        print_cmd();
        newline();
        if (*g_buf == 'h')
        {
            if (g_verbose)
            {
                std::fputs("Type + to enter select thread mode using all the already-read articles.\n"
                      "(The selected threads will be marked as unread and displayed as usual.)\n",
                      stdout);
                std::fputs(u_help_thread, stdout);
                std::fputs("Type a to mark all articles in this group as unread.\n"
                      "Type n or q to change nothing.\n",
                      stdout);
                term_down(6);
            }
            else
            {
                std::fputs("+ to select threads from the unread.\n",stdout);
                std::fputs(u_help_thread,stdout);
                std::fputs("a to mark all articles unread.\n"
                      "n or q to change nothing.\n",
                      stdout);
                term_down(5);
            }
            goto reask_unread;
        }
        else if (*g_buf == 'n' || *g_buf == 'q')
        {
            return AS_ASK;
        }
        else if (*g_buf == 't' && *u_help_thread)
        {
            if (g_artp->subj->thread)
            {
                unkill_thread(g_artp->subj->thread);
            }
            else
            {
                unkill_subject(g_artp->subj);
            }
            g_artp = first_art(g_artp->subj);
            if (g_artp != nullptr)
            {
                g_art = article_num(g_artp);
            }
        }
        else if (*g_buf == 's' && *u_help_thread)
        {
            unkill_sub_thread(g_artp);
        }
        else if (*g_buf == 'a')
        {
            check_first(g_abs_first);
            article_walk(mark_all_unread, 0);
            count_subjects(CS_NORM);
            g_newsgroup_ptr->to_read = (ArticleUnread)g_obj_count.value_of();
        }
        else if (*g_buf == '+')
        {
            *g_buf = 'U';
            goto run_the_selector;
        }
        else
        {
            std::fputs(g_h_for_help,stdout);
            term_down(1);
            settle_down();
            goto reask_unread;
        }
        return AS_NORM;
    }

    case '[':                 // goto parent article
    case '{':                 // goto thread's root article
          if (g_artp && g_threaded_group)
          {
              if (!find_parent(*g_buf == '{'))
              {
                const char* cp = (*g_buf=='['?"parent":"root");
                if (g_verbose)
                {
                    std::printf("\nThere is no %s article prior to this one.\n", cp);
                }
                else
                {
                    std::printf("\nNo prior %s.\n", cp);
                }
                term_down(2);
                return AS_ASK;
            }
            g_reread = true;
            g_s_follow_temp = true;
            g_univ_follow_temp = true;
            return AS_NORM;
        }
not_threaded:
        if (!g_artp)
        {
            if (g_verbose)
            {
                std::fputs("\nYou're at the end of the group.\n", stdout);
            }
            else
            {
                std::fputs("\nEnd of group.\n", stdout);
            }
            term_down(2);
            return AS_ASK;
        }
        if (g_verbose)
        {
            std::fputs("\nThis group is not threaded.\n", stdout);
        }
        else
        {
            std::fputs("\nUnthreaded group.\n", stdout);
        }
        term_down(2);
        return AS_ASK;

    case ']':                 // goto child article
    case '}':                 // goto thread's leaf article
        if (g_artp && g_threaded_group)
        {
            if (!find_leaf(*g_buf == '}'))
            {
                if (g_verbose)
                {
                    std::fputs("\n"
                          "This is the last leaf in this tree.\n",
                          stdout);
                }
                else
                {
                    std::fputs("\n"
                          "Last leaf.\n",
                          stdout);
                }
                term_down(2);
                return AS_ASK;
            }
            g_reread = true;
            g_s_follow_temp = true;
            g_univ_follow_temp = true;
            return AS_NORM;
        }
        goto not_threaded;

    case '(':                 // goto previous sibling
    case ')':                     // goto next sibling
        if (g_artp && g_threaded_group)
        {
            if (!(*g_buf == '(' ? find_prev_sib() : find_next_sib()))
            {
                const char* cp = (*g_buf == '(' ? "previous" : "next");
                if (g_verbose)
                {
                    std::printf("\nThis article has no %s sibling.\n", cp);
                }
                else
                {
                    std::printf("\nNo %s sibling.\n", cp);
                }
                term_down(2);
                return AS_ASK;
            }
            g_reread = true;
            g_s_follow_temp = true;
            g_univ_follow_temp = true;
            return AS_NORM;
        }
        goto not_threaded;

    case 'T':
        if (!g_threaded_group)
        {
            goto not_threaded;
        }
        // FALL THROUGH

    case 'A':
        if (!g_artp)
        {
            goto not_threaded;
        }
        switch (ask_memorize(*g_buf))
        {
        case ',':  case 'J': case 'K': case 'j':
            return AS_NORM;
        }
        return AS_ASK;

    case 'K':
        if (!g_artp)
        {
            goto not_threaded;
        }
        // first, write kill-subject command
        (void)art_search(g_buf, (sizeof g_buf), true);
        g_art = g_curr_art;
        g_artp = g_curr_artp;
        kill_subject(g_artp->subj,AFFECT_ALL);// take care of any prior subjects
        if (g_sa_in && !(g_sa_follow || g_s_follow_temp))
        {
            return AS_SA;
        }
        return AS_NORM;

    case ',':         // kill this node and all descendants
        if (!g_artp)
        {
            goto not_threaded;
        }
        if (g_threaded_group)
        {
            kill_sub_thread(g_artp,AFFECT_ALL);
        }
        else if (g_art >= g_abs_first && g_art <= g_last_art)
        {
            mark_as_read(g_artp);
        }
        if (g_sa_in && !(g_sa_follow || g_s_follow_temp))
        {
            return AS_SA;
        }
        return AS_NORM;

    case 'J':         // Junk all nodes in this thread
        if (!g_artp)
        {
            goto not_threaded;
        }
        if (g_threaded_group)
        {
            kill_thread(g_artp->subj->thread,AFFECT_ALL);
            if (g_sa_in)
            {
                return AS_SA;
            }
            return AS_NORM;
        }
        // FALL THROUGH

    case 'k':         // kill current subject
        if (!g_artp)
        {
            goto not_threaded;
        }
        kill_subject(g_artp->subj,AFFECT_ALL);
        if (!g_threaded_group || g_last_cached < g_last_art)
        {
            *g_buf = 'k';
            goto normal_search;
        }
        if (g_sa_in && !(g_sa_follow || g_s_follow_temp))
        {
            return AS_SA;
        }
        return AS_NORM;

    case 't':
        erase_line(g_erase_screen && g_erase_each_line);
        g_page_line = 1;
        entire_tree(g_curr_artp);
        return AS_ASK;

    case ':':                 // execute command on selected articles
        g_page_line = 1;
        if (!thread_perform())
        {
            return AS_INP;
        }
        carriage_return();
        perform_status_end(g_newsgroup_ptr->to_read, "article");
        std::fputs(g_msg, stdout);
        newline();
        g_art = g_curr_art;
        g_artp = g_curr_artp;
        return AS_ASK;

    case 'p':                 // find previous unread article
        g_s_follow_temp = true; // keep going until change req.
        g_univ_follow_temp = true;
        do
        {
            dec_article(g_selected_only,false);
        } while (g_art >= g_first_art && (was_read(g_art) || !parse_header(g_art)));
        g_search_ahead = ArticleNum{};
        if (g_art >= g_first_art)
        {
            return AS_NORM;
        }
        g_art = g_abs_first;
        // FALL THROUGH

    case 'P':         // goto previous article
        g_s_follow_temp = true; // keep going until change req.
        g_univ_follow_temp = true;
        dec_article(false,true);
check_dec_art:
        if (g_art < g_abs_first)
        {
            if (g_verbose)
            {
                std::printf("\nThere are no%s%s articles prior to this one.\n",
                       *g_buf=='P'?"":" unread",
                       g_selected_only?" selected":"");
            }
            else
            {
                std::printf("\nNo previous%s%s articles\n",
                       *g_buf=='P'?"":" unread",
                       g_selected_only?" selected":"");
            }
            term_down(2);
            g_art = g_curr_art;
            g_artp = g_curr_artp;
            return AS_ASK;
        }
        g_reread = true;
        g_search_ahead = ArticleNum{};
        return AS_NORM;

    case '-':
    case '\b':  case '\177':
        if (g_recent_art >= 0)
        {
            g_art = g_recent_art;
            g_artp = g_recent_artp;
            g_reread = true;
            g_force_last = true;
            g_search_ahead = ArticleNum{-(g_search_ahead.value_of() != 0)};
            return AS_NORM;
        }
        s_exit_code = NG_MINUS;
        return AS_CLEAN;

    case 'n':         // find next unread article?
        if (g_sa_in && g_s_default_cmd && !(g_sa_follow || g_s_follow_temp))
        {
            return AS_SA;
        }
        if (g_univ_read_virt_flag && g_univ_default_cmd && !(g_sa_in && (g_sa_follow || g_s_follow_temp)) &&
            !(g_univ_follow || g_univ_follow_temp))
        {
            s_exit_code = NG_NEXT;
            return AS_CLEAN;
        }
        if (!g_univ_default_cmd)
        {
            g_univ_follow_temp = true;
        }
        if (!g_s_default_cmd)
        {
            g_s_follow_temp = true;     // keep going until change req.
        }
        if (g_art > g_last_art)
        {
            if (!g_newsgroup_ptr->to_read)
            {
                return AS_CLEAN;
            }
            top_article();
            if (g_sa_in)
            {
                return AS_SA;
            }
        }
        else if (g_scan_on && !g_threaded_group && g_search_ahead)
        {
            *g_buf = Ctl('n');
            if (!next_article_with_subj())
            {
                goto normal_search;
            }
            return AS_NORM;
        }
        else
        {
            // TODO: will this work with 4.0?
            if (g_sa_in && g_threaded_group)
            {
                Article* old_artp = g_artp;
                inc_article(g_selected_only,false);
                if (!g_artp || !old_artp)
                {
                    return AS_SA;
                }
                switch (g_sel_mode)
                {
                case SM_ARTICLE:
                    if (g_s_default_cmd)
                    {
                        return AS_SA;
                    }
                    break;

                case SM_SUBJECT:
                    if (old_artp->subj != g_artp->subj)
                    {
                        return AS_SA;
                    }
                    break;

                case SM_THREAD:
                    if (old_artp->subj->thread != g_artp->subj->thread)
                    {
                        return AS_SA;
                    }
                    break;

                default:
                    // HUH?  Just hope for the best
                    break;
                }
            }
            else
            {
                inc_article(g_selected_only,false);
            }
            if (g_art > g_last_art)
            {
                top_article();
            }
        }
        g_search_ahead = ArticleNum{};
        return AS_NORM;

    case 'N':                 // goto next article
        if (g_sa_in && g_s_default_cmd && !(g_sa_follow || g_s_follow_temp))
        {
            return AS_SA;
        }
        if (g_univ_read_virt_flag && g_univ_default_cmd && !(g_sa_in && (g_sa_follow || g_s_follow_temp)) &&
            !(g_univ_follow || g_univ_follow_temp))
        {
            s_exit_code = NG_NEXT;
            return AS_CLEAN;
        }
        if (!g_univ_default_cmd)
        {
            g_univ_follow_temp = true;
        }
        if (!g_s_default_cmd)
        {
            g_s_follow_temp = true;     // keep going until change req.
        }
        if (g_art > g_last_art)
        {
            if (!g_first_subject)
            {
                g_art = g_abs_first;
                g_artp = article_ptr(g_art);
            }
            else
            {
                g_artp = g_first_subject->articles;
                if (g_artp->flags & AF_EXISTS)
                {
                    g_art = article_num(g_artp);
                }
                else
                {
                    inc_article(false,true);
                }
            }
        }
        else
        {
            inc_article(false,true);
        }
        if (g_art <= g_last_art)
        {
            g_reread = true;
        }
        else
        {
            g_force_last = true;
        }
        g_search_ahead = ArticleNum{};
        return AS_NORM;

    case '$':
        g_art = article_after(g_last_art);
        g_artp = nullptr;
        g_force_last = true;
        g_search_ahead = ArticleNum{};
        return AS_NORM;

    case '0':
    case '1': case '2': case '3':     // goto specified article
    case '4': case '5': case '6':     // or do something with a range
    case '7': case '8': case '9': case '.':
        g_force_last = true;
        switch (num_num())
        {
        case NN_INP:
            return AS_INP;

        case NN_ASK:
            return AS_ASK;

        case NN_REREAD:
            g_reread = true;
            if (g_search_ahead)
            {
                g_search_ahead = ArticleNum{-1};
            }
            break;

        case NN_NORM:
            if (g_use_threads)
            {
                erase_line(false);
                perform_status_end(g_newsgroup_ptr->to_read, "article");
                std::fputs(g_msg, stdout);
            }
            newline();
            return AS_ASK;
        }
        return AS_NORM;

    case Ctl('k'):
        edit_kill_file();
        return AS_ASK;

    case Ctl('n'):    // search for next article with same subject
    case Ctl('p'):    // search for previous article with same subject
        if (g_sa_in && g_s_default_cmd && *g_buf == Ctl('n') && !(g_sa_follow || g_s_follow_temp))
        {
            return AS_SA;
        }
        if (g_univ_read_virt_flag && g_univ_default_cmd && (*g_buf == Ctl('n')) && !(g_sa_in && (g_sa_follow || g_s_follow_temp)) &&
            !(g_univ_follow || g_univ_follow_temp))
        {
            s_exit_code = NG_NEXT;
            return AS_CLEAN;
        }
        if (!g_univ_default_cmd)
        {
            g_univ_follow_temp = true;
        }
        if (!g_s_default_cmd)
        {
            g_s_follow_temp = true;     // keep going until change req.
        }
        if (*g_buf == Ctl('n')? next_article_with_subj() : prev_article_with_subj())
        {
            return AS_NORM;
        }
        // FALL THROUGH

    case '/': case '?':
normal_search:
    {         // search for article by pattern
        char cmd = *g_buf;

        g_reread = true;                // assume this
        g_page_line = 1;
        switch (art_search(g_buf, (sizeof g_buf), true))
        {
        case SRCH_ERROR:
            g_art = g_curr_art;
            return AS_ASK;

        case SRCH_ABORT:
            g_art = g_curr_art;
            return AS_INP;

        case SRCH_INTR:
            if (g_verbose)
            {
                std::printf("\n(Interrupted at article %ld)\n",(long)g_art.value_of());
            }
            else
            {
                std::printf("\n(Intr at %ld)\n",(long)g_art.value_of());
            }
            term_down(2);
            g_art = g_curr_art;     // restore to current article
            return AS_ASK;

        case SRCH_DONE:
            if (g_use_threads)
            {
                erase_line(false);
                perform_status_end(g_newsgroup_ptr->to_read, "article");
                std::printf("%s\n",g_msg);
            }
            else
            {
                std::fputs("done\n",stdout);
            }
            term_down(1);
            pad(g_just_a_sec/3);        // 1/3 second
            if (!g_search_ahead)
            {
                g_art = g_curr_art;
                return AS_ASK;
            }
            top_article();
            g_reread = false;
            return AS_NORM;

        case SRCH_SUBJ_DONE:
            if (g_sa_in)
            {
                return AS_SA;
            }
            top_article();
            g_reread = false;
            return AS_NORM;

        case SRCH_NOT_FOUND:
            std::fputs("\n\n\n\nNot found.\n",stdout);
            term_down(5);
            g_art = g_curr_art;  // restore to current article
            if (g_sa_in)
            {
                return AS_SA;
            }
            return AS_ASK;

        case SRCH_FOUND:
            if (cmd == Ctl('n') || cmd == Ctl('p'))
            {
                g_old_subject = true;
                g_reread = false;
            }
            break;
        }
        return AS_NORM;
    }

    case 'u':                 // unsubscribe from this newsgroup?
        newline();
        std::printf(g_unsub_to,g_newsgroup_name.c_str());
        term_down(1);
        g_newsgroup_ptr->subscribe_char = UNSUBSCRIBED_CHAR;
        g_newsgroup_ptr->rc->flags |= RF_RC_CHANGED;
        --g_newsgroup_to_read;
        return AS_CLEAN;

    case 'M':
        if (g_art <= g_last_art)
        {
            delay_unmark(g_artp);
            one_less(g_artp);
            std::printf("\nArticle %ld will return.\n", g_art.value_of());
            term_down(2);
        }
        return AS_ASK;

    case 'm':
        if (g_art >= g_abs_first && g_art <= g_last_art)
        {
            unmark_as_read(g_artp);
            std::printf("\nArticle %ld marked as still unread.\n", g_art.value_of());
            term_down(2);
        }
        return AS_ASK;

    case 'c':                 // catch up
        switch (ask_catchup())
        {
        case 'n':
            return AS_ASK;
        case 'u':
            return AS_CLEAN;
        }
        g_art = article_after(g_last_art);
        g_artp = nullptr;
        g_force_last = false;
        return AS_NORM;

    case 'Q':  case '`':
        s_exit_code = NG_ASK;
        return AS_CLEAN;

    case 'q':                 // go back up to newsgroup level?
        s_exit_code = NG_NEXT;
        return AS_CLEAN;

    case 'i':
        g_auto_view_inline = !g_auto_view_inline;
        if (g_auto_view_inline != 0)
        {
            g_first_view = ArticleLine{};
        }
        std::printf("\nAuto-View inlined mime is %s\n", g_auto_view_inline? "on" : "off");
        term_down(2);
        break;

    case 'j':
        newline();
        if (g_art >= g_abs_first && g_art <= g_last_art)
        {
            mark_as_read(g_artp);
        }
        return AS_ASK;

    case 'h':
        univ_help(UHELP_ART);
        return AS_ASK;

    case 'H':                 // help?
        help_art();
        return AS_ASK;

    case '&':
        if (switcheroo()) // get rest of command
        {
            return AS_INP;      // if rubbed out, try something else
        }
        return AS_ASK;

    case '#':
        if (g_verbose)
        {
            std::printf("\nThe last article is %ld.\n",(long)g_last_art.value_of());
        }
        else
        {
            std::printf("\n%ld\n",(long)g_last_art.value_of());
        }
        term_down(2);
        return AS_ASK;

    case '+':                 // enter selection mode
run_the_selector:
        if (s_art_sel_lock)
        {
            std::printf("\nAlready inside article selector!\n");
            term_down(2);
            return AS_ASK;
        }
        // modes do not mix very well, so turn off the SA mode
        g_sa_in = false;
        // turn on temporary follow
        g_s_follow_temp = true;
        g_univ_follow_temp = true;
        s_art_sel_lock = true;
        *g_buf = article_selector(*g_buf);
        s_art_sel_lock = false;
        switch (*g_buf)
        {
        case '+':
            newline();
            g_term_scrolled = g_tc_LINES;
            g_term_line = g_tc_LINES-1;
            return AS_ASK;

        case 'Q':
            s_exit_code = NG_ASK;
            break;

        case 'q':
            s_exit_code = NG_NEXT;
            break;

        case 'N':
            s_exit_code = NG_SEL_NEXT;
            break;

        case 'P':
            s_exit_code = NG_SEL_PRIOR;
            break;

        case ';':
            g_sa_do_sel_threads = true;
            g_sa_go_explicit = true;
            return AS_SA;

        default:
            if (g_newsgroup_ptr->to_read)
            {
                return AS_NORM;
            }
            break;
        }
        return AS_CLEAN;

    case '=':                 // list subjects
    {
        ArticleNum oldart = g_art;
        page_start();
        article_walk(output_subject, AF_UNREAD);
        g_int_count = 0;
        g_subj_line = nullptr;
        g_art = oldart;
        return AS_ASK;
    }

    case '^':
        top_article();
        g_search_ahead = ArticleNum{};
        return AS_NORM;

#ifdef DEBUG
    case 'D':
        std::printf("\nFirst article: %ld\n",g_first_art.value_of());
        term_down(2);
        article_walk(debug_article_output, 0);
        g_int_count = 0;
        return AS_ASK;
#endif

    case 'v':
        if (g_art <= g_last_art)
        {
            g_reread = true;
            g_do_hiding = false;
        }
        return AS_NORM;

    case Ctl('r'):
        g_do_hiding = true;
        g_rotate = false;
        if (g_art <= g_last_art)
        {
            g_reread = true;
        }
        else
        {
            g_force_last = true;
        }
        return AS_NORM;

    case 'x':
    case Ctl('x'):
        // In the future the behavior of 'x' may change back to a
        // filter-select mechanism.
        // Currently, both keys do ROT-13 translation.
        //
        g_rotate = true;
        if (g_art <= g_last_art)
        {
            g_reread = true;
        }
        else
        {
            g_force_last = true;
        }
        return AS_NORM;

    case 'X':
        g_rotate = !g_rotate;
        // FALL THROUGH

    case 'l': case Ctl('l'):          // refresh screen
refresh_screen:
        if (g_art <= g_last_art)
        {
            g_reread = true;
            clear();
            g_do_fseek = true;
            g_art_line_num = g_top_line;
            g_art_line_num = std::max(g_art_line_num, ArticleLine{});
        }
        return AS_NORM;

    case Ctl('^'):
        erase_line(false);              // erase the prompt
#ifdef MAIL_CALL
        set_mail(true);          // force a mail check
#endif
        return AS_ASK;

    case Ctl('e'):
        if (g_art <= g_last_art)
        {
            if (g_art_size < 0)
            {
                nntp_finish_body(FB_OUTPUT);
                g_raw_art_size = nntp_art_size();
                g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
            }
            if (g_do_hiding)
            {
                seek_art_buf(g_art_size);
                seek_art_buf(g_art_pos);
            }
            g_reread = true;
            g_do_fseek = true;
            g_top_line = g_art_line_num;
            g_inner_light = line_before(g_art_line_num);
            g_inner_search = g_art_size;
            g_g_line = 0;
            g_hide_everything = 'b';
        }
        return AS_NORM;

    case 'B':                         // back up one line
    case 'b': case Ctl('b'):          // back up a page
        if (g_art <= g_last_art)
        {
            ArticleLine target;

            g_reread = true;
            clear();
            g_do_fseek = true;
            if (*g_buf == 'B')
            {
                target = line_before(g_top_line);
            }
            else
            {
                target = g_top_line - ArticleLine{g_tc_LINES - 2};
                if (g_marking && (g_marking_areas & BACK_PAGE_MARKING))
                {
                    g_highlight = g_top_line;
                }
            }
            g_art_line_num = g_top_line;
            if (g_art_line_num >= 0)
            {
                do
                {
                    g_art_line_num--;
                } while (g_art_line_num >= 0 && g_art_line_num > target &&
                         virtual_read(line_before(g_art_line_num)) >= 0);
            }
            g_top_line = g_art_line_num;
            g_art_line_num = std::max(g_art_line_num, ArticleLine{});
        }
        return AS_NORM;

    case '!':                 // shell escape
        if (escapade())
        {
            return AS_INP;
        }
        return AS_ASK;

    case 'C':
        cancel_article();
        return AS_ASK;

    case 'Z':
    case 'z':
        supersede_article();    // supersedes
        return AS_ASK;

    case 'R':
    case 'r':                 // reply?
    {
        reply();
        return AS_ASK;
    }

    case 'F':
    case 'f':                 // followup command
    {
        followup();
        g_force_grow = true;             // recalculate g_lastart
        return AS_ASK;
    }

    case Ctl('f'):                    // forward?
    {
        forward();
        return AS_ASK;
    }

    case '|':
    case 'w': case 'W':
    case 's': case 'S':       // save command
    case 'e':                 // extract command
        if (save_article() == SAVE_ABORT)
        {
            return AS_INP;
        }
        g_int_count = 0;
        return AS_ASK;

#if 0
    case 'E':
        if (decode_fp)
        {
            decode_end();
        }
        else
        {
            newline();
        }
        return AS_ASK;
#endif

    case 'a':                 // attachment-view command
        newline();
        if (view_article() == SAVE_ABORT)
        {
            return AS_INP;
        }
        g_int_count = 0;
        return AS_ASK;

    case 'Y':                         // yank back M articles
        yank_back();
        top_article();                  // from the beginning
        return AS_NORM;                 // pretend nothing happened

#ifdef STRICT_CR
    case '\n':   case '\r':
        std::fputs(g_bad_cr,stdout);
        return AS_ASK;
#endif

    case '_':
        if (!finish_dbl_char())
        {
            return AS_INP;
        }
        switch (g_buf[1] & 0177)
        {
        case 'P':
            --g_art;
            goto check_dec_art;

        case 'N':
            if (g_art > g_last_art)
            {
                g_art = g_abs_first;
            }
            else
            {
                ++g_art;
            }
            if (g_art <= g_last_art)
            {
                g_reread = true;
            }
            g_search_ahead = ArticleNum{};
            return AS_NORM;

        case '+':
            if (!g_artp)
            {
                goto not_threaded;
            }
            if (g_threaded_group)
            {
                select_articles_thread(g_artp, AUTO_KILL_NONE);
                std::printf("\nSelected all articles in this thread.\n");
            }
            else
            {
                select_articles_subject(g_artp, AUTO_KILL_NONE);
                std::printf("\nSelected all articles in this subject.\n");
            }
            term_down(2);
            g_artp = first_art(g_artp->subj);
            if (g_artp != nullptr)
            {
                if (g_art == article_num(g_artp))
                {
                    return AS_ASK;
                }
                g_art = article_num(g_artp);
            }
            return AS_NORM;

        case '-':
            if (!g_artp)
            {
                goto not_threaded;
            }
            if (g_sel_mode == SM_THREAD)
            {
                deselect_articles_thread(g_artp);
                std::printf("\nDeselected all articles in this thread.\n");
            }
            else
            {
                deselect_articles_subject(g_artp);
                std::printf("\nDeselected all articles in this subject.\n");
            }
            term_down(2);
            return AS_ASK;

        case 'C':
            if (!*(++g_char_subst))
            {
                g_char_subst = g_charsets.c_str();
            }
            goto refresh_screen;

        case 'a':  case 's':  case 't':  case 'T':
            *g_buf = g_buf[1];
            goto run_the_selector;

        case 'm':
            if (!g_artp)
            {
                goto not_threaded;
            }
            kill_sub_thread(g_artp, SET_TO_RETURN | AFFECT_ALL);
            return AS_NORM;

        case 'M':
            if (!g_artp)
            {
                goto not_threaded;
            }
            kill_articles_thread(g_artp, SET_TO_RETURN | AFFECT_ALL);
            return AS_NORM;
        }
        // FALL THROUGH

    default:
        std::printf("\n%s",g_h_for_help);
        term_down(2);
        settle_down();
        break;
    }
    return AS_ASK;
}

// see if there is any mail

#ifdef MAIL_CALL
void set_mail(bool force)
{
    if (force)
    {
        g_mail_count = 0;
    }
    if (!(g_mail_count++))
    {
        const char* mailfile = file_exp(get_val_const("MAILFILE",MAIL_FILE));
        stat_t mail_file_stat{};
        if (stat(mailfile,&mail_file_stat) < 0 || !mail_file_stat.st_size
            || mail_file_stat.st_atime > mail_file_stat.st_mtime)
        {
            g_mail_call = "";
        }
        else
        {
            g_mail_call = get_val("MAILCALL", "(Mail) ");
        }
    }
    g_mail_count %= 5;                   // check every 5 articles
}
#endif

void set_default_cmd()
{
    if (!g_newsgroup_ptr || !g_newsgroup_ptr->to_read)
    {
        g_default_cmd = "npq";
    }
    else
    {
#if 0
        if (multimedia_mime == true)
        {
            multimedia_mime++;
            g_dfltcmd = "anpq";
        }
        else
#endif
        if (g_search_ahead)
        {
            g_default_cmd = "^Nnpq";
        }
        else
        {
            g_default_cmd = "npq";
        }
    }
}

// Ask the user about catching-up the current group.  Returns 'y' if yes,
// 'n' or 'N' if no ('N' means we used one line when in the selector),
// or 'u' for yes with unsubscribe.  Actually performs the catchup and
// unsubscribe as needed.
//
char ask_catchup()
{
    bool use_one_line = (g_general_mode == GM_SELECTOR);
    int leave_unread = 0;

    if (!use_one_line)
    {
        newline();
    }
reask_catchup:
    if (g_verbose)
    {
        std::sprintf(g_buf, "Mark everything in %s as read?", g_newsgroup_name.c_str());
    }
    else
    {
        std::sprintf(g_buf, "Catchup %s?", g_newsgroup_name.c_str());
    }
    in_char(g_buf,MM_CONFIRM_CATCH_UP_PROMPT,"yn#h");
    print_cmd();
    char ch = *g_buf;
    if (ch == 'h' || ch == 'H')
    {
        use_one_line = false;
        if (g_verbose)
        {
            std::fputs("\n"
                 "Type y or SP to mark all articles as read.\n"
                 "Type n to leave articles marked as they are.\n"
                 "The # means enter a number to mark all but the last # articles as read.\n"
                 "Type u to mark everything read and unsubscribe.\n\n",
                 stdout);
        }
        else
        {
            std::fputs("\n"
                 "y or SP to mark all read.\n"
                 "n to forget it.\n"
                 "# means enter a number to leave unread.\n"
                 "u to mark all and unsubscribe.\n\n",
                 stdout);
        }
        term_down(6);
        goto reask_catchup;
    }
    if (ch == 'n' || ch == 'q')
    {
        if (use_one_line)
        {
            return 'N';
        }
        newline();
        return 'n';
    }
    if (ch == '#')
    {
        use_one_line = false;
        in_char("\nEnter approx. number of articles to leave unread: ", MM_CONFIRM_CATCH_UP_PROMPT, "0");
        ch = *g_buf;
        if (ch == '0')
        {
            ch = 'y';
        }
    }
    if (isdigit(ch))
    {
        g_buf[1] = FINISH_CMD;
        if (!finish_command(false))
        {
            use_one_line = false;
            newline();
            goto reask_catchup;
        }
        else
        {
            leave_unread = atoi(g_buf);
            ch = 'y';
        }
    }
    if (ch != 'y' && ch != 'u')
    {
        use_one_line = false;
        std::printf("\n%s\n", g_h_for_help);
        term_down(3);
        settle_down();
        goto reask_catchup;
    }
    if (g_in_ng)
    {
        article_walk(mark_all_read, leave_unread);
        if (leave_unread)
        {
            count_subjects(CS_NORM);
            g_newsgroup_ptr->to_read = (ArticleUnread)g_obj_count.value_of();
        }
        else
        {
            g_selected_count = false;
            g_selected_subj_cnt = false;
            g_selected_only = false;
            g_newsgroup_ptr->to_read = 0;
            if (g_dm_count)
            {
                yank_back();
            }
        }
        newline();
    }
    else
    {
        newline();
        catch_up(g_newsgroup_ptr, leave_unread, 1);
    }
    if (ch == 'u')
    {
        g_newsgroup_ptr->subscribe_char = UNSUBSCRIBED_CHAR;
        g_newsgroup_ptr->rc->flags |= RF_RC_CHANGED;
        --g_newsgroup_to_read;
        newline();
        std::printf(g_unsub_to,g_newsgroup_name.c_str());
        std::printf("(If you meant to hit 'y' instead of 'u', press '-'.)\n");
        term_down(2);
    }
    return ch;
}

static bool count_uncached_article(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if ((ap->flags & (AF_UNREAD|AF_CACHED)) == AF_UNREAD)
    {
        ++g_obj_count;
    }
    return false;
}

static bool mark_all_read(char *ptr, int leave_unread)
{
    Article* ap = (Article*)ptr;
    if (article_num(ap) > g_last_art - ArticleNum{leave_unread})
    {
        return true;
    }
    ap->flags &= ~(static_cast<ArticleFlags>(g_sel_mask) |AF_UNREAD);
    return false;
}

static bool mark_all_unread(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if ((ap->flags & (AF_UNREAD | AF_EXISTS)) == AF_EXISTS)
    {
        ap->flags |= AF_UNREAD;         // mark as unread
        ++g_obj_count;
    }
    return false;
}

bool output_subject(char *ptr, int flag)
{
    char tmpbuf[256];

    if (g_int_count)
    {
        return true;
    }

    if (!g_subj_line)
    {
        g_subj_line = get_val("SUBJLINE",nullptr);
        if (!g_subj_line)
        {
            g_subj_line = "";
        }
    }

    Article *ap = (Article*)ptr;
    if (flag && !(ap->flags & flag))
    {
        return false;
    }
    ArticleNum i = article_num(ap);
    char *  s = fetch_subj(i, false);
    if (s != nullptr)
    {
        std::sprintf(tmpbuf,"%-5ld ", i.value_of());
        int len = std::strlen(tmpbuf);
        if (!empty(g_subj_line))
        {
            g_art = i;
            interp(tmpbuf + len, sizeof tmpbuf - len, g_subj_line);
        }
        else
        {
            safe_copy(tmpbuf + len, s, sizeof tmpbuf - len);
        }
        if (g_mode == MM_PROCESSING_KILL)
        {
            g_page_line = 1;
        }
        if (print_lines(tmpbuf, NO_MARKING) != 0)
        {
            return true;
        }
    }
    return false;
}

#ifdef DEBUG
static bool debug_article_output(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if (g_int_count)
    {
        return true;
    }
    if (article_num(ap) >= g_first_art && ap->subj)
    {
        std::printf("%5ld %c %s\n", article_num(ap).value_of(),
               (ap->flags & AF_UNREAD)? 'y' : 'n', ap->subj->str);
        term_down(1);
    }
    return false;
}
#endif

char ask_memorize(char_int ch)
{
    bool thread_cmd = (ch == 'T');
    bool use_one_line = (g_general_mode == GM_SELECTOR);
    bool global_save = false;
    const char* mode_string = (thread_cmd? "thread" : "subject");
    const char* mode_phrase = (thread_cmd? "replies to this article" :
                                     "this subject and all replies");
    ArticleNum art_hold = g_art;
    Article* artp_hold = g_artp;

    if (!use_one_line)
    {
        newline();
    }
reask_memorize:
    std::sprintf(g_cmd_buf,"%sMemorize %s command:", global_save?"Global-" : "",
            mode_string);
    in_char(g_cmd_buf, MM_MEMORIZE_THREAD_PROMPT, thread_cmd? "+S.mJK,jcC" : "+S.mJK,jcCfg");
    print_cmd();
    ch = *g_buf;
    if (!thread_cmd && ch == 'f')
    {
        mode_string = *mode_string == 'a'? "subject" : "author";
        erase_line(false);
        goto reask_memorize;
    }
    if (!thread_cmd && ch == 'g')
    {
        global_save = !global_save;
        erase_line(false);
        goto reask_memorize;
    }
    if (ch == 'h' || ch == 'H')
    {
        use_one_line = false;
        if (g_verbose)
        {
            std::printf("\n"
                   "Type + or SP to auto-select this %s (i.e. includes future articles).\n"
                   "Type S to auto-select the current subject.\n"
                   "Type . to auto-select %s.\n"
                   "Type m to auto-select the current article.\n"
                   "Type J to auto-kill (junk) this %s.\n"
                   "Type K to auto-kill the current subject.\n"
                   "Type , to auto-kill %s.\n"
                   "Type j to auto-kill the current article.\n"
                   "Type C to clear all selection/killing on %s.\n"
                   "Type c to clear all selection/killing on this %s.\n"
                   "Type q to abort the operation.\n",
                   mode_string, mode_phrase, mode_string, mode_phrase, mode_phrase, mode_string);
            if (!thread_cmd)
            {
                std::printf("Type f to toggle author (from-line) searching.\n"
                       "Type g to toggle global memorization.\n");
                term_down(2);
            }
        }
        else
        {
            std::printf("\n"
                   "+ or SP auto-selects this %s.\n"
                   "S auto-selects the subject.\n"
                   ". auto-selects %s.\n"
                   "m auto-selects this article.\n"
                   "J auto-kills this %s.\n"
                   "K auto-kills the subject.\n"
                   ", auto-kills %s.\n"
                   "j auto-kills the current article.\n"
                   "C clears auto-commands for %s.\n"
                   "c clears auto-commands for this %s.\n"
                   "q aborts.\n",
                   mode_string, mode_phrase, mode_string, mode_phrase, mode_phrase, mode_string);
            if (!thread_cmd)
            {
                std::printf("f toggles author (from) mode.\n"
                       "g toggles global memorization.\n");
                term_down(2);
            }
        }
        newline();
        term_down(9);
        goto reask_memorize;
    }
    if (ch == 'q')
    {
        if (use_one_line)
        {
            return 'Q';
        }
        newline();
        return 'q';
    }
    if (!thread_cmd)
    {
        g_buf[1] = *mode_string == 'a'? 'f' : 's';
        g_buf[2] = global_save? 'g' : 'l';
    }
    if (ch == '+')
    {
        if (!thread_cmd)
        {
            (void)art_search(g_buf, (sizeof g_buf), true);
            g_art = art_hold;
            g_artp = artp_hold;
            ch = '.';
        }
        else
        {
            select_articles_thread(g_artp, AUTO_SEL_THD);
            ch = (use_one_line? '+' : '.');
        }
        if (g_general_mode != GM_SELECTOR)
        {
            std::printf("\nSelection memorized.\n");
            term_down(2);
        }
    }
    else if (ch == 'S')
    {
        select_articles_subject(g_artp, AUTO_SEL_SBJ);
        ch = (use_one_line? '+' : '.');
        if (g_general_mode != GM_SELECTOR)
        {
            std::printf("\nSelection memorized.\n");
            term_down(2);
        }
    }
    else if (ch == '.')
    {
        if (!thread_cmd)
        {
            (void)art_search(g_buf, (sizeof g_buf), true);
            g_art = art_hold;
            g_artp = artp_hold;
        }
        else
        {
            select_sub_thread(g_artp, AUTO_SEL_FOL);
            ch = (use_one_line? '+' : '.');
        }
        if (g_general_mode != GM_SELECTOR)
        {
            std::printf("\nSelection memorized.\n");
            term_down(2);
        }
    }
    else if (ch == 'm')
    {
        if (g_artp)
        {
            change_auto_flags(g_artp, AUTO_SEL_1);
            ch = (use_one_line? '+' : '.');
            if (g_general_mode != GM_SELECTOR)
            {
                std::printf("\nSelection memorized.\n");
                term_down(2);
            }
        }
    }
    else if (ch == 'J')
    {
        if (!thread_cmd)
        {
            *g_buf = 'K';
            (void)art_search(g_buf, (sizeof g_buf), true);
            g_art = art_hold;
            g_artp = artp_hold;
        }
        else
        {
            kill_thread(g_artp->subj->thread, AFFECT_ALL | AUTO_KILL_THD);
        }
        if (g_general_mode != GM_SELECTOR)
        {
            std::printf("\nKill memorized.\n");
            term_down(2);
        }
    }
    else if (ch == 'j')
    {
        if (g_artp)
        {
            mark_as_read(g_artp);
            change_auto_flags(g_artp, AUTO_KILL_1);
            if (g_general_mode != GM_SELECTOR)
            {
                std::printf("\nKill memorized.\n");
                term_down(2);
            }
        }
    }
    else if (ch == 'K')
    {
        kill_subject(g_artp->subj,AFFECT_ALL|AUTO_KILL_SBJ);
        if (g_general_mode != GM_SELECTOR)
        {
            std::printf("\nKill memorized.\n");
            term_down(2);
        }
    }
    else if (ch == ',')
    {
        if (!thread_cmd)
        {
            (void)art_search(g_buf, (sizeof g_buf), true);
            g_art = art_hold;
            g_artp = artp_hold;
        }
        else
        {
            kill_sub_thread(g_artp, AFFECT_ALL | AUTO_KILL_FOL);
        }
        if (g_general_mode != GM_SELECTOR)
        {
            std::printf("\nKill memorized.\n");
            term_down(2);
        }
    }
    else if (ch == 'C')
    {
        if (thread_cmd)
        {
            clear_thread(g_artp->subj->thread);
        }
        else
        {
            clear_subject(g_artp->subj);
        }
    }
    else if (ch == 'c')
    {
        clear_sub_thread(g_artp);
    }
#if 0
    else if (ch == 's')
    {
        g_buf[1] = FINISHCMD;
        finish_command(1);
        (void)art_search(g_buf, (sizeof g_buf), true);
        g_art = art_hold;
        g_artp = artp_hold;
    }
#endif
    else
    {
        use_one_line = false;
        std::printf("\n%s\n", g_h_for_help);
        term_down(3);
        settle_down();
        goto reask_memorize;
    }
    if (!use_one_line)
    {
        newline();
    }
    return ch;
}
