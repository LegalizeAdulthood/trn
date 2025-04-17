/* kfile.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/kfile.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/artsrch.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/color.h"
#include "trn/hash.h"
#include "trn/ng.h"
#include "trn/ngstuff.h"
#include "trn/rcstuff.h"
#include "trn/rt-process.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rthread.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

std::FILE          *g_local_kfp{};               /* local (for this newsgroup) file */
KillFileStateFlags g_kf_state{};               /* the state of our kill files */
KillFileStateFlags g_kfs_thread_change_set{};  /* bits to set for thread changes */
int                 g_kf_change_thread_cnt{};       /* # entries changed from old to new */
ArticleNum             g_kill_first{};              /* used as g_firstart when killing */

static void mention(const char *str);
static bool kfile_junk(char *ptr, int killmask);
static int write_local_thread_commands(int keylen, HashDatum *data, int extra);
static int write_global_thread_commands(int keylen, HashDatum *data, int appending);
static int age_thread_commands(int keylen, HashDatum *data, int elapsed_days);

static std::FILE          *s_globkfp{};                /* global article killer file */
static KillFileStateFlags s_kfs_local_change_clear{}; /* bits to clear local changes */
static int                 s_kf_thread_cnt{};          /* # entries in the thread kfile */
static long                s_kf_daynum{};              /* day number for thread killfile */
static bool                s_exitcmds{};
static char                s_thread_cmd_ltr[] = "JK,j+S.m";
static AutoKillFlags      s_thread_cmd_flag[] = {
         AUTO_KILL_THD, AUTO_KILL_SBJ, AUTO_KILL_FOL, AUTO_KILL_1, AUTO_SEL_THD, AUTO_SEL_SBJ, AUTO_SEL_FOL, AUTO_SEL_1,
};
static char  s_killglobal[] = KILLGLOBAL;
static char  s_killlocal[] = KILLLOCAL;
static char  s_killthreads[] = KILLTHREADS;
static bool  s_kill_mentioned;
static std::FILE *s_newkfp{};

inline long killfile_daynum(long x)
{
    return (long) std::time(nullptr) / 86400 - 10490 - x;
}

void kill_file_init()
{
    char* cp = get_val("KILLTHREADS");
    if (!cp)
    {
        cp = s_killthreads;
    }
    if (*cp && std::strcmp(cp,"none") != 0)
    {
        s_kf_daynum = killfile_daynum(0);
        s_kf_thread_cnt = 0;
        g_kf_change_thread_cnt = 0;
        std::FILE *fp = std::fopen(filexp(cp), "r");
        if (fp != nullptr)
        {
            g_msgid_hash = hash_create(1999, msgid_cmp);
            while (std::fgets(g_buf, sizeof g_buf, fp) != nullptr)
            {
                if (*g_buf == '<')
                {
                    cp = std::strchr(g_buf,' ');
                    if (!cp)
                    {
                        cp = ",";
                    }
                    else
                    {
                        *cp++ = '\0';
                    }
                    int age = s_kf_daynum - std::atol(cp + 1);
                    if (age > KF_MAX_DAYS)
                    {
                        g_kf_change_thread_cnt++;
                        continue;
                    }
                    cp = std::strchr(s_thread_cmd_ltr, *cp);
                    if (cp != nullptr)
                    {
                        int auto_flag = s_thread_cmd_flag[cp - s_thread_cmd_ltr];
                        HashDatum data = hash_fetch(g_msgid_hash, g_buf, std::strlen(g_buf));
                        if (!data.dat_ptr)
                        {
                            data.dat_ptr = savestr(g_buf);
                        }
                        else
                        {
                            g_kf_change_thread_cnt++;
                        }
                        data.dat_len = auto_flag | age;
                        hash_store_last(data);
                    }
                    s_kf_thread_cnt++;
                }
            }
            std::fclose(fp);
        }
        g_kf_state |= KFS_GLOBAL_THREAD_FILE;
        s_kfs_local_change_clear = KFS_LOCAL_CHANGES;
        g_kfs_thread_change_set = KFS_THREAD_CHANGES;
    }
    else
    {
        s_kfs_local_change_clear = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
        g_kfs_thread_change_set = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
    }
}

