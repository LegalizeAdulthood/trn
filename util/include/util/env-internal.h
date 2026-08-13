// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_ENV_INTERNAL_H
#define TRN_ENV_INTERNAL_H

#include <util/env.h>

// Internal entry points exposed for the purposes of unit testing.

#include <functional>
#include <string>
#include <string_view>

enum class PasswdNameFormat
{
    Berkeley,
    Usg
};

std::string passwd_full_name(std::string_view gecos, std::string_view login_name, PasswdNameFormat format);

bool env_init(bool lax, const std::function<bool()> &set_user_name_fn, const std::function<bool()> &set_host_name_fn);

void set_environment(std::function<char *(const char *)> getenv_fn);

#endif
