/* This file Copyright 1992 by Clifford A. Adams */
/* scanart.h
 *
 * Interface to rest of [t]rn
 */
#ifndef TRN_SCANART_H
#define TRN_SCANART_H

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

/* per-entry data */
struct SA_ENTRYDATA
{
    ART_NUM artnum;
    long subj_thread_num;
    char sa_flags;		/* status bitmap (marked, select, etc...) */
};

extern SA_ENTRYDATA *g_sa_ents;
extern int g_sa_num_ents;

extern bool g_sa_initialized;       /* Have we initialized? */
extern bool g_sa_never_initialized; /* Have we ever initialized? */

/* note: g_sa_in should be checked for returning to SA */
extern bool g_sa_in; /* Are we "in" SA? */

extern bool g_sa_go;          /* go to sa.  Do not pass GO (:-) */
extern bool g_sa_go_explicit; /* want to bypass read-next-marked */

extern bool g_sa_context_init; /* has context been initialized? */

/* used to pass an article number to read soon */
extern ART_NUM g_sa_art;

/* reimplement later */
/* select threads from TRN thread selector */
extern bool g_sa_do_selthreads;

/* true if read articles are eligible */
/* in scanart.h for world-visibilty */
extern bool g_sa_mode_read_elig;

/* Options */
/* Display order variable:
 *
 * 1: Arrival order
 * 2: Descending score
 */
extern int g_sa_mode_order;

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

#endif
