#pragma once

#include "config/config.h"

#ifdef I_UNISTD
#include <unistd.h>

inline int current_user_id()
{
    return getuid();
}
#else
inline int current_user_id()
{
    constexpr int ARBITRARY_USER_ID{2};
    return ARBITRARY_USER_ID;
}
#endif
