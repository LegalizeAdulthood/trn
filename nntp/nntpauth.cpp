/* nntpauth.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <nntp/nntpauth.h>

#include <config/common.h>
#include <nntp/nntpclient.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <string>

int nntp_handle_auth_err()
{
    // save previous command
    const std::string last_command_save{g_last_command};

    {
        const char *auth_user = get_auth_user();
        const char *auth_pass = get_auth_pass();
        if (!auth_user || !auth_pass)
        {
            return -2;
        }
        if (nntp_command(fmt::format("AUTHINFO USER {}", auth_user)) <= 0 || nntp_check() <= 0)
        {
            return -2;
        }
        if (nntp_command(fmt::format("AUTHINFO PASS {}", auth_pass)) <= 0 || nntp_check() <= 0)
        {
            return -2;
        }
    }

    if (nntp_command(last_command_save) <= 0)
    {
        return -2;
    }

    return 1;
}
