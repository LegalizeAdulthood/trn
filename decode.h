/* decode.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef DECODE_H
#define DECODE_H

extern char *g_decode_filename;

enum decode_state
{
    DECODE_DONE = 0,
    DECODE_START = 1,
    DECODE_INACTIVE = 2,
    DECODE_SETLEN = 3,
    DECODE_ACTIVE = 4,
    DECODE_NEXT2LAST = 5,
    DECODE_LAST = 6,
    DECODE_MAYBEDONE = 7,
    DECODE_ERROR = 8
};

using DECODE_FUNC = decode_state (*)(FILE *ifp, decode_state state);

void decode_init();
char *decode_fix_fname(const char *s);
char *decode_subject(ART_NUM artnum, int *partp, int *totalp);
int decode_piece(MIMECAP_ENTRY *mcp, char *first_line);
DECODE_FUNC decode_function(int encoding);
char *decode_mkdir(const char *filename);
void decode_rmdir(char *dir);

#endif
