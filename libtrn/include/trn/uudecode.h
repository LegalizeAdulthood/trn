/* trn/uudecode.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_UUDECODE_H
#define TRN_UUDECODE_H

#include "trn/decode.h"

#include <cstdio>

/* Length of a normal uuencoded line, including newline */
enum
{
    UU_LENGTH = 62
};

int uue_prescan(char *bp, char **filenamep, int *partp, int *totalp);
DecodeState uudecode(std::FILE *ifp, DecodeState state);

#endif
