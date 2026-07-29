/* common.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <config/common.h>

#include <fmt/format.h>

#include <cstdio>
#include <stdexcept>

// GLOBAL THINGS

// Global message text.

std::string g_msg; // general purpose message text

#ifndef NDEBUG
[[noreturn]]
void report_assertion(const char *expr, const char *file, unsigned int line)
{
    fmt::print(stderr, "{}({}): Assertion '{}' failed\n", file, line, expr);
    throw std::runtime_error("assertion failure");
}
#endif
