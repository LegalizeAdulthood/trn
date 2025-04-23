/* typedef.h
 */
#ifndef TRN_TYPEDEF_H
#define TRN_TYPEDEF_H

#include <strong_type/strong_type.hpp>

// some important types

template <typename Base, typename Tag>
using Number = strong::type<Base, Tag,                     //
                            strong::arithmetic,            //
                            strong::bicrementable,         //
                            strong::ordered,               //
                            strong::equality,              //
                            strong::default_constructible, //
                            strong::boolean,               //
                            strong::ordered_with<Base>,    //
                            strong::equality_with<Base>    //
                            >;

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

// could be short to save space
using ArticleUnread = long;

// char position in article file
using ArticlePosition = Number<long, struct ArticlePositionTag>;

// line position in article file
using ArticleLine = Number<int, struct ArticleLineTag>;

inline ArticleLine line_after(ArticleLine value)
{
    return value + ArticleLine{1};
}

inline ArticleLine line_before(ArticleLine value)
{
    return value - ArticleLine{1};
}

// char position in active file
using ActivePosition = Number<long, struct ActivePositionTag>;

using MemorySize = unsigned int; // for passing to malloc
using Uchar = unsigned char;     // more space-efficient
using stat_t = struct stat;

#endif
