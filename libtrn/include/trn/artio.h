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

struct ArticleInput
{
    ArticlePosition ftell_art();

    ArticlePosition pos{};      // byte position in article file
    ArticleLine     line_num{}; // current line number in article file
    std::FILE      *fp{};       // current article file pointer
    ArticleNum      open_art{}; // the article number we have open
};

extern ArticleInput    g_art_input;
extern char            g_wrapped_nl;
extern int             g_word_wrap_offset; // right-hand column size (0 is off)

struct PresentationBuffer
{
    void init();
    void final();
    void clear_art_buf();
    bool empty();
    bool at_end();

    std::string     bytes;
    ArticlePosition position{};
    ArticlePosition seek_pos{};
    ArticlePosition length{};
};

extern PresentationBuffer g_art_buf;

class PreserveArticleBuffer
{
public:
    PreserveArticleBuffer();
    ~PreserveArticleBuffer();
    PreserveArticleBuffer(const PreserveArticleBuffer &) = delete;
    PreserveArticleBuffer &operator=(const PreserveArticleBuffer &) = delete;

private:
    std::string     m_buffer;
    ArticlePosition m_position;
    ArticlePosition m_seek;
    ArticlePosition m_length;
};

std::FILE *art_open(ArticleNum art_num, ArticlePosition pos);
void art_close();
int seek_art(ArticlePosition pos);
ArticlePosition tell_art();
ArticlePosition ftell_art();
bool            read_art(std::string &line);
int             seek_art_buf(ArticlePosition pos);
bool            read_art_buf(std::string &line, bool view_inline);
char            *read_art_buf_raw(bool view_inline);
ArticlePosition art_buf_pos();
ArticlePosition art_buf_len();
ArticlePosition art_buf_seek();
void            set_art_buf_seek(ArticlePosition pos);
ArticlePosition art_buf_article_pos();
ArticlePosition art_buf_size_from_raw(ArticlePosition raw_art_size);
std::string_view art_buf_view(ArticlePosition pos);

#endif
