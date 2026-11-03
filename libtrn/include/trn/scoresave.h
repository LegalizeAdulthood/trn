/* trn/scoresave.h
 *
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCORESAVE_H
#define TRN_SCORESAVE_H

#include <config/typedef.h>

#include <string>
#include <string_view>
#include <vector>

struct SavedScoreStore
{
    void load();
    void save();

    // TODO: private
    void add(std::string_view str);
    void del_group(std::string_view gname);
    void get_file();
    ArticleNum use_line(std::string_view line, ArticleNum a);
    ArticleNum make_line(ArticleNum a);

    std::vector<std::string> lines;
    int                      loaded{};
    int                      used{};
    int                      saved{};
    ArticleNum               last{};
    long                     save_new{};     // new articles (unloaded)
    int                      loaded_count{}; // how many articles were loaded?
};

extern SavedScoreStore g_saved_scores;

void       sc_sv_save_file();

#endif
