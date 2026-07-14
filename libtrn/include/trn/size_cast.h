// Copyright (c) 2026, Richard Thomson

#pragma once

#include <vector>

template <typename T, typename U>
T size_cast(const std::vector<U> &vec)
{
    return static_cast<T>(vec.size());
}
