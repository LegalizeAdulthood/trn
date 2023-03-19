/* artio.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT ART_POS artpos INIT(0);	/* byte position in article file */

EXT ART_LINE artline INIT(0);	/* current line number in article file */
EXT FILE* artfp INIT(nullptr);	/* current article file pointer */
EXT ART_NUM openart INIT(0);	/* the article number we have open */

EXT char* artbuf;
EXT long artbuf_size;
EXT long artbuf_pos;
EXT long artbuf_seek;
EXT long artbuf_len;

#define WRAPPED_NL  '\003'
#define AT_NL(c) ((c) == '\n' || (c) == WRAPPED_NL)

EXT char wrapped_nl INIT(WRAPPED_NL);

#ifdef LINKART
EXT char* linkartname INIT("");/* real name of article for Eunice */
#endif

void artio_init();
FILE *artopen(ART_NUM, ART_POS);
void artclose();
int seekart(ART_POS);
ART_POS tellart();
char *readart(char *s, int limit);
void clear_artbuf();
int seekartbuf(ART_POS);
char *readartbuf(bool view_inline);
