// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_ENV_INTERNAL_H
#define TRN_ENV_INTERNAL_H

#include <util/env.h>

// Internal entry points exposed for the purposes of unit testing.

#include <functional>

bool env_init(bool lax, const std::function<bool()> &set_user_name_fn, const std::function<bool()> &set_host_name_fn);

void set_environment(std::function<char *(const char *)> getenv_fn);

#endif
