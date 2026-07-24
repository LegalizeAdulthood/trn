/* msdos.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_MSDOS_H
#define TRN_MSDOS_H

#include <cctype>
#include <stdio.h>
#include <string_view>

inline bool file_ref(std::string_view s)
{
    return !s.empty() &&
           (s.front() == '/' || (s.size() > 1 && std::isalpha(static_cast<unsigned char>(s.front())) && s[1] == ':'));
}

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
#define LAX_INEWS

#define sleep(secs_) _sleep(secs_)

#endif