static void mention(const char *str)
{
    if (g_verbose)
    {
        color_string(COLOR_NOTICE,str);
        newline();
    }
    else
    {
        std::putchar('.');
    }
    std::fflush(stdout);
}

int do_kill_file(std::FILE *kfp, int entering)
{
    bool first_time = (entering && !g_kill_first);
    char last_kill_type = '\0';
    int thread_kill_cnt = 0;
    int thread_select_cnt = 0;
    char* cp;
    char* bp;

    g_art = g_last_art+1;
    g_kill_first = g_first_art;
    std::fseek(kfp,0L,0);                    /* rewind file */
    while (std::fgets(g_buf, LBUFLEN, kfp) != nullptr)
    {
        if (*(cp = g_buf + std::strlen(g_buf) - 1) == '\n')
        {
            *cp = '\0';
        }
        bp = skip_space(g_buf);
        if (!std::strncmp(bp, "THRU", 4))
        {
            int len = std::strlen(g_newsgroup_ptr->rc->name);
            cp = skip_space(bp + 4);
            if (std::strncmp(cp, g_newsgroup_ptr->rc->name, len) != 0 || !std::isspace(cp[len]))
            {
                continue;
            }
            g_kill_first = std::atol(cp+len+1)+1;
            g_kill_first = std::max(g_kill_first, g_first_art);
            if (g_kill_first > g_last_art)
            {
                g_kill_first = g_last_art + 1;
            }
            continue;
        }
        if (*bp == 'I')
        {
            cp = skip_non_space(bp + 1);
            cp = skip_space(cp);
            if (!*cp)
            {
                continue;
            }
            cp = filexp(cp);
            if (!std::strchr(cp, '/'))
            {
                set_ngname(cp);
                cp = filexp(get_val_const("KILLLOCAL",s_killlocal));
                set_ngname(g_newsgroup_ptr->rc_line);
            }
            std::FILE *incfile = std::fopen(cp, "r");
            if (incfile != nullptr)
            {
                int ret = do_kill_file(incfile, entering);
                std::fclose(incfile);
                if (ret)
                {
                    return ret;
                }
            }
            continue;
        }
        if (*bp == 'X')                 /* exit command? */
        {
            if (entering)
            {
                s_exitcmds = true;
                continue;
            }
            bp++;
        }
        else if (!entering)
        {
            continue;
        }

        if (*bp == '&')
        {
            mention(bp);
            if (bp > g_buf)
            {
                std::strcpy(g_buf, bp);
            }
            switcheroo();
        }
        else if (*bp == '/')
        {
            g_kf_state |= KFS_NORMAL_LINES;
            if (g_first_art > g_last_art)
            {
                continue;
            }
            if (last_kill_type)
            {
                if (perform_status_end(g_newsgroup_ptr->to_read, "article"))
                {
                    s_kill_mentioned = true;
                    carriage_return();
                    std::fputs(g_msg, stdout);
                    newline();
                }
            }
            perform_status_init(g_newsgroup_ptr->to_read);
            last_kill_type = '/';
            mention(bp);
            s_kill_mentioned = true;
            switch (art_search(bp, (sizeof g_buf) - (bp - g_buf), false))
            {
            case SRCH_ABORT:
                continue;

            case SRCH_INTR:
                if (g_verbose)
                {
                    std::printf("\n(Interrupted at article %ld)\n", (long) g_art);
                }
                else
                {
                    std::printf("\n(Intr at %ld)\n", (long) g_art);
                }
                termdown(2);
                return -1;

            case SRCH_DONE:
                break;

            case SRCH_SUBJ_DONE:
                /*std::fputs("\tsubject not found (?)\n",stdout);*/
                break;

            case SRCH_NOT_FOUND:
                /*std::fputs("\tnot found\n",stdout);*/
                break;

            case SRCH_FOUND:
                /*std::fputs("\tfound\n",stdout);*/
                break;

            case SRCH_ERROR:
                break;
            }
        }
        else if (first_time && *bp == '<')
        {
            if (last_kill_type != '<')
            {
                if (last_kill_type)
                {
                    if (perform_status_end(g_newsgroup_ptr->to_read, "article"))
                    {
                        s_kill_mentioned = true;
                        carriage_return();
                        std::fputs(g_msg, stdout);
                        newline();
                    }
                }
                perform_status_init(g_newsgroup_ptr->to_read);
                last_kill_type = '<';
            }
            cp = std::strchr(bp,' ');
            if (!cp)
            {
                cp = "T,";
            }
            else
            {
                *cp++ = '\0';
            }
            Article *ap = get_article(bp);
            if (ap != nullptr)
            {
                if ((ap->flags & AF_FAKE) && !ap->child1)
                {
                    if (*cp == 'T')
                    {
                        cp++;
                    }
                    cp = std::strchr(s_thread_cmd_ltr, *cp);
                    if (cp != nullptr)
                    {
                        ap->auto_flags = s_thread_cmd_flag[cp-s_thread_cmd_ltr];
                        if (ap->auto_flags & AUTO_KILL_MASK)
                        {
                            thread_kill_cnt++;
                        }
                        else
                        {
                            thread_select_cnt++;
                        }
                    }
                }
                else
                {
                    g_art = article_num(ap);
                    g_artp = ap;
                    perform(cp,false);
                    if (ap->auto_flags & AUTO_SEL_MASK)
                    {
                        thread_select_cnt++;
                    }
                    else if (ap->auto_flags & AUTO_KILL_MASK)
                    {
                        thread_kill_cnt++;
                    }
                }
            }
            g_art = g_last_art+1;
            g_kf_state |= KFS_THREAD_LINES;
        }
        else if (*bp == '<')
        {
            g_kf_state |= KFS_THREAD_LINES;
        }
        else if (*bp == '*')
        {
            int killmask = AF_UNREAD;
            switch (bp[1])
            {
            case 'X':
                killmask |= g_sel_mask; /* don't kill selected articles */
                /* FALL THROUGH */

            case 'j':
                article_walk(kfile_junk, killmask);
                break;
            }
            g_kf_state |= KFS_NORMAL_LINES;
        }
    }
    if (thread_kill_cnt)
    {
        std::sprintf(g_buf,"%ld auto-kill command%s.", (long)thread_kill_cnt,
                plural(thread_kill_cnt));
        mention(g_buf);
        s_kill_mentioned = true;
    }
    if (thread_select_cnt)
    {
        std::sprintf(g_buf,"%ld auto-select command%s.", (long)thread_select_cnt,
                plural(thread_select_cnt));
        mention(g_buf);
        s_kill_mentioned = true;
    }
    if (last_kill_type)
    {
        if (perform_status_end(g_newsgroup_ptr->to_read, "article"))
        {
            s_kill_mentioned = true;
            carriage_return();
            std::fputs(g_msg, stdout);
            newline();
        }
    }
    return 0;
}

