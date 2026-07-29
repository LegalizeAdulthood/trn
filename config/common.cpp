/* common.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <config/common.h>

#include <fmt/format.h>

#include <cstdio>
#include <stdexcept>

// GLOBAL THINGS

// various things of type char

std::string g_msg;                   // general purpose message text
char        g_buf[LINE_BUF_LEN + 1]; // general purpose line buffer

#ifndef NDEBUG
[[noreturn]]
void report_assertion(const char *expr, const char *file, unsigned int line)
{
    fmt::print(stderr, "{}({}): Assertion '{}' failed\n", file, line, expr);
    throw std::runtime_error("assertion failure");
}
#endif
