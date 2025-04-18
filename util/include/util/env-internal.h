#ifndef TRN_ENV_INTERNAL_H
#define TRN_ENV_INTERNAL_H

#include "util/env.h"

// Internal entry points exposed for the purposes of unit testing.

#include <functional>

bool  env_init(char *tcbuf, bool lax, const std::function<bool(char *tmpbuf)> &set_user_name_fn,
               const std::function<bool(char *tmpbuf)> &set_host_name_fn);

void set_environment(std::function<char *(const char *)> getenv_fn);

#endif
