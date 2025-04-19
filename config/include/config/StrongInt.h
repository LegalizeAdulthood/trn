#pragma once

template <typename T>
struct StrongInt
{
    StrongInt() = default;
    StrongInt(const StrongInt &rhs) = default;
    StrongInt(StrongInt &&rhs) = default;
    StrongInt &operator=(const StrongInt &rhs) = default;
    StrongInt &operator=(StrongInt &&rhs) = default;

    explicit operator bool() const
    {
        return static_cast<bool>(num);
    }

    T num{};
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
template <typename T, typename U = T>
StrongInt<T> operator+(StrongInt<T> lhs, StrongInt<U> rhs)
{
    return {lhs.num + rhs.num};
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
