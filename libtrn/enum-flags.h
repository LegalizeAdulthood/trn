#ifndef TRN_ENUM_FLAGS_H
#define TRN_ENUM_FLAGS_H

template <typename T, typename Base>
T operator_or(T lhs, T rhs)
{
    return static_cast<T>(static_cast<Base>(lhs) | static_cast<Base>(rhs));
}

template <typename T, typename Base>
T operator_and(T lhs, T rhs)
{
    return static_cast<T>(static_cast<Base>(lhs) & static_cast<Base>(rhs));
}

template <typename T, typename Base>
inline T operator_xor(T lhs, T rhs)
{
    return static_cast<T>(static_cast<Base>(lhs) ^ static_cast<Base>(rhs));
}

template <typename T, typename Base>
inline T operator_not(T val)
{
    return static_cast<T>(~static_cast<Base>(val));
}

template <typename T, typename Base>
inline T &operator_or_equal(T &lhs, T rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename T, typename Base>
inline T &operator_and_equal(T &lhs, T rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename T, typename Base>
inline T &operator_xor_equal(T &lhs, T rhs)
{
    lhs = lhs ^ rhs;
    return lhs;
}

#define DECLARE_ENUM_BINOP(Enum_, Base_, op_, fn_) \
    inline Enum_ op_(Enum_ lhs, Enum_ rhs)         \
    {                                              \
        return fn_<Enum_, Base_>(lhs, rhs);        \
    }

#define DECLARE_ENUM_UNOP(Enum_, Base_, op_, fn_) \
    inline Enum_ op_(Enum_ val)                   \
    {                                             \
        return fn_<Enum_, Base_>(val);            \
    }

#define DECLARE_ENUM_BINOP_EQUAL(Enum_, Base_, op_, fn_) \
    inline Enum_ &op_(Enum_ &lhs, Enum_ rhs)             \
    {                                                    \
        return fn_<Enum_, Base_>(lhs, rhs);              \
    }

#define DECLARE_FLAGS_ENUM(Enum_, Base_)                                   \
    DECLARE_ENUM_BINOP(Enum_, Base_, operator|, operator_or)               \
    DECLARE_ENUM_BINOP(Enum_, Base_, operator&, operator_and)              \
    DECLARE_ENUM_BINOP(Enum_, Base_, operator^, operator_xor)              \
    DECLARE_ENUM_UNOP(Enum_, Base_, operator~, operator_not)               \
    DECLARE_ENUM_BINOP_EQUAL(Enum_, Base_, operator|=, operator_or_equal)  \
    DECLARE_ENUM_BINOP_EQUAL(Enum_, Base_, operator&=, operator_and_equal) \
    DECLARE_ENUM_BINOP_EQUAL(Enum_, Base_, operator^=, operator_xor_equal)

#endif
