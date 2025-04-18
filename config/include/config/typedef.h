/* typedef.h
 */
#ifndef TRN_TYPEDEF_H
#define TRN_TYPEDEF_H

// some important types

using NewsgroupNum = int;        // newsgroup number
using ArticleNum = long;         // article number
using ArticleUnread = long;      // could be short to save space
using ArticlePosition = long;    // char position in article file
using ArticleLine = int;         // line position in article file
using ActivePosition = long;     // char position in active file
using MemorySize = unsigned int; // for passing to malloc
using Uchar = unsigned char;     // more space-efficient
using stat_t = struct stat;

#endif
