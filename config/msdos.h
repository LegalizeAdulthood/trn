/* msdos.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_MSDOS_H
#define TRN_MSDOS_H

#include <ctype.h>
#include <stdio.h>

inline bool file_ref(const char *s)
{
    return s[0] == '/' || (isalpha(s[0]) && s[1] == ':');
}

#define FILE_REF(s) file_ref(s)

#define FOPEN_RB "rb"
#define FOPEN_WB "wb"

#define B19200  19200
#define B9600   9600
#define B4800   4800
#define B2400   2400
#define B1800   1800
#define B1200   1200
#define B600    600
#define B300    300
#define B200    200
#define B150    150
#define B134    134
#define B110    110
#define B75     75
#define B50     50

#define RESTORE_ORIGDIR
#define NO_FILELINKS
#define LAX_INEWS

#define sleep(secs_) _sleep(secs_)

#endif
