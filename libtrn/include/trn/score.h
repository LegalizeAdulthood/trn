// This file Copyright 1992 by Clifford A. Adams
/* trn/score.h
 *
 */
#ifndef TRN_SCORE_H
#define TRN_SCORE_H

#include <config/typedef.h>

// RETHINK LOWSCORE: (especially for 16-bit scores?)
// score given to unavailable articles
enum
{
    LOW_SCORE = -999999
};

extern bool       g_kill_thresh_active;
extern int        g_kill_thresh;     // KILL articles at or below this score
extern ArticleNum g_sc_fill_max;     // maximum art# scored by fill-routine
extern bool       g_sc_fill_read;    // true if also scoring read arts...
extern bool       g_sc_initialized;  // has score been initialized (are we "in" scoring?)
extern bool       g_sc_scoring;      // are we currently scoring an article (prevents loops)
extern bool       g_score_new_first; // changes order of sorting (artnum comparison) when scores are equal
extern bool       g_sc_saves_cores;  // If true, save the scores for this group on exit.
extern bool       g_sc_delay;        // If true, delay initialization of scoring until explicitly required

void sc_init(bool pend_wait);
void sc_cleanup();
void sc_set_score(ArticleNum a, int score);
void sc_score_art_basic(ArticleNum a);
int  sc_score_art(ArticleNum a, bool now);
void sc_fill_score_list(ArticleNum first, ArticleNum last);
void sc_look_ahead(bool flag, bool nowait);
int  sc_percent_scored();
void sc_rescore_arts();
void sc_append(char *line);
void sc_rescore();
void sc_score_cmd(const char *line);
void sc_kill_threshold(int thresh);

#endif
