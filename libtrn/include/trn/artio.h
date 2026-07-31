/* trn/artio.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ARTIO_H
#define TRN_ARTIO_H

#include <config/typedef.h>

#include <cstdio>
#include <string>
#include <string_view>

enum : char
{
    WRAPPED_NL = '\003'
};

inline bool at_nl(char c)
{
    return c == '\n' || c == WRAPPED_NL;
}

extern ArticlePosition g_art_pos;      // byte position in article file
extern ArticleLine     g_art_line_num; // current line number in article file
extern std::FILE      *g_art_fp;       // current article file pointer
extern ArticleNum      g_open_art;     // the article number we have open
extern char            g_wrapped_nl;
extern int             g_word_wrap_offset; // right-hand column size (0 is off)

void art_io_init();
void art_io_final();
std::FILE *art_open(ArticleNum art_num, ArticlePosition pos);
void art_close();
int seek_art(ArticlePosition pos);
ArticlePosition tell_art();
ArticlePosition ftell_art();
bool            read_art(std::string &line);
void            clear_art_buf();
int             seek_art_buf(ArticlePosition pos);
bool            read_art_buf(std::string &line, bool view_inline);
bool            art_buf_empty();
bool            art_buf_at_end();
ArticlePosition art_buf_pos();
ArticlePosition art_buf_len();
ArticlePosition art_buf_seek();
void            set_art_buf_seek(ArticlePosition pos);
ArticlePosition art_buf_article_pos();
ArticlePosition art_buf_size_from_raw(ArticlePosition raw_art_size);
std::string_view art_buf_view(ArticlePosition pos);

#endif