static bool kfile_junk(char *ptr, int killmask)
{
    Article* ap = (Article*)ptr;
    if ((ap->flags & killmask) == AF_UNREAD)
    {
        set_read(ap);
    }
    else if (ap->flags & g_sel_mask)
    {
        ap->flags &= ~static_cast<ArticleFlags>(g_sel_mask);
        if (!g_selected_count--)
        {
            g_selected_count = 0;
        }
    }
    return false;
}

void kill_unwanted(ArticleNum starting, const char *message, int entering)
{
    bool intr = false;                  /* did we get an interrupt? */
    MinorMode oldmode = g_mode;
    bool anytokill = (g_newsgroup_ptr->to_read > 0);

    set_mode(GM_READ,MM_PROCESSING_KILL);
    if ((entering || s_exitcmds) && (g_local_kfp || s_globkfp))
    {
        s_exitcmds = false;
        ArticleNum oldfirst = g_first_art;
        g_first_art = starting;
        clear();
        if (message && (g_verbose || entering))
        {
            std::fputs(message, stdout);
        }

        s_kill_mentioned = false;
        if (g_local_kfp)
        {
            if (entering)
            {
                g_kf_state |= KFS_LOCAL_CHANGES;
            }
            intr = do_kill_file(g_local_kfp, entering);
        }
        open_kill_file(KF_GLOBAL);          /* Just in case the name changed */
        if (s_globkfp && !intr)
        {
            intr = do_kill_file(s_globkfp, entering);
        }
        newline();
        if (entering && s_kill_mentioned && g_novice_delays)
        {
            if (g_verbose)
            {
                get_anything();
            }
            else
            {
                pad(g_just_a_sec);
            }
        }
        if (anytokill)                  /* if there was anything to kill */
        {
            g_force_last = false;        /* allow for having killed it all */
        }
        g_first_art = oldfirst;
    }
    if (!entering && (g_kf_state & KFS_LOCAL_CHANGES) && !intr)
    {
        rewrite_kill_file(g_last_art);
    }
    set_mode(g_general_mode,oldmode);
}

