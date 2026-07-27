/* trn/ngstuff-internal.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NGSTUFF_INTERNAL_H
#define TRN_NGSTUFF_INTERNAL_H

#include <trn/ngstuff.h>

#include <functional>

// Internal entry points for testing purposes.

using NgstuffShellRunner = std::function<int(std::string_view shell, std::string_view cmd)>;
bool escapade_with_shell_runner(const NgstuffShellRunner &shell_runner);
bool escapade_with_shell_runner(const NgstuffShellRunner &shell_runner, std::string_view command);

#endif
