/* trn/artio.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ARTIO_H
#define TRN_ARTIO_H

#include <config/typedef.h>

#include <stdio.h>

enum : char
{
    WRAPPED_NL = '\003'
};

inline bool at_nl(char c)
{
    return c == '\n' || c == WRAPPED_NL;
}

extern ART_POS  g_artpos;  /* byte position in article file */
extern ART_LINE g_artline; /* current line number in article file */
extern FILE    *g_artfp;   /* current article file pointer */
extern ART_NUM  g_openart; /* the article number we have open */
extern char    *g_artbuf;
extern long     g_artbuf_pos;
extern long     g_artbuf_seek;
extern long     g_artbuf_len;
extern char     g_wrapped_nl;
extern int      g_word_wrap_offset; /* right-hand column size (0 is off) */

void artio_init();
void artio_final();
FILE *artopen(ART_NUM artnum, ART_NUM pos);
void artclose();
int seekart(ART_POS pos);
ART_POS tellart();
char *readart(char *s, int limit);
void clear_artbuf();
int seekartbuf(ART_POS pos);
char *readartbuf(bool view_inline);

#endif