static int write_local_thread_commands(int keylen, HashDatum *data, int extra)
{
    Article* ap = (Article*)data->dat_ptr;
    int autofl = ap->auto_flags;
    char ch;

    if (autofl && ((ap->flags & AF_EXISTS) || ap->child1))
    {
        /* The arrays are in priority order, so find highest priority bit. */
        for (int i = 0; s_thread_cmd_ltr[i]; i++)
        {
            if (autofl & s_thread_cmd_flag[i])
            {
                ch = s_thread_cmd_ltr[i];
                break;
            }
        }
        std::fprintf(s_newkfp,"%s T%c\n", ap->msg_id, ch);
    }
    return 0;
}

void rewrite_kill_file(ArticleNum thru)
{
    bool has_content = (g_kf_state & (KFS_THREAD_LINES|KFS_GLOBAL_THREAD_FILE))
                                 == KFS_THREAD_LINES;
    bool has_star_commands = false;
    bool needs_newline = false;
    char* killname = filexp(get_val_const("KILLLOCAL",s_killlocal));
    char* bp;

    if (g_local_kfp)
    {
        std::fseek(g_local_kfp, 0L, 0);       /* rewind current file */
    }
    else
    {
        makedir(killname, MD_FILE);
    }
    remove(killname);                   /* to prevent file reuse */
    g_kf_state &= ~(s_kfs_local_change_clear | KFS_NORMAL_LINES);
    s_newkfp = std::fopen(killname, "w");
    if (s_newkfp != nullptr)
    {
        std::fprintf(s_newkfp,"THRU %s %ld\n",g_newsgroup_ptr->rc->name,(long)thru);
        while (g_local_kfp && std::fgets(g_buf, LBUFLEN, g_local_kfp) != nullptr)
        {
            if (!std::strncmp(g_buf, "THRU", 4))
            {
                char* cp = g_buf+4;
                int len = std::strlen(g_newsgroup_ptr->rc->name);
                cp = skip_space(cp);
                if (std::isdigit(*cp))
                {
                    continue;
                }
                if (std::strncmp(cp, g_newsgroup_ptr->rc->name, len) != 0 || (cp[len] && !std::isspace(cp[len])))
                {
                    std::fputs(g_buf,s_newkfp);
                    needs_newline = !std::strchr(g_buf,'\n');
                }
                continue;
            }
            bp = skip_space(g_buf);
            /* Leave out any outdated thread commands */
            if (*bp == 'T' || *bp == '<')
            {
                continue;
            }
            /* Write star commands after other kill commands */
            if (*bp == '*')
            {
                has_star_commands = true;
            }
            else
            {
                std::fputs(g_buf,s_newkfp);
                needs_newline = !std::strchr(bp,'\n');
            }
            has_content = true;
        }
        if (needs_newline)
        {
            std::putc('\n', s_newkfp);
        }
        if (has_star_commands)
        {
            std::fseek(g_local_kfp,0L,0);                     /* rewind file */
            while (std::fgets(g_buf, LBUFLEN, g_local_kfp) != nullptr)
            {
                bp = skip_space(g_buf);
                if (*bp == '*')
                {
                    std::fputs(g_buf,s_newkfp);
                    needs_newline = !std::strchr(bp,'\n');
                }
            }
            if (needs_newline)
            {
                std::putc('\n', s_newkfp);
            }
        }
        if (!(g_kf_state & KFS_GLOBAL_THREAD_FILE))
        {
            /* Append all the still-valid thread commands */
            hash_walk(g_msgid_hash, write_local_thread_commands, 0);
        }
        std::fclose(s_newkfp);
        if (!has_content)
        {
            remove(killname);
        }
        open_kill_file(KF_LOCAL);           /* and reopen local file */
    }
    else
    {
        std::printf(g_cantcreate, g_buf);
    }
}

