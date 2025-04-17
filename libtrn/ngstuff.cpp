/* ngstuff.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/ngstuff.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/addng.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/change_dir.h"
#include "trn/final.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/opt.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/respond.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rt-wumpus.h"
#include "trn/rthread.h"
#include "trn/string-algos.h"
#include "trn/sw.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cstring>

bool        g_one_command{}; /* no ':' processing in perform() */
std::string g_save_dir;       /* -d */

/* given the new and complex universal/help possibilities,
 * the following interlock variable may save some trouble.
 * (if true, we are currently processing options)
 */
static bool s_option_sel_ilock{};

void newsgroup_stuff_init()
{
    s_option_sel_ilock = false;
}

/* do a shell escape */

int escapade()
{
    bool interactive = (g_buf[1] == FINISHCMD);
    char whereiam[1024];

    if (!finish_command(interactive))   /* get remainder of command */
    {
        return -1;
    }
    char *s = g_buf + 1;
    bool  docd = *s != '!';
    if (!docd)
    {
        s++;
    }
    else
    {
        trn_getwd(whereiam, sizeof(whereiam));
        if (change_dir(g_priv_dir))
        {
            std::printf(g_nocd,g_priv_dir.c_str());
            sig_catcher(0);
        }
    }
    s = skip_eq(s, ' ');                /* skip leading spaces */
    interp(g_cmd_buf, (sizeof g_cmd_buf), s);/* interpret any % escapes */
    reset_tty();                          /* make sure tty is friendly */
    doshell(nullptr,g_cmd_buf); /* invoke the shell */
    no_echo();                           /* and make terminal */
    cr_mode();                           /*   unfriendly again */
    if (docd)
    {
        if (change_dir(whereiam))
        {
            std::printf(g_nocd,whereiam);
            sig_catcher(0);
        }
    }
#ifdef MAILCALL
    g_mail_count = 0;                    /* force recheck */
#endif
    return 0;
}

/* process & command */

int switcheroo()
{
    if (!finish_command(true)) /* get rest of command */
    {
        return -1;      /* if rubbed out, try something else */
    }
    if (!g_buf[1])
    {
        const std::string prior_savedir = g_save_dir;
        if (s_option_sel_ilock)
        {
            g_buf[1] = '\0';
            return 0;
        }
        s_option_sel_ilock = true;
        if (g_general_mode != GM_SELECTOR || g_sel_mode != SM_OPTIONS)
        {
            option_selector();
        }
        s_option_sel_ilock = false;
        if (g_save_dir != prior_savedir)
        {
            cwd_check();
        }
        g_buf[1] = '\0';
    }
    else if (g_buf[1] == '&')
    {
        if (!g_buf[2])
        {
            page_start();
            show_macros();
        }
        else
        {
            char tmpbuf[LBUFLEN];
            char *s = skip_space(g_buf + 2);
            mac_line(s,tmpbuf,(sizeof tmpbuf));
        }
    }
    else
    {
        bool docd = (in_string(g_buf,"-d", true) != nullptr);
         char whereami[1024];
        char tmpbuf[LBUFLEN+16];

        if (docd)
        {
            trn_getwd(whereami, sizeof(whereami));
        }
        if (g_buf[1] == '-' || g_buf[1] == '+')
        {
            std::strcpy(tmpbuf,g_buf+1);
            sw_list(tmpbuf);
        }
        else
        {
            std::sprintf(tmpbuf,"[options]\n%s\n",g_buf+1);
            prep_ini_data(tmpbuf,"'&' input");
            parse_ini_section(tmpbuf+10,g_options_ini);
            set_options(ini_values(g_options_ini));
        }
        if (docd)
        {
            cwd_check();
            if (change_dir(whereami))                /* -d does chdirs */
            {
                std::printf(g_nocd,whereami);
                sig_catcher(0);
            }
        }
    }
    return 0;
}

/* process range commands */

