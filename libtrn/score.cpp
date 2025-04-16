/* This file Copyright 1992 by Clifford A. Adams */
/* score.c
 *
 */

#include "config/common.h"
#include "trn/score.h"

#include "trn/artio.h" /* for g_openart var.*/
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/list.h"
#include "trn/ng.h" /* g_art */
#include "trn/ngdata.h"
#include "trn/rt-util.h" /* spinner */
#include "trn/samisc.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/score-easy.h" /* interactive menus and such */
#include "trn/scorefile.h"
#include "trn/scoresave.h"
#include "trn/string-algos.h"
#include "trn/sorder.h"
#include "trn/terminal.h" /* input_pending() */

#include <cstdio>

bool    g_kill_thresh_active{};  //
int     g_kill_thresh{LOWSCORE}; /* KILL articles at or below this score */
ART_NUM g_sc_fill_max;           /* maximum art# scored by fill-routine */
bool    g_sc_fill_read{};        /* true if also scoring read arts... */
bool    g_sc_initialized{};      /* has score been initialized (are we "in" scoring?) */
bool    g_sc_scoring{};          /* are we currently scoring an article (prevents loops) */
bool    g_score_newfirst{};      /* changes order of sorting (artnum comparison) when scores are equal */
bool    g_sc_savescores{};       /* If true, save the scores for this group on exit. */
bool    g_sc_delay{};            /* If true, delay initialization of scoring until explicitly required */

static bool s_sc_sf_force_init{}; /* If true, always sf_init() */
static bool s_sc_sf_delay{};      /* if true, delay loading rule files */
static bool s_sc_do_spin{};       /* actually do the score spinner */
static bool s_sc_rescoring{};     /* are we rescoring now? */

//bool pend_wait;       /* if true, enter pending mode when scoring... */
void sc_init(bool pend_wait)
{
    int i;

    if (g_lastart == 0 || g_lastart < g_absfirst)
    {
#if 0
        std::printf("No articles exist to be scored.\n");
#endif
        return;
    }
    s_sc_sf_force_init = true;          /* generally force initialization */
    if (g_sc_delay)                     /* requested delay? */
    {
        return;
    }
    s_sc_sf_delay = false;

/* Consider the relationships between scoring and article scan mode.
 * Should one be able to initialize the other?  How much can they depend on
 * each other when both are #defined?
 * Consider this especially in a later redesign or porting these systems
 * to other newsreaders.
 */
    g_kill_thresh_active = false;  /* kill thresholds are generic */
    /* July 24, 1993: changed default of g_sc_savescores to true */
    g_sc_savescores = true;

/* CONSIDER: (for sc_init callers) is g_lastart properly set yet? */
    g_sc_fill_max = g_absfirst - 1;
    g_sc_fill_read = g_sa_mode_read_elig || g_firstart > g_lastart;

    if (g_sf_verbose)
    {
        std::printf("\nScoring articles...");
        std::fflush(stdout);         /* print it *now* */
    }

    g_sc_initialized = true;    /* little white lie for lookahead */
    /* now is a good time to load a saved score-list which may exist */
    if (!s_sc_rescoring)        /* don't load if rescoring */
    {
        sc_load_scores();       /* will be quiet if non-existent */
        i = g_firstart;
        if (g_sc_fill_read)
        {
            i = g_absfirst;
        }
        if (s_sc_sf_force_init)
        {
            i = g_lastart+1;    /* skip loop */
        }
        for (i = article_first(i); i <= g_lastart; i = article_next(i))
        {
            if (!article_scored(i) && (g_sc_fill_read || article_unread(i)))
            {
                break;
            }
        }
        if (i == g_lastart)     /* all scored */
        {
            s_sc_sf_delay = true;
        }
    }
    if (s_sc_sf_force_init)
    {
        s_sc_sf_delay = false;
    }

    if (!s_sc_sf_delay)
    {
        sf_init();      /* initialize the scorefile code */
    }

    s_sc_do_spin = false;
    for (i = article_last(g_lastart); i >= g_absfirst; i = article_prev(i))
    {
        if (article_scored(i))
        {
            break;
        }
    }
    if (i < g_absfirst)                         /* none scored yet */
    {
        /* score one article, or give up */
        ART_NUM a;
        for (a = article_last(g_lastart); a >= g_absfirst; a = article_prev(a))
        {
            sc_score_art(a,true);       /* I want it *now* */
            if (article_scored(a))
            {
                break;
            }
        }
        if (a < g_absfirst)             /* no articles scored */
        {
            if (g_sf_verbose)
            {
                std::printf("\nNo articles available for scoring\n");
            }
            sc_cleanup();
            return;
        }
    }

    /* if no scoring rules/methods are present, score everything */
    /* XXX will be bad if later methods are added. */
    if (!g_sf_num_entries)
    {
        /* score everything really fast */
        for (ART_NUM art = article_last(g_lastart); art >= g_absfirst; art = article_prev(art))
        {
            sc_score_art(art,true);
        }
    }
    if (pend_wait)
    {
        bool waitflag = true; /* normal mode: wait for key first */
        if (g_sf_verbose && waitflag)
        {
#ifdef PENDING
            std::printf("(press key to start reading)");
#else
            std::printf("(interrupt to start reading)");
#endif
            std::fflush(stdout);
        }
        if (waitflag)
        {
            setspin(SPIN_FOREGROUND);
            s_sc_do_spin = true;                /* really do it */
        }
        sc_lookahead(true,waitflag);    /* jump in *now* */
        if (waitflag)
        {
            s_sc_do_spin = false;
            setspin(SPIN_POP);
        }
    }
    if (g_sf_verbose)
    {
        std::putchar('\n');
    }

    g_sc_initialized = true;
}

