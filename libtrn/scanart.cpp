/* This file is Copyright 1992, 1993 by Clifford A. Adams */
/* scanart.c
 *
 * article scan mode: screen oriented article interface
 * based loosely on the older "SubScan" mode.
 * Interface routines to the rest of trn
 */

#include "config/common.h"
#include "trn/scanart.h"

#include "trn/ngdata.h"
#include "trn/artstate.h"  /* for g_reread */
#include "trn/ng.h"        /* variable g_art, the next article to read. */
#include "trn/rt-select.h" /* g_selected_only */
#include "trn/samain.h"
#include "trn/samisc.h"
#include "trn/scan.h"
#include "trn/smisc.h"
#include "trn/terminal.h" /* macro to clear... */

ScanArticleEntryData *g_sa_ents{};
int           g_sa_num_ents{};
bool          g_sa_initialized{};           /* Have we initialized? */
bool          g_sa_never_initialized{true}; /* Have we ever initialized? */
/* note: g_sa_in should be checked for returning to SA */
bool    g_sa_in{};             /* Are we "in" SA? */
bool    g_sa_go{};             /* go to sa.  Do not pass GO (:-) */
bool    g_sa_go_explicit{};    /* want to bypass read-next-marked */
ART_NUM g_sa_art{};            /* used to pass an article number to read soon */
bool    g_sa_do_selthreads{};  /* select threads from TRN thread selector; re-implement later */
bool    g_sa_mode_read_elig{}; /* true if read articles are eligible in trn/scanart.h for world-visibilty */

/* Options */
/* Display order variable:
 *
 * 1: Arrival order
 * 2: Descending score
 */
sa_display_order g_sa_mode_order{SA_ORDER_DESCENDING};

/* if true, don't move the cursor after marking or selecting articles */
bool g_sa_mark_stay{};

/* if true, re-"fold" after an un-zoom operation. */
/* This flag is useful for very slow terminals */
bool g_sa_unzoomrefold{};

/* true if in "fold" mode */
bool g_sa_mode_fold{};

/* Follow threads by default? */
bool g_sa_follow{true};

/* Options: what to display */
bool g_sa_mode_desc_artnum{};     /* show art#s */
bool g_sa_mode_desc_author{true}; /* show author */
bool g_sa_mode_desc_score{true};  /* show score */
/* flags to determine whether to display various things */
bool g_sa_mode_desc_threadcount{};
bool g_sa_mode_desc_subject{true};
bool g_sa_mode_desc_summary{};
bool g_sa_mode_desc_keyw{};

SaMainResult sa_main()
{
    g_sa_in = true;
    g_sa_go = false;    /* ...do not collect $200... */
    g_s_follow_temp = false;

    if (g_lastart < g_absfirst)
    {
        s_beep();
        return SA_QUIT;
    }
    if (!g_sa_initialized)
    {
        sa_init();
        if (!g_sa_initialized)          /* still not working... */
        {
            return SA_ERR;              /* we don't belong here */
        }
        g_sa_never_initialized = false; /* we have entered at least once */
    }
    else
    {
        s_change_context(g_sa_scan_context);
    }

    /* unless "explicit" entry, read any marked articles */
    if (!g_sa_go_explicit)
    {
        long a = sa_readmarked_elig();
        if (a)          /* there was an article */
        {
            g_art = g_sa_ents[a].artnum;
            g_reread = true;
            sa_clear_mark(a);
            /* trn 3.x won't read an unselected article if g_selected_only */
            g_selected_only = false;
            s_save_context();
            return SA_NORM;
        }
    }
    g_sa_go_explicit = false;

    /* If called from the trn thread-selector and articles/threads were
     * selected there, "select" the articles and enter the zoom mode.
     */
    if (g_sa_do_selthreads)
    {
        sa_selthreads();
        g_sa_do_selthreads = false;
        g_sa_mode_zoom = true;          /* zoom in by default */
        g_s_top_ent = -1;               /* go to top of arts... */
    }

    MinorMode sa_oldmode = g_mode; /* save mode */
    g_mode = MM_S;             /* for RN macros */
    SaMainResult i = sa_mainloop();
    g_mode = sa_oldmode;                        /* restore mode */

    if (i == SA_NORM || i == SA_FAKE)
    {
        g_art = g_sa_art;
        /* trn 3.x won't read an unselected article if g_selected_only */
        g_selected_only = false;
        if (g_sa_mode_read_elig)
        {
            g_reread = true;
        }
    }
    if (g_sa_scan_context >= 0)
    {
        s_save_context();
    }
    return i;
}

/* called when more articles arrive */
void sa_grow(ART_NUM oldlast, ART_NUM last)
{
    if (!g_sa_initialized)
    {
        return;
    }
    sa_growarts(oldlast,last);
}

void sa_cleanup()
{
    /* we might be called by other routines which aren't sure
     * about the scan status
     */
    if (!g_sa_initialized)
    {
        return;
    }

    sa_cleanmain();
    clear();            /* should something else clear the screen? */
    g_sa_initialized = false;           /* goodbye... */
}
