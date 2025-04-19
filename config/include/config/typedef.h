/* typedef.h
 */
#ifndef TRN_TYPEDEF_H
#define TRN_TYPEDEF_H

// some important types

template <typename T>
struct StrongInt
{
    T num{};


    explicit operator bool() const
    {
        return static_cast<bool>(num);
    }
};
template <typename T>
bool operator==(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return lhs.num == rhs.num;
}
template <typename T>
bool operator!=(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return lhs.num != rhs.num;
}
template <typename T>
bool operator<(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return lhs.num < rhs.num;
}
template <typename T>
bool operator>(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return lhs.num > rhs.num;
}
template <typename T>
bool operator<=(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return lhs.num <= rhs.num;
}
template <typename T>
bool operator>=(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return lhs.num >= rhs.num;
}
template <typename T, typename U>
StrongInt<T> operator+(StrongInt<T> lhs, U rhs)
{
    return StrongInt<T>{lhs.num + rhs};
}
template <typename T, typename U>
StrongInt<T> operator-(StrongInt<T> lhs, U rhs)
{
    return StrongInt<T>{lhs.num - rhs};
}
template <typename T>
StrongInt<T> operator-(StrongInt<T> lhs, StrongInt<T> rhs)
{
    return StrongInt<T>{lhs.num - rhs.num};
}
template <typename T>
StrongInt<T> &operator+=(StrongInt<T> &lhs, StrongInt<T> rhs)
{
    lhs.num += rhs.num;
    return lhs;
}
template <typename T>
StrongInt<T> &operator-=(StrongInt<T> &lhs, StrongInt<T> rhs)
{
    lhs.num -= rhs.num;
    return lhs;
}

struct NewsgroupNum : StrongInt<int>
{
};

// article number
struct ArticleNum : StrongInt<long>
{
};

using ArticleUnread = long;      // could be short to save space
using ArticlePosition = long;    // char position in article file
using ArticleLine = int;         // line position in article file
using ActivePosition = long;     // char position in active file
using MemorySize = unsigned int; // for passing to malloc
using Uchar = unsigned char;     // more space-efficient
using stat_t = struct stat;

#endif
