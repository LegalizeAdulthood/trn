/* typedef.h
 */
#ifndef TRN_TYPEDEF_H
#define TRN_TYPEDEF_H

#include <strong_type/strong_type.hpp>

// some important types

template <typename Base, typename Tag>
using Number = strong::type<Base, Tag, strong::arithmetic, strong::bicrementable, strong::ordered, strong::equality,
                            strong::default_constructible, strong::boolean>;

// newsgroup number
using NewsgroupNum  = Number<int, struct NewsgroupNumTag>;

inline NewsgroupNum newsgroup_after(NewsgroupNum value, long increment = 1)
{
    return value + NewsgroupNum{increment};
}

inline NewsgroupNum newsgroup_before(NewsgroupNum value, long increment = 1)
{
    return value - NewsgroupNum{increment};
}

// article number
using ArticleNum = Number<long, struct ArticleNumTag>;

inline ArticleNum article_after(ArticleNum value, long increment = 1)
{
    return value + ArticleNum{increment};
}

inline ArticleNum article_before(ArticleNum value, long increment = 1)
{
    return value - ArticleNum{increment};
}

using ArticleUnread = long;      // could be short to save space
using ArticlePosition = long;    // char position in article file
using ArticleLine = int;         // line position in article file
using ActivePosition = long;     // char position in active file
using MemorySize = unsigned int; // for passing to malloc
using Uchar = unsigned char;     // more space-efficient
using stat_t = struct stat;

#endif