void sc_cleanup()
{
    if (!g_sc_initialized)
    {
        return;
    }

    if (g_sc_savescores)
    {
        sc_save_scores();
    }
    g_sc_loaded_count = 0;

    if (g_sf_verbose)
    {
        std::printf("\nCleaning up scoring...");
        std::fflush(stdout);
    }

    if (!s_sc_sf_delay)
    {
        sf_clean();     /* let the scorefile do whatever cleaning it needs */
    }
    g_sc_initialized = false;

    if (g_sf_verbose)
    {
        std::printf("Done.\n");
    }
}

void sc_set_score(ART_NUM a, int score)
{
    if (is_unavailable(a))      /* newly unavailable */
    {
        return;
    }
    if (g_kill_thresh_active && score <= g_kill_thresh && article_unread(a))
    {
        oneless_artnum(a);
    }

    ARTICLE *ap = article_ptr(a);
    ap->score = score;  /* update the score */
    ap->scoreflags |= SFLAG_SCORED;
    g_s_order_changed = true;   /* resort */
}

/* Hopefully people will write more scoring routines later */
/* This is where you should add hooks for new scoring methods. */
void sc_score_art_basic(ART_NUM a)
{
    int score = 0;
    score += sf_score(a);       /* get a score */

    if (s_sc_do_spin)           /* appropriate to spin */
    {
        spin(20);               /* keep the user amused */
    }
    sc_set_score(a,score);      /* set the score */
}

/* Returns an article's score, scoring it if necessary */
//bool now;     /* if true, sort the scores if necessary... */
int sc_score_art(ART_NUM a, bool now)
{
    if (a < g_absfirst || a > g_lastart)
    {
#if 0
         std::printf("\nsc_score_art: illegal article# %d\n",a);
#endif
        return LOWSCORE;                /* definitely unavailable */
    }
    if (is_unavailable(a))
    {
        return LOWSCORE;
    }

    if (!g_sc_initialized)
    {
        g_sc_delay = false;
        s_sc_sf_force_init = true;
        sc_init(false);
        s_sc_sf_force_init = false;
    }

    if (!article_scored(a))
    {
        if (s_sc_sf_delay)
        {
            sf_init();
            s_sc_sf_delay = false;
        }
        sc_score_art_basic(a);
    }
    if (is_unavailable(a))
    {
        return LOWSCORE;
    }
    return article_ptr(a)->score;
}

/* scores articles in a range */
/* CONSIDER: option for scoring only unread articles (obey sc_fill_unread?) */
void sc_fill_scorelist(ART_NUM first, ART_NUM last)
{
    for (int i = article_first(first); i <= last; i = article_next(i))
    {
        (void)sc_score_art(i,false);    /* will be sorted later... */
    }
}

/* consider having this return a flag (is finished/is not finished) */

