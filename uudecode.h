/* uudecode.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef UUDECODE_H
#define UUDECODE_H

/* Length of a normal uuencoded line, including newline */
enum
{
    UULENGTH = 62
};

int uue_prescan(char *bp, char **filenamep, int *partp, int *totalp);
decode_state uudecode(FILE *ifp, decode_state state);

#endif
