/* This file Copyright 1992 by Clifford A. Adams */
/* score.h
 *
 */

/* RETHINK LOWSCORE: (especially for 16-bit scores?) */
/* score given to unavailable articles */
#define LOWSCORE (-999999)

/* specific scoreflag meanings:  (note: bad placement, but where else?) */
/* author has a score (match on FROM: line) */
#define SFLAG_AUTHOR 1
/* if true, the article has been scored */
#define SFLAG_SCORED 16
#define SCORED(a) (article_ptr(a)->scoreflags & SFLAG_SCORED)

EXT bool kill_thresh_active INIT(false);
EXT int kill_thresh INIT(LOWSCORE);   /* KILL articles at or below this score */

EXT ART_NUM sc_fill_max;	   /* maximum art# scored by fill-routine */
EXT bool sc_fill_read INIT(false); /* true if also scoring read arts... */

/* has score been initialized (are we "in" scoring?) */
EXT bool sc_initialized INIT(false);

/* are we currently scoring an article (prevents loops) */
EXT bool sc_scoring INIT(false);

/* changes order of sorting (artnum comparison) when scores are equal */
EXT bool score_newfirst INIT(false);

/* if nice background available, use it */
EXT bool sc_mode_nicebg INIT(true);

/* If true, save the scores for this group on exit. */
EXT bool sc_savescores INIT(false);

/* If true, delay initialization of scoring until explicitly required */
EXT bool sc_delay INIT(false);

EXT bool sc_rescoring INIT(false);	/* are we rescoring now? */

EXT bool sc_do_spin INIT(false);	/* actually do the score spinner */

EXT bool sc_sf_delay INIT(false);	/* if true, delay loading rule files */
EXT bool sc_sf_force_init INIT(false);	/* If true, always sf_init() */

void sc_init(bool pend_wait);
void sc_cleanup(void);
void sc_set_score(ART_NUM, int);
void sc_score_art_basic(ART_NUM);
int sc_score_art(ART_NUM a, bool now);
void sc_fill_scorelist(ART_NUM, ART_NUM);
void sc_lookahead(bool flag, bool nowait);
int sc_percent_scored(void);
void sc_rescore_arts(void);
void sc_append(char *);
void sc_rescore(void);
void sc_score_cmd(char *);
void sc_kill_threshold(int);
