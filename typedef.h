/* typedef.h
 */
#ifndef TYPEDEF_H
#define TYPEDEF_H

/* some important types */

using NG_NUM = int;            /* newsgroup number */
using ART_NUM = long;          /* article number */
using ART_UNREAD = long;       /* could be short to save space */
using ART_POS = long;          /* char position in article file */
using ART_LINE = int;          /* line position in article file */
using ACT_POS = long;          /* char position in active file */
using MEM_SIZE = unsigned int; /* for passing to malloc */
using Uchar = unsigned char;   /* more space-efficient */

#endif