static int write_global_thread_commands(int keylen, HashDatum *data, int appending)
{
    int autofl;
    int age;
    char* msgid;
    char ch;

    if (data->dat_len)
    {
        if (appending)
        {
            return 0;
        }
        autofl = data->dat_len;
        age = autofl & KF_AGE_MASK;
        msgid = data->dat_ptr;
    }
    else
    {
        Article* ap = (Article*)data->dat_ptr;
        autofl = ap->auto_flags;
        if (!autofl || (appending && (autofl & AUTO_OLD)))
        {
            return 0;
        }
        ap->auto_flags |= AUTO_OLD;
        age = 0;
        msgid = ap->msg_id;
    }

    /* The arrays are in priority order, so find highest priority bit. */
    for (int i = 0; s_thread_cmd_ltr[i]; i++)
    {
        if (autofl & s_thread_cmd_flag[i])
        {
            ch = s_thread_cmd_ltr[i];
            break;
        }
    }
    std::fprintf(s_newkfp,"%s %c %ld\n", msgid, ch, s_kf_daynum - age);
    s_kf_thread_cnt++;

    return 0;
}

static int age_thread_commands(int keylen, HashDatum *data, int elapsed_days)
{
    if (data->dat_len)
    {
        int age = (data->dat_len & KF_AGE_MASK) + elapsed_days;
        if (age > KF_MAX_DAYS)
        {
            std::free(data->dat_ptr);
            g_kf_change_thread_cnt++;
            return -1;
        }
        data->dat_len += elapsed_days;
    }
    else
    {
        Article* ap = (Article*)data->dat_ptr;
        if (ap->auto_flags & AUTO_OLD)
        {
            ap->auto_flags &= ~AUTO_OLD;
            g_kf_change_thread_cnt++;
            g_kf_state |= KFS_THREAD_CHANGES;
        }
    }
    return 0;
}