NumNumResult num_num()
{
    ArticleNum min;
    ArticleNum max;
    char   * cmdlst = nullptr;
    char* s;
    char* c;
    ArticleNum oldart = g_art;
    char tmpbuf[LBUFLEN];
    bool output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    bool justone = true;                /* assume only one article */

    if (!finish_command(true))  /* get rest of command */
    {
        return NN_INP;
    }
    if (g_last_art < 1)
    {
        error_msg("No articles");
        return NN_ASK;
    }
    if (g_search_ahead)
    {
        g_search_ahead = -1;
    }

    perform_status_init(g_newsgroup_ptr->to_read);

    for (s=g_buf; *s && (std::isdigit(*s) || std::strchr(" ,-.$",*s)); s++)
    {
        if (!std::isdigit(*s))
        {
            justone = false;
        }
    }
    if (*s)
    {
        cmdlst = savestr(s);
        justone = false;
    }
    else if (!justone)
    {
        cmdlst = savestr("m");
    }
    *s++ = ',';
    *s = '\0';
    safecpy(tmpbuf,g_buf,LBUFLEN);
    if (!output_level && !justone)
    {
        std::printf("Processing...");
        std::fflush(stdout);
    }
    for (char *t = tmpbuf; (c = std::strchr(t, ',')) != nullptr; t = ++c)
    {
        *c = '\0';
        if (*t == '.')
        {
            min = oldart;
        }
        else
        {
            min = std::atol(t);
        }
        if (min < g_abs_first)
        {
            min = g_abs_first;
            std::sprintf(g_msg,"(First article is %ld)",(long)g_abs_first);
            warn_msg(g_msg);
        }
        if ((t = std::strchr(t, '-')) != nullptr)
        {
            t++;
            if (*t == '$')
            {
                max = g_last_art;
            }
            else if (*t == '.')
            {
                max = oldart;
            }
            else
            {
                max = std::atol(t);
            }
        }
        else
        {
            max = min;
        }
        if (max > g_last_art)
        {
            max = g_last_art;
            min = std::min(min, max);
            std::sprintf(g_msg,"(Last article is %ld)",(long)g_last_art);
            warn_msg(g_msg);
        }
        if (max < min)
        {
            error_msg("Bad range");
            if (cmdlst)
            {
                std::free(cmdlst);
            }
            return NN_ASK;
        }
        if (justone)
        {
            g_art = min;
            return NN_REREAD;
        }
        for (g_art = article_first(min); g_art <= max; g_art = article_next(g_art))
        {
            g_artp = article_ptr(g_art);
            if (perform(cmdlst, output_level && g_page_line == 1) < 0)
            {
                if (g_verbose)
                {
                    std::sprintf(g_msg, "(Interrupted at article %ld)", (long) g_art);
                }
                else
                {
                    std::sprintf(g_msg, "(Intr at %ld)", (long) g_art);
                }
                error_msg(g_msg);
                if (cmdlst)
                {
                    std::free(cmdlst);
                }
                return NN_ASK;
            }
            if (!output_level)
            {
                perform_status(g_newsgroup_ptr->to_read, 50);
            }
        }
    }
    g_art = oldart;
    if (cmdlst)
    {
        free(cmdlst);
    }
    return NN_NORM;
}

