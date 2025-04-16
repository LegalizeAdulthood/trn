/* trn/decode.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_DECODE_H
#define TRN_DECODE_H

#include <config/typedef.h>

#include <cstdio>

struct MIMECAP_ENTRY;

extern char *g_decode_filename;

enum MimeEncoding
{
    MENCODE_NONE = 0,
    MENCODE_BASE64 = 1,
    MENCODE_QPRINT = 2,
    MENCODE_UUE = 3,
    MENCODE_UNHANDLED = 4
};

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

using DECODE_FUNC = decode_state (*)(std::FILE *ifp, decode_state state);

void decode_init();
char *decode_fix_fname(const char *s);
char *decode_subject(ART_NUM artnum, int *partp, int *totalp);
bool decode_piece(MIMECAP_ENTRY *mcp, char *first_line);
DECODE_FUNC decode_function(MimeEncoding encoding);
char *decode_mkdir(const char *filename);
void decode_rmdir(char *dir);

#endif