void update_thread_kill_file()
{
    if (!(g_kf_state & KFS_GLOBAL_THREAD_FILE))
    {
        return;
    }

    int elapsed_days = killfile_daynum(s_kf_daynum);
    if (elapsed_days)
    {
        hash_walk(g_msgid_hash, age_thread_commands, elapsed_days);
        s_kf_daynum += elapsed_days;
    }

    if (!(g_kf_state & KFS_THREAD_CHANGES))
    {
        return;
    }

    char *cp = filexp(get_val_const("KILLTHREADS", s_killthreads));
    makedir(cp, MD_FILE);
    if (g_kf_change_thread_cnt * 5 > s_kf_thread_cnt)
    {
        remove(cp);                     /* to prevent file reuse */
        s_newkfp = std::fopen(cp, "w");
        if (s_newkfp == nullptr)
        {
            return; /* Yikes! */
        }
        s_kf_thread_cnt = 0;
        g_kf_change_thread_cnt = 0;
        hash_walk(g_msgid_hash, write_global_thread_commands, 0); /* Rewrite */
    }
    else
    {
        s_newkfp = std::fopen(cp, "a");
        if (s_newkfp == nullptr)
        {
            return; /* Yikes! */
        }
        hash_walk(g_msgid_hash, write_global_thread_commands, 1); /* Append */
    }
    std::fclose(s_newkfp);

    g_kf_state &= ~KFS_THREAD_CHANGES;
}

void change_auto_flags(Article *ap, AutoKillFlags auto_flag)
{
    if (auto_flag > (ap->auto_flags & (AUTO_KILL_MASK | AUTO_SEL_MASK)))
    {
        if (ap->auto_flags & AUTO_OLD)
        {
            g_kf_change_thread_cnt++;
        }
        ap->auto_flags = auto_flag;
        g_kf_state |= g_kfs_thread_change_set;
    }
}

void clear_auto_flags(Article *ap)
{
    if (ap->auto_flags)
    {
        if (ap->auto_flags & AUTO_OLD)
        {
            g_kf_change_thread_cnt++;
        }
        ap->auto_flags = AUTO_KILL_NONE;
        g_kf_state |= g_kfs_thread_change_set;
    }
}

void perform_auto_flags(Article *ap, AutoKillFlags thread_flags, AutoKillFlags subj_flags, AutoKillFlags chain_flags)
{
    if (thread_flags & AUTO_SEL_THD)
    {
        if (g_sel_mode == SM_THREAD)
        {
            select_arts_thread(ap, AUTO_SEL_THD);
        }
        else
        {
            select_arts_subject(ap, AUTO_SEL_THD);
        }
    }
    else if (subj_flags & AUTO_SEL_SBJ)
    {
        select_arts_subject(ap, AUTO_SEL_SBJ);
    }
    else if (chain_flags & AUTO_SEL_FOL)
    {
        select_subthread(ap, AUTO_SEL_FOL);
    }
    else if (ap->auto_flags & AUTO_SEL_1)
    {
        select_article(ap, AUTO_SEL_1);
    }

    if (thread_flags & AUTO_KILL_THD)
    {
        if (g_sel_mode == SM_THREAD)
        {
            kill_arts_thread(ap, AFFECT_ALL | AUTO_KILL_THD);
        }
        else
        {
            kill_arts_subject(ap, AFFECT_ALL | AUTO_KILL_THD);
        }
    }
    else if (subj_flags & AUTO_KILL_SBJ)
    {
        kill_arts_subject(ap, AFFECT_ALL | AUTO_KILL_SBJ);
    }
    else if (chain_flags & AUTO_KILL_FOL)
    {
        kill_subthread(ap, AFFECT_ALL | AUTO_KILL_FOL);
    }
    else if (ap->auto_flags & AUTO_KILL_1)
    {
        mark_as_read(ap);
    }
}

/* edit KILL file for newsgroup */

