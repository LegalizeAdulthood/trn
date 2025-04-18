/* trn/artio.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_ARTIO_H
#define TRN_ARTIO_H

#include <config/typedef.h>

#include <cstdio>

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
extern char           *g_art_buf;
extern long            g_art_buf_pos;
extern long            g_art_buf_seek;
extern long            g_art_buf_len;
extern char            g_wrapped_nl;
extern int             g_word_wrap_offset; // right-hand column size (0 is off)

void art_io_init();
void art_io_final();
std::FILE *art_open(ArticleNum art_num, ArticleNum pos);
void art_close();
int seek_art(ArticlePosition pos);
ArticlePosition tell_art();
char *read_art(char *s, int limit);
void clear_art_buf();
int seek_art_buf(ArticlePosition pos);
char *read_art_buf(bool view_inline);

#endif
