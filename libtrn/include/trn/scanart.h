/* This file Copyright 1992 by Clifford A. Adams */
/* trn/scanart.h
 *
 * Interface to rest of [t]rn
 */
#ifndef TRN_SCANART_H
#define TRN_SCANART_H

#include "trn/enum-flags.h"

/* return codes for sa_main */
enum sa_main_result
{
    SA_READ = -7,     /* read article pointed to by g_art (always) */
    SA_QUIT_SEL = -6, /* quit, and return to previous selector (backtick) */
    SA_PRIOR = -5,    /* go to and enter prior newsgroup */
    SA_NEXT = -4,     /* go to and enter next newsgroup */
    SA_FAKE = -3,     /* Fake a command (g_buf and g_art already set up) */
    SA_ERR = -2,      /* error, quit out one level */
    SA_QUIT = -1,     /* quit out one level and clean up... */
    SA_NORM = 0       /* do the normal thing (usually read article pointed to by g_art) */
};

/* sa_flags character bitmap:
 * 0: If set, the article is "marked" (for reading).
 * 1: If set, the article is "selected" (for zoom mode display).
 * 2: Secondary selection--not currently used.
 * 3: If set, the author of this article influenced its score.
 */
enum scanart_flags : unsigned char
{
    SAF_NONE = 0,
    SAF_MARK = 0x01,
    SAF_SELECT1 = 0x02,
    SAF_SELECT2 = 0x04,
    SAF_AUTH_SCORED = 0x08
};
DECLARE_FLAGS_ENUM(scanart_flags, unsigned char);

/* per-entry data */
struct SA_ENTRYDATA
{
    ART_NUM       artnum;
    long          subj_thread_num;
    scanart_flags sa_flags; /* status bitmap (marked, select, etc...) */
};

extern SA_ENTRYDATA *g_sa_ents;
extern int g_sa_num_ents;

extern bool g_sa_initialized;       /* Have we initialized? */
extern bool g_sa_never_initialized; /* Have we ever initialized? */

/* note: g_sa_in should be checked for returning to SA */
extern bool g_sa_in; /* Are we "in" SA? */

extern bool g_sa_go;          /* go to sa.  Do not pass GO (:-) */
extern bool g_sa_go_explicit; /* want to bypass read-next-marked */

/* used to pass an article number to read soon */
extern ART_NUM g_sa_art;

/* reimplement later */
/* select threads from TRN thread selector */
extern bool g_sa_do_selthreads;

/* true if read articles are eligible */
/* in trn/scanart.h for world-visibilty */
extern bool g_sa_mode_read_elig;

/* Options */
/* Display order variable:
 *
 * 1: Arrival order
 * 2: Descending score
 */
enum sa_display_order
{
    SA_ORDER_NONE = 0,
    SA_ORDER_ARRIVAL = 1,
    SA_ORDER_DESCENDING = 2,
};
extern sa_display_order g_sa_mode_order;

/* if true, don't move the cursor after marking or selecting articles */
extern bool g_sa_mark_stay;

/* if true, re-"fold" after an un-zoom operation. */
/* This flag is useful for very slow terminals */
extern bool g_sa_unzoomrefold;

/* true if in "fold" mode */
extern bool g_sa_mode_fold;

/* Follow threads by default? */
extern bool g_sa_follow;

/* Options: what to display */
extern bool g_sa_mode_desc_artnum; /* show art#s */
extern bool g_sa_mode_desc_author; /* show author */
extern bool g_sa_mode_desc_score;  /* show score */
/* flags to determine whether to display various things */
extern bool g_sa_mode_desc_threadcount;
extern bool g_sa_mode_desc_subject;
extern bool g_sa_mode_desc_summary;
extern bool g_sa_mode_desc_keyw;

sa_main_result sa_main();
sa_main_result sa_mainloop();
void sa_grow(ART_NUM oldlast, ART_NUM last);
void sa_cleanup();

inline void sa_mark(int a)
{
    g_sa_ents[a].sa_flags |= SAF_MARK;
}
inline void sa_clear_mark(int a)
{
    g_sa_ents[a].sa_flags &= ~SAF_MARK;
}
inline bool sa_marked(int a)
{
    return g_sa_ents[a].sa_flags & SAF_MARK;
}

inline void sa_select1(int a)
{
    g_sa_ents[a].sa_flags |= SAF_SELECT1;
}
inline void sa_clear_select1(int a)
{
    g_sa_ents[a].sa_flags &= ~SAF_SELECT1;
}
inline bool sa_selected1(int a)
{
    return g_sa_ents[a].sa_flags & SAF_SELECT1;
}

/* sa_select2 is not currently used */
inline void sa_select2(int a)
{
    g_sa_ents[a].sa_flags |= SAF_SELECT2;
}
inline void sa_clear_select2(int a)
{
    g_sa_ents[a].sa_flags &= ~SAF_SELECT2;
}
inline bool sa_selected2(int a)
{
    return g_sa_ents[a].sa_flags & SAF_SELECT2;
}

inline void sa_set_auth_scored(int a)
{
    g_sa_ents[a].sa_flags |= SAF_AUTH_SCORED;
}
inline void sa_clear_auth_scored(int a)
{
    g_sa_ents[a].sa_flags &= ~SAF_AUTH_SCORED;
}
inline bool sa_auth_scored(int a)
{
    return g_sa_ents[a].sa_flags & SAF_AUTH_SCORED;
}

#endif