/* flag == true means sort now, false means wait until later (not used)
 * nowait == true means start scoring immediately (group entry)
 */
void sc_lookahead(bool flag, bool nowait)
{
    ART_NUM oldart = g_openart;
    ART_POS oldartpos;

    if (!g_sc_initialized)
    {
        return;                 /* no looking ahead now */
    }

#ifdef PENDING
    if (input_pending())
    {
        return;                 /* delay as little as possible */
    }
#endif
    if (!g_sc_initialized)
    {
        return;         /* don't score then... */
    }
    if (oldart)                 /* Was there an article open? */
    {
        oldartpos = tellart();  /* where were we in it? */
    }
#ifndef PENDING
    if (g_int_count)
    {
        g_int_count = 0;                /* clear the interrupt count */
    }
#endif
    /* prevent needless looping below */
    if (g_sc_fill_max < g_firstart && !g_sc_fill_read)
    {
        g_sc_fill_max = article_first(g_firstart)-1;
    }
    else
    {
        g_sc_fill_max = article_first(g_sc_fill_max);
    }
    while (g_sc_fill_max < g_lastart //
#ifdef PENDING
           && !input_pending()
#endif
    )
    {
#ifndef PENDING
        if (g_int_count > 0)
        {
            g_int_count = 0;
            return;     /* user requested break */
        }
#endif
        g_sc_fill_max = article_next(g_sc_fill_max);
        /* skip over some articles quickly */
        while (g_sc_fill_max < g_lastart
         && (article_scored(g_sc_fill_max)
          || (!g_sc_fill_read && !article_unread(g_sc_fill_max))))
        {
            g_sc_fill_max = article_next(g_sc_fill_max);
        }

        if (article_scored(g_sc_fill_max))
        {
            continue;
        }
        if (!g_sc_fill_read)    /* score only unread */
        {
            if (!article_unread(g_sc_fill_max))
            {
                continue;
            }
        }
        (void)sc_score_art(g_sc_fill_max,false);
    }
    if (oldart)                 /* copied from cheat.c */
    {
        artopen(oldart,oldartpos);      /* do not screw the pager */
    }
}

int sc_percent_scored()
{
    if (!g_sc_initialized)
    {
        return 0;       /* none scored */
    }
    if (g_sc_fill_max == g_lastart)
    {
        return 100;
    }
    ART_NUM i = g_firstart;
    if (g_sa_mode_read_elig)
    {
        i = g_absfirst;
    }
    int total = 0;
    int scored = 0;
    for (ART_NUM art = article_first(i); art <= g_lastart; art = article_next(art))
    {
        if (!article_exists(art))
        {
            continue;
        }
        if (!article_unread(art) && !g_sa_mode_read_elig)
        {
            continue;
        }
        total++;
        if (article_scored(art))
        {
            scored++;
        }
    } /* for */
    if (total == 0)
    {
        return 0;
    }
    return (scored*100) / total;
}

void sc_rescore_arts()
{
    if (!g_sc_initialized)
    {
        if (g_sc_delay)
        {
            g_sc_delay = false;
            s_sc_sf_force_init = true;
            sc_init(true);
            s_sc_sf_force_init = false;
        }
    }
    else if (s_sc_sf_delay)
    {
        sf_init();
        s_sc_sf_delay = false;
    }
    if (!g_sc_initialized)
    {
        std::printf("\nScoring is not initialized, aborting command.\n");
        return;
    }
    /* I think s_sc_do_spin will always be false, but why take chances? */
    bool old_spin = s_sc_do_spin;
    setspin(SPIN_FOREGROUND);
    s_sc_do_spin = true;                                /* amuse the user */
    for (ART_NUM a = article_first(g_absfirst); a <= g_lastart; a = article_next(a))
    {
        if (article_exists(a))
        {
            sc_score_art_basic(a);              /* rescore it then */
        }
    }
    s_sc_do_spin = old_spin;
    setspin(SPIN_POP);
    if (g_sa_in)
    {
        g_s_ref_all = true;
        g_s_refill = true;
        g_s_top_ent = 0;                /* make sure the refill starts from top */
    }
}

