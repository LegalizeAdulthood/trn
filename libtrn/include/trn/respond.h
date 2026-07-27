/* trn/respond.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RESPOND_H
#define TRN_RESPOND_H

#include <config/typedef.h>

#include <string>
#include <string_view>

extern std::string     g_save_dest;     // value of %b
extern std::string     g_extract_dest;  // value of %E
extern std::string     g_extract_prog;  // value of %e
extern ArticlePosition g_save_from;     // value of %B
extern bool            g_mbox_always;   // -M
extern bool            g_norm_always;   // -N
extern std::string     g_priv_dir;      // private news directory
extern std::string     g_indent_string; // indent for old article embedded in followup

enum SaveResult
{
    SAVE_ABORT = 0,
    SAVE_DONE = 1
};

void       respond_init();
SaveResult save_article(std::string_view command);
SaveResult view_article();
int        cancel_article();
int        supersede_article(std::string_view command);
void       reply(std::string_view command);
void       forward();
void       followup(std::string_view command);

#endif
