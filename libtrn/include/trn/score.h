// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
/* trn/score.h
 *
 */
#ifndef TRN_SCORE_H
#define TRN_SCORE_H

#include <config/typedef.h>

#include <string_view>

// RETHINK LOWSCORE: (especially for 16-bit scores?)
// score given to unavailable articles
enum
{
    LOW_SCORE = -999999
};

struct ScoringSession
{
    void set_score(ArticleNum a, int score);
    void init(bool pend_wait);
    int  score_art(ArticleNum a, bool now);

    // TODO: private
    void score_art_basic(ArticleNum a);

    bool       kill_thresh_active{};
    int        kill_thresh{LOW_SCORE}; // KILL articles at or below this score
    ArticleNum fill_max{};             // maximum art# scored by fill-routine
    bool       fill_read{};            // true if also scoring read arts...
    bool       initialized{};          // has score been initialized (are we "in" scoring?)
    bool       scoring{};              // are we currently scoring an article (prevents loops)
    bool       save_scores{};          // If true, save the scores for this group on exit.
    bool       delay{};                // If true, delay initialization of scoring until explicitly required
    bool       sf_force_init{};        // If true, always sf_init()
    bool       sf_delay{};             // if true, delay loading rule files
    bool       do_spin{};              // actually do the score spinner
    bool       rescoring{};            // are we rescoring now?
};

extern ScoringSession g_scoring;
extern bool       g_score_new_first; // changes order of sorting (artnum comparison) when scores are equal

void sc_cleanup();
void sc_fill_score_list(ArticleNum first, ArticleNum last);
void sc_look_ahead(bool flag, bool nowait);
int  sc_percent_scored();
void sc_append(std::string_view line);
void sc_rescore();
void sc_score_cmd(std::string_view line);
void sc_kill_threshold(int thresh);

#endif
