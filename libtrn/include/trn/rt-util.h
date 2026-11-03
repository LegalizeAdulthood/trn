/* trn/rt-util.h
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_UTIL_H
#define TRN_RT_UTIL_H

#include <config/typedef.h>

#include <string>
#include <string_view>

struct Article;

extern bool g_performed_article_loop; //
extern bool g_bkgnd_spinner;          // -B
extern bool g_unbroken_subjects;      // -u

enum SpinMode
{
    SPIN_OFF = 0,
    SPIN_POP,
    SPIN_FOREGROUND,
    SPIN_BACKGROUND,
    SPIN_BAR_GRAPH
};

struct ProgressDisplay
{
    bool in_background();
    void set_spin(SpinMode mode);
    void spin(int count);

    char             mark{' '};  // char to put back when we're done spinning
    long             estimate{}; // best guess of how much work there is
    long             todo{};     // the max word to do (might decrease)
    int              count{};    // counter for when to spin
    int              marks{25};  // how many bargraph marks we want
    std::string_view chars;
    int              level{}; // used to allow non-interfering nested spins
    SpinMode         mode{};
    int              place{}; // represents place in chars
    int              pos{};   // the last spinbar position we drew
    ArticleNum       art{};
    ArticlePosition  tell{};
};

extern ProgressDisplay g_progress;

std::string_view extract_name(std::string_view name);
std::string compress_name(std::string_view name, int max);
std::string compress_from(std::string_view from, int size);
bool        strip_one_re(std::string_view subject, std::string_view &remaining);
bool        subject_has_re(std::string_view subject, std::string_view &remaining);
bool        subject_has_re(std::string_view subject);
std::string compress_subj(const Article *ap, int max);
void perform_status_init(int cnt);
void perform_status(int cnt, int spin);
int perform_status_end(long cnt, std::string_view obj_type);

#endif
