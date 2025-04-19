/* typedef.h
 */
#ifndef TRN_TYPEDEF_H
#define TRN_TYPEDEF_H

// some important types

using NewsgroupNum = int;        // newsgroup number

// article number
struct ArticleNum
{
    long num{};

    explicit operator bool() const
    {
        return static_cast<bool>(num);
    }
};
inline bool operator==(ArticleNum lhs, ArticleNum rhs)
{
    return lhs.num == rhs.num;
}
inline bool operator!=(ArticleNum lhs, ArticleNum rhs)
{
    return lhs.num != rhs.num;
}
inline bool operator<(ArticleNum lhs, ArticleNum rhs)
{
    return lhs.num < rhs.num;
}
inline bool operator>(ArticleNum lhs, ArticleNum rhs)
{
    return lhs.num > rhs.num;
}
inline bool operator<=(ArticleNum lhs, ArticleNum rhs)
{
    return lhs.num <= rhs.num;
}
inline bool operator>=(ArticleNum lhs, ArticleNum rhs)
{
    return lhs.num >= rhs.num;
}

inline ArticleNum operator+(ArticleNum lhs, long rhs)
{
    return ArticleNum{lhs.num + rhs};
}
inline ArticleNum operator-(ArticleNum lhs, long rhs)
{
    return ArticleNum{lhs.num - rhs};
}
inline ArticleNum operator-(ArticleNum lhs, ArticleNum rhs)
{
    return ArticleNum{lhs.num - rhs.num};
}
inline ArticleNum &operator+=(ArticleNum &lhs, ArticleNum rhs)
{
    lhs.num += rhs.num;
    return lhs;
}
inline ArticleNum &operator-=(ArticleNum &lhs, ArticleNum rhs)
{
    lhs.num -= rhs.num;
    return lhs;
}

using ArticleUnread = long;      // could be short to save space
using ArticlePosition = long;    // char position in article file
using ArticleLine = int;         // line position in article file
using ActivePosition = long;     // char position in active file
using MemorySize = unsigned int; // for passing to malloc
using Uchar = unsigned char;     // more space-efficient
using stat_t = struct stat;

#endif