int thread_perform()
{
    Subject*sp;
    Article*ap;
    int     bits;
    bool    output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    bool    one_thread = false;

    if (!finish_command(true))  /* get rest of command */
    {
        return 0;
    }
    if (!g_buf[1])
    {
        return -1;
    }
    int len = 1;
    if (g_buf[1] == ':')
    {
        bits = 0;
        len++;
    }
    else
    {
        bits = SF_VISIT;
    }
    if (g_buf[len] == '.')
    {
        if (!g_artp)
        {
            return -1;
        }
        one_thread = true;
        len++;
    }
    char *cmdstr = savestr(g_buf + len);
    bool  want_unread = !g_sel_rereading && *cmdstr != 'm';

    perform_status_init(g_newsgroup_ptr->to_read);
    len = std::strlen(cmdstr);

    if (!output_level && !one_thread)
    {
        std::printf("Processing...");
        std::fflush(stdout);
    }
    /* A few commands can just loop through the subjects. */
    if ((len == 1 && (*cmdstr == 't' || *cmdstr == 'J'))                       //
        || (len == 2                                                           //
            && (((*cmdstr == '+' || *cmdstr == '-') && cmdstr[0] == cmdstr[1]) //
                || *cmdstr == 'T' || *cmdstr == 'A')))
    {
        g_performed_article_loop = false;
        if (one_thread)
        {
            sp = (g_sel_mode == SM_THREAD ? g_artp->subj->thread->subj : g_artp->subj);
        }
        else
        {
            sp = next_subj((Subject *) nullptr, bits);
        }
        for (; sp; sp = next_subj(sp, bits))
        {
            if ((!(sp->flags & g_sel_mask) ^ !bits) || !sp->misc)
            {
                continue;
            }
            g_artp = first_art(sp);
            if (g_artp)
            {
                g_art = article_num(g_artp);
                if (perform(cmdstr, 0) < 0)
                {
                    error_msg("Interrupted");
                    goto break_out;
                }
            }
            if (one_thread)
            {
                break;
            }
        }
#if 0
    }
    else if (!std::strcmp(cmdstr, "E"))
    {
        /* The 'E'nd-decode command doesn't do any looping at all. */
        if (decode_fp)
        {
            decode_end();
        }
#endif
    }
    else if (*cmdstr == 'p')
    {
        ArticleNum oldart = g_art;
        g_art = g_last_art+1;
        followup();
        g_force_grow = true;
        g_art = oldart;
        g_page_line++;
    }
    else
    {
        /* The rest loop through the articles. */
        /* Use the explicit article-order if it exists */
        if (g_art_ptr_list)
        {
            Article** limit = g_art_ptr_list + g_art_ptr_list_size;
            sp = (g_sel_mode==SM_THREAD? g_artp->subj->thread->subj : g_artp->subj);
            for (Article **app = g_art_ptr_list; app < limit; app++)
            {
                ap = *app;
                if (one_thread && ap->subj->thread != sp->thread)
                {
                    continue;
                }
                if ((!(ap->flags & AF_UNREAD) ^ want_unread) //
                    && !(ap->flags & g_sel_mask) ^ !!bits)
                {
                    g_art = article_num(ap);
                    g_artp = ap;
                    if (perform(cmdstr, output_level && g_page_line == 1) < 0)
                    {
                        error_msg("Interrupted");
                        goto break_out;
                    }
                }
                if (!output_level)
                {
                    perform_status(g_newsgroup_ptr->to_read, 50);
                }
            }
        }
        else
        {
            if (one_thread)
            {
                sp = (g_sel_mode == SM_THREAD ? g_artp->subj->thread->subj : g_artp->subj);
            }
            else
            {
                sp = next_subj((Subject *) nullptr, bits);
            }
            for (; sp; sp = next_subj(sp, bits))
            {
                for (ap = first_art(sp); ap; ap = next_art(ap))
                {
                    if ((!(ap->flags & AF_UNREAD) ^ want_unread)
                        && !(ap->flags & g_sel_mask) ^ !!bits)
                    {
                        g_art = article_num(ap);
                        g_artp = ap;
                        if (perform(cmdstr, output_level && g_page_line == 1) < 0)
                        {
                            error_msg("Interrupted");
                            goto break_out;
                        }
                    }
                }
                if (one_thread)
                {
                    break;
                }
                if (!output_level)
                {
                    perform_status(g_newsgroup_ptr->to_read, 50);
                }
            }
        }
    }
  break_out:
    std::free(cmdstr);
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

    if (output_level == 1)
    {
        std::printf("%-6ld ",g_art);
        std::fflush(stdout);
    }

    g_perform_count++;
    for (; (ch = *cmdlst) != 0; cmdlst++)
    {
        if (std::isspace(ch) || ch == ':')
        {
            continue;
        }
        if (ch == 'j')
        {
            if (savemode)
            {
                mark_as_read(g_artp);
                change_auto_flags(g_artp, AUTO_KILL_1);
            }
            else if (!was_read(g_art))
            {
                mark_as_read(g_artp);
                if (output_level && g_verbose)
                {
                    std::fputs("\tJunked", stdout);
                }
            }
            if (g_sel_rereading)
            {
                deselect_article(g_artp, output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == '+')
        {
            if (savemode || cmdlst[1] == '+')
            {
                if (g_sel_mode == SM_THREAD)
                {
                    select_arts_thread(g_artp, savemode ? AUTO_SEL_THD : AUTO_KILL_NONE);
                }
                else
                {
                    select_arts_subject(g_artp, savemode ? AUTO_SEL_SBJ : AUTO_KILL_NONE);
                }
                if (cmdlst[1] == '+')
                {
                    cmdlst++;
                }
            }
            else
            {
                select_article(g_artp, output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == 'S')
        {
            select_arts_subject(g_artp, AUTO_SEL_SBJ);
        }
        else if (ch == '.')
        {
            select_sub_thread(g_artp, savemode? AUTO_SEL_FOL : AUTO_KILL_NONE);
        }
        else if (ch == '-')
        {
            if (cmdlst[1] == '-')
            {
                if (g_sel_mode == SM_THREAD)
                {
                    deselect_arts_thread(g_artp);
                }
                else
                {
                    deselect_arts_subject(g_artp);
                }
                cmdlst++;
            }
            else
            {
                deselect_article(g_artp, output_level ? ALSO_ECHO : AUTO_KILL_NONE);
            }
        }
        else if (ch == ',')
        {
            kill_sub_thread(g_artp, AFFECT_ALL | (savemode? AUTO_KILL_FOL : AUTO_KILL_NONE));
        }
        else if (ch == 'J')
        {
            if (g_sel_mode == SM_THREAD)
            {
                kill_arts_thread(g_artp, AFFECT_ALL | (savemode ? AUTO_KILL_THD : AUTO_KILL_NONE));
            }
            else
            {
                kill_arts_subject(g_artp, AFFECT_ALL | (savemode ? AUTO_KILL_SBJ : AUTO_KILL_NONE));
            }
        }
        else if (ch == 'K' || ch == 'k')
        {
            kill_arts_subject(g_artp, AFFECT_ALL|(savemode? AUTO_KILL_SBJ : AUTO_KILL_NONE));
        }
        else if (ch == 'x')
        {
            if (!was_read(g_art))
            {
                one_less(g_artp);
                if (output_level && g_verbose)
                {
                    std::fputs("\tKilled", stdout);
                }
            }
            if (g_sel_rereading)
            {
                deselect_article(g_artp, AUTO_KILL_NONE);
            }
        }
        else if (ch == 't')
        {
            entire_tree(g_artp);
        }
        else if (ch == 'T')
        {
            savemode = 1;
        }
        else if (ch == 'A')
        {
            savemode = 2;
        }
        else if (ch == 'm')
        {
            if (savemode)
            {
                change_auto_flags(g_artp, AUTO_SEL_1);
            }
            else if ((g_artp->flags & (AF_UNREAD | AF_EXISTS)) == AF_EXISTS)
            {
                unmark_as_read(g_artp);
                if (output_level && g_verbose)
                {
                    std::fputs("\tMarked unread", stdout);
                }
            }
        }
        else if (ch == 'M')
        {
            delay_unmark(g_artp);
            one_less(g_artp);
            if (output_level && g_verbose)
            {
                std::fputs("\tWill return", stdout);
            }
        }
        else if (ch == '=')
        {
            carriage_return();
            output_subject((char*)g_artp,0);
            output_level = 0;
        }
        else if (ch == 'C')
        {
            int ret = cancel_article();
            if (output_level && g_verbose)
            {
                std::printf("\t%sanceled", ret ? "Not c" : "C");
            }
        }
        else if (ch == '%')
        {
            char tmpbuf[512];

            if (g_one_command)
            {
                interp(tmpbuf, (sizeof tmpbuf), cmdlst);
            }
            else
            {
                cmdlst = do_interp(tmpbuf, sizeof tmpbuf, cmdlst, ":", nullptr) - 1;
            }
            g_perform_count--;
            if (perform(tmpbuf,output_level?2:0) < 0)
            {
                return -1;
            }
        }
        else if (std::strchr("!&sSwWae|", ch))
        {
            if (g_one_command)
            {
                std::strcpy(g_buf, cmdlst);
            }
            else
            {
                cmdlst = cpytill(g_buf, cmdlst, ':') - 1;
            }
            /* we now have the command in g_buf */
            if (ch == '!')
            {
                escapade();
                if (output_level && g_verbose)
                {
                    std::fputs("\tShell escaped", stdout);
                }
            }
            else if (ch == '&')
            {
                switcheroo();
                if (output_level && g_verbose)
                {
                    if (g_buf[1] && g_buf[1] != '&')
                    {
                        std::fputs("\tSwitched", stdout);
                    }
                }
            }
            else
            {
                if (output_level != 1)
                {
                    erase_line(false);
                    std::printf("%-6ld ",g_art);
                }
                if (ch == 'a')
                {
                    view_article();
                }
                else
                {
                    save_article();
                }
                newline();
                output_level = 0;
            }
        }
        else
        {
            std::sprintf(g_msg,"Unknown command: %s",cmdlst);
            error_msg(g_msg);
            return -1;
        }
        if (output_level && g_verbose)
        {
            std::fflush(stdout);
        }
        if (g_one_command)
        {
            break;
        }
    }
    if (output_level && g_verbose)
    {
        newline();
    }
    if (g_int_count)
    {
        g_int_count = 0;
        return -1;
    }
    return 1;
}

int newsgroup_sel_perform()
{
    NewsgroupFlags bits;
    bool one_group = false;

    if (!finish_command(true))  /* get rest of command */
    {
        return 0;
    }
    if (!g_buf[1])
    {
        return -1;
    }
    int len = 1;
    if (g_buf[1] == ':')
    {
        bits = NF_NONE;
        len++;
    }
    else
    {
        bits = NF_INCLUDED;
    }
    if (g_buf[len] == '.')
    {
        if (!g_newsgroup_ptr)
        {
            return -1;
        }
        one_group = true;
        len++;
    }
    char *cmdstr = savestr(g_buf + len);

    perform_status_init(g_newsgroup_to_read);
    len = std::strlen(cmdstr);

    if (one_group)
    {
        newsgroup_perform(cmdstr, 0);
        goto break_out;
    }

    for (g_newsgroup_ptr = g_first_newsgroup; g_newsgroup_ptr; g_newsgroup_ptr = g_newsgroup_ptr->next)
    {
        if (g_sel_rereading? g_newsgroup_ptr->to_read != TR_NONE
                         : g_newsgroup_ptr->to_read < g_newsgroup_min_to_read)
        {
            continue;
        }
        set_newsgroup(g_newsgroup_ptr);
        if ((g_newsgroup_ptr->flags & bits) == bits //
            && (!(g_newsgroup_ptr->flags & static_cast<NewsgroupFlags>(g_sel_mask)) ^ !!bits))
        {
            if (newsgroup_perform(cmdstr, 0) < 0)
            {
                break;
            }
        }
        perform_status(g_newsgroup_to_read, 50);
    }

  break_out:
    std::free(cmdstr);
    return 1;
}

int newsgroup_perform(char *cmdlst, int output_level)
{
    int ch;

    if (output_level == 1)
    {
        std::printf("%s ",g_ngname.c_str());
        std::fflush(stdout);
    }

    g_perform_count++;
    for (; (ch = *cmdlst) != 0; cmdlst++)
    {
        if (std::isspace(ch) || ch == ':')
        {
            continue;
        }
        switch (ch)
        {
        case '+':
            if (!(g_newsgroup_ptr->flags & static_cast<NewsgroupFlags>(g_sel_mask)))
            {
                g_newsgroup_ptr->flags = ((g_newsgroup_ptr->flags | static_cast<NewsgroupFlags>(g_sel_mask)) & ~NF_DEL);
                g_selected_count++;
            }
            break;

        case 'c':
            catch_up(g_newsgroup_ptr, 0, 0);
            /* FALL THROUGH */

        case '-':
          deselect:
            if (g_newsgroup_ptr->flags & static_cast<NewsgroupFlags>(g_sel_mask))
            {
                g_newsgroup_ptr->flags &= ~static_cast<NewsgroupFlags>(g_sel_mask);
                if (g_sel_rereading)
                {
                    g_newsgroup_ptr->flags |= NF_DEL;
                }
                g_selected_count--;
            }
            break;

        case 'u':
            if (output_level && g_verbose)
            {
                std::printf(g_unsubto,g_newsgroup_ptr->rc_line);
                term_down(1);
            }
            g_newsgroup_ptr->subscribe_char = NEGCHAR;
            g_newsgroup_ptr->to_read = TR_UNSUB;
            g_newsgroup_ptr->rc->flags |= RF_RC_CHANGED;
            g_newsgroup_ptr->flags &= ~static_cast<NewsgroupFlags>(g_sel_mask);
            g_newsgroup_to_read--;
            goto deselect;

        default:
            std::sprintf(g_msg,"Unknown command: %s",cmdlst);
            error_msg(g_msg);
            return -1;
        }
        if (output_level && g_verbose)
        {
            std::fflush(stdout);
        }
        if (g_one_command)
        {
            break;
        }
    }
    if (output_level && g_verbose)
    {
        newline();
    }
    if (g_int_count)
    {
        g_int_count = 0;
        return -1;
    }
    return 1;
}

int add_group_sel_perform()
{
    int bits;
    bool one_group = false;

    if (!finish_command(true))  /* get rest of command */
    {
        return 0;
    }
    if (!g_buf[1])
    {
        return -1;
    }
    int len = 1;
    if (g_buf[1] == ':')
    {
        bits = 0;
        len++;
    }
    else
    {
        bits = g_sel_mask;
    }
    if (g_buf[len] == '.')
    {
        if (g_first_add_group)
        {
            return -1;
        }
        one_group = true;
        len++;
    }
    char *cmdstr = savestr(g_buf + len);

    perform_status_init(g_newsgroup_to_read);
    len = std::strlen(cmdstr);

    if (one_group)
    {
        goto break_out;
    }

    for (AddGroup *gp = g_first_add_group; gp; gp = gp->next)
    {
        if (!(gp->flags & g_sel_mask) ^ !!bits)
        {
            if (add_group_perform(gp, cmdstr, 0) < 0)
            {
                break;
            }
        }
        perform_status(g_newsgroup_to_read, 50);
    }

  break_out:
    free(cmdstr);
    return 1;
}

int add_group_perform(AddGroup *gp, char *cmdlst, int output_level)
{
    int ch;

    if (output_level == 1)
    {
        std::printf("%s ",gp->name);
        std::fflush(stdout);
    }

    g_perform_count++;
    for (; (ch = *cmdlst) != 0; cmdlst++)
    {
        if (std::isspace(ch) || ch == ':')
        {
            continue;
        }
        if (ch == '+')
        {
            gp->flags |= AGF_SEL;
            g_selected_count++;
        }
        else if (ch == '-')
        {
            gp->flags &= ~AGF_SEL;
            g_selected_count--;
        }
        else
        {
            std::sprintf(g_msg,"Unknown command: %s",cmdlst);
            error_msg(g_msg);
            return -1;
        }
        if (output_level && g_verbose)
        {
            std::fflush(stdout);
        }
        if (g_one_command)
        {
            break;
        }
    }
    if (output_level && g_verbose)
    {
        newline();
    }
    if (g_int_count)
    {
        g_int_count = 0;
        return -1;
    }
    return 1;
}