void edit_kill_file()
{
    char* bp;

    if (g_in_ng)
    {
        if (g_kf_state & KFS_LOCAL_CHANGES)
        {
            rewrite_kill_file(g_last_art);
        }
        if (!(g_kf_state & KFS_GLOBAL_THREAD_FILE))
        {
            for (Subject *sp = g_first_subject; sp; sp = sp->next)
            {
                clear_subject(sp);
            }
        }
        std::strcpy(g_buf,filexp(get_val_const("KILLLOCAL",s_killlocal)));
    }
    else
    {
        std::strcpy(g_buf, filexp(get_val_const("KILLGLOBAL", s_killglobal)));
    }
    if (!makedir(g_buf, MD_FILE))
    {
        std::sprintf(g_cmd_buf,"%s %s",
            filexp(get_val_const("VISUAL",get_val_const("EDITOR",DEFEDITOR))),g_buf);
        std::printf("\nEditing %s KILL file:\n%s\n",
            (g_in_ng?"local":"global"),g_cmd_buf);
        termdown(3);
        resetty();                      /* make sure tty is friendly */
        doshell(SH, g_cmd_buf);         /* invoke the shell */
        noecho();                       /* and make terminal */
        crmode();                       /*   unfriendly again */
        open_kill_file(g_in_ng);
        if (g_local_kfp)
        {
            std::fseek(g_local_kfp,0L,0);     /* rewind file */
            g_kf_state &= ~KFS_NORMAL_LINES;
            while (std::fgets(g_buf, LBUFLEN, g_local_kfp) != nullptr)
            {
                bp = skip_space(g_buf);
                if (*bp == '/' || *bp == '*')
                {
                    g_kf_state |= KFS_NORMAL_LINES;
                }
                else if (*bp == '<')
                {
                    char* cp = std::strchr(bp,' ');
                    if (!cp)
                    {
                        cp = ",";
                    }
                    else
                    {
                        *cp++ = '\0';
                    }
                    Article *ap = get_article(bp);
                    if (ap != nullptr)
                    {
                        if (*cp == 'T')
                        {
                            cp++;
                        }
                        cp = std::strchr(s_thread_cmd_ltr, *cp);
                        if (cp != nullptr)
                        {
                            ap->auto_flags |= s_thread_cmd_flag[cp - s_thread_cmd_ltr];
                        }
                    }
                }
            }
        }
    }
    else
    {
        std::printf("Can't make %s\n",g_buf);
        termdown(1);
    }
}

void open_kill_file(int local)
{
    const char *kname = filexp(local ? get_val_const("KILLLOCAL", s_killlocal) : get_val_const("KILLGLOBAL", s_killglobal));

    /* delete the file if it is empty */
    if (std::filesystem::exists(kname) && std::filesystem::file_size(kname) == 0)
    {
        std::filesystem::remove(kname);
    }
    if (local)
    {
        if (g_local_kfp)
        {
            std::fclose(g_local_kfp);
        }
        g_local_kfp = std::fopen(kname,"r");
    }
    else
    {
        if (s_globkfp)
        {
            std::fclose(s_globkfp);
        }
        s_globkfp = std::fopen(kname,"r");
    }
}

void kill_file_append(const char *cmd, bool local)
{
    std::strcpy(g_cmd_buf, filexp(local ? get_val_const("KILLLOCAL", s_killlocal) : get_val_const("KILLGLOBAL", s_killglobal)));
    if (!makedir(g_cmd_buf, MD_FILE))
    {
        if (g_verbose)
        {
            std::printf("\nDepositing command in %s...", g_cmd_buf);
        }
        else
        {
            std::printf("\n--> %s...", g_cmd_buf);
        }
        std::fflush(stdout);
        if (g_novice_delays)
        {
            sleep(2);
        }
        if (std::FILE *fp = std::fopen(g_cmd_buf, "a+"))
        {
            char ch;
            if (std::fseek(fp,-1L,2) < 0)
            {
                ch = '\n';
            }
            else
            {
                ch = std::getc(fp);
            }
            std::fseek(fp,0L,2);
            if (ch != '\n')
            {
                std::putc('\n', fp);
            }
            std::fprintf(fp,"%s\n",cmd);
            std::fclose(fp);
            if (local && !g_local_kfp)
            {
                open_kill_file(KF_LOCAL);
            }
            std::fputs("done\n",stdout);
        }
        else
        {
            std::printf(g_cantopen, g_cmd_buf);
        }
        termdown(2);
    }
    g_kf_state |= KFS_NORMAL_LINES;
}
