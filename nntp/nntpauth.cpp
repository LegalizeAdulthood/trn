/* nntpauth.c
*/
// This software is copyrighted as detailed in the LICENSE file.

#include "nntp/nntpauth.h"

#include "nntp/nntpclient.h"

#include <config/common.h>
#include <trn/util.h>

#include <cstdio>
#include <cstring>

int nntp_handle_auth_err()
{
    char last_command_save[NNTP_STRLEN];

    // save previous command
    std::strcpy(last_command_save, g_last_command);

    {
        char* auth_user = get_auth_user();
        char* auth_pass = get_auth_pass();
        if (!auth_user || !auth_pass)
        {
            return -2;
        }
        std::sprintf(g_ser_line, "AUTHINFO USER %s", auth_user);
        if (nntp_command(g_ser_line) <= 0 || nntp_check() <= 0)
        {
            return -2;
        }
        std::sprintf(g_ser_line, "AUTHINFO PASS %s", auth_pass);
        if (nntp_command(g_ser_line) <= 0 || nntp_check() <= 0)
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