/* Wrapper to isolate scorefile functions from the rest of the world */
/* corrupted (:-) 11/12/92 by CAA for online rescoring */
void sc_append(char *line)
{
    if (!line)          /* empty line */
    {
        return;
    }

    if (!g_sc_initialized)
    {
        if (g_sc_delay)
        {
            g_sc_delay = false;
            s_sc_sf_force_init = true;
            sc_init(true);
            s_sc_sf_force_init = false;
        }
    }
    else if (s_sc_sf_delay)
    {
        sf_init();
        s_sc_sf_delay = false;
    }
    if (!g_sc_initialized)
    {
        std::printf("\nScoring is not initialized, aborting command.\n");
        return;
    }
    if (!*line)
    {
        line = sc_easy_append();
        if (!line)
        {
            return;             /* do nothing with empty string */
        }
    }
    char filechar = *line; /* first char */
    sf_append(line);
    if (filechar == '!')
    {
        std::printf("\nRescoring articles...");
        std::fflush(stdout);
        sc_rescore_arts();
        std::printf("Done.\n");
        if (g_sa_initialized)
        {
            g_s_top_ent = -1;           /* reset top of page */
        }
    }
}

void sc_rescore()
{
    s_sc_rescoring = true; /* in case routines need to know */
    sc_cleanup();        /* get rid of the old */
    sc_init(true);       /* enter the new... (wait for rescore) */
    if (g_sa_initialized)
    {
        g_s_top_ent = -1;       /* reset top of page */
        g_s_refill = true;      /* make sure a refill is done */
    }
    s_sc_rescoring = false;
}

/* May have a very different interface in the user versions */
void sc_score_cmd(const char *line)
{
    long        i;
    long        j;
    const char* s;

    if (!g_sc_initialized)
    {
        if (g_sc_delay)
        {
            g_sc_delay = false;
            s_sc_sf_force_init = true;
            sc_init(true);
            s_sc_sf_force_init = false;
        }
    }
    else if (s_sc_sf_delay)
    {
        sf_init();
        s_sc_sf_delay = false;
    }
    if (!g_sc_initialized)
    {
        std::printf("\nScoring is not initialized, aborting command.\n");
        return;
    }
    if (!*line)
    {
        line = sc_easy_command();
        if (!line)
        {
            return;             /* do nothing with empty string */
        }
        if (*line == '\"')
        {
            g_buf[0] = '\0';
            sc_append(g_buf);
            return;
        }
    }
    switch (*line)
    {
    case 'f': /* fill (useful when PENDING is unavailable) */
        std::printf("Scoring more articles...");
        std::fflush(stdout); /* print it now */
        setspin(SPIN_FOREGROUND);
        s_sc_do_spin = true;
        sc_lookahead(true,false);
        s_sc_do_spin = false;
        setspin(SPIN_POP);
        /* consider a "done" message later,
         * *if* lookahead did all the arts */
        std::putchar('\n');
        break;

    case 'r': /* rescore */
        std::printf("Rescoring articles...\n");
        sc_rescore();
        break;

    case 's': /* verbose score for this article */
        /* XXX CONSIDER: A VERBOSE-SCORE ROUTINE (instead of this hack) */
        i = 0;  /* total score */
        g_sf_score_verbose = true;
        j = sf_score(g_art);
        g_sf_score_verbose = false;
        std::printf("Scorefile total score: %ld\n",j);
        i += j;
        j = sc_score_art(g_art,true);
        if (i != j)
        {
            /* Consider resubmitting article to filter? */
            std::printf("Other scoring total: %ld\n", j - i);
        }
        std::printf("Total score is %ld\n",i);
        break;

    case 'e': /* edit scorefile or other file */
        s = skip_hor_space(line+1);
        if (!*s)                /* empty name for scorefile */
        {
            sf_edit_file("\""); /* edit local scorefile */
        }
        else
        {
            sf_edit_file(s);
        }
        break;

    default:
        std::printf("Unknown scoring command |%s|\n",line);
    } /* switch */
}

//int thresh;           /* kill all articles with this score or lower */
void sc_kill_threshold(int thresh)
{
    for (ART_NUM a = article_first(g_firstart); a <= g_lastart; a = article_next(a))
    {
        if (article_ptr(a)->score <= thresh &&
            article_unread(a)
            /* CAA 6/19/93: this is needed for zoom mode */
            && sa_basic_elig(sa_artnum_to_ent(a)))
        {
            oneless_artnum(a);
        }
    }
}
