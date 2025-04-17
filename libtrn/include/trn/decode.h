/* trn/decode.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_DECODE_H
#define TRN_DECODE_H

#include <config/typedef.h>

#include <cstdio>

struct MimeCapEntry;

extern char *g_decode_filename;

enum MimeEncoding
{
    MENCODE_NONE = 0,
    MENCODE_BASE64 = 1,
    MENCODE_QPRINT = 2,
    MENCODE_UUE = 3,
    MENCODE_UNHANDLED = 4
};

enum DecodeState
{
    DECODE_DONE = 0,
    DECODE_START = 1,
    DECODE_INACTIVE = 2,
    DECODE_SET_LEN = 3,
    DECODE_ACTIVE = 4,
    DECODE_NEXT_TO_LAST = 5,
    DECODE_LAST = 6,
    DECODE_MAYBE_DONE = 7,
    DECODE_ERROR = 8
};

using DecodeFunc = DecodeState (*)(std::FILE *ifp, DecodeState state);

void decode_init();
char *decode_fix_filename(const char *s);
char *decode_subject(ArticleNum art_num, int *partp, int *totalp);
bool decode_piece(MimeCapEntry *mcp, char *first_line);
DecodeFunc decode_function(MimeEncoding encoding);
char *decode_mkdir(const char *filename);
void decode_rmdir(char *dir);

#endif
