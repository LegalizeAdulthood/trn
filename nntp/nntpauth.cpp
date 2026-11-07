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

namespace
{

constexpr int AUTH_ACCEPTED = 281;
constexpr int PASSWORD_REQUIRED = 381;

} // namespace

NNTPLink::AuthMethod NNTPLink::authentication_method()
{
    if (capabilities.has("AUTHINFO", "USER"))
    {
        return AuthMethod::USER_PASS;
    }
    if (capabilities.has("AUTHINFO", "SASL"))
    {
        return AuthMethod::SASL;
    }
    return !capabilities.has("AUTHINFO") && allow_legacy_auth ? AuthMethod::USER_PASS : AuthMethod::UNAVAILABLE;
}

namespace
{

bool authenticate_user_pass()
{
    const std::string auth_user = get_auth_user();
    if (auth_user.empty() || g_nntp.command(fmt::format("AUTHINFO USER {}", auth_user)) <= 0 || g_nntp.check() <= 0)
    {
        return false;
    }
    if (nntp_response_code(g_nntp.ser_line) == AUTH_ACCEPTED)
    {
        return true;
    }
    if (nntp_response_code(g_nntp.ser_line) != PASSWORD_REQUIRED)
    {
        return false;
    }
    const std::string auth_pass = get_auth_pass();
    return !auth_pass.empty() && g_nntp.command(fmt::format("AUTHINFO PASS {}", auth_pass)) > 0 && g_nntp.check() > 0 &&
           nntp_response_code(g_nntp.ser_line) == AUTH_ACCEPTED;
}

} // namespace

int nntp_handle_auth_err()
{
    const std::string last_command_save{g_nntp.last_command};
    g_nntp.link.flags &= ~NNTP_FORCE_AUTH_NOW;
    if (g_nntp.link.auth_state != NntpAuthState::UNAUTHENTICATED)
    {
        return -2;
    }

    bool capabilities_refreshed{};
    for (;;)
    {
        if (g_nntp.link.require_tls_for_auth && nntp_require_tls() < 0)
        {
            return -2;
        }
        if (g_nntp.link.auth_state == NntpAuthState::AUTHENTICATED)
        {
            capabilities_refreshed = true;
            break;
        }
        switch (g_nntp.link.authentication_method())
        {
        case NNTPLink::AuthMethod::USER_PASS:
            break;

        case NNTPLink::AuthMethod::SASL:
            // Dispatch future SASL mechanisms here, before obtaining credentials.
            g_nntp.ser_line = "503 No supported SASL authentication mechanism.";
            return -2;

        case NNTPLink::AuthMethod::UNAVAILABLE:
            g_nntp.ser_line = "502 No permitted authentication method.";
            return -2;
        }

        g_nntp.link.auth_state = NntpAuthState::AUTHENTICATING;
        const bool authenticated = authenticate_user_pass();
        g_nntp.link.auth_state = authenticated ? NntpAuthState::AUTHENTICATED : NntpAuthState::UNAUTHENTICATED;
        if (authenticated)
        {
            break;
        }
        if (nntp_response_code(g_nntp.ser_line) != NNTP_ENCRYPTION_REQUIRED_VAL ||
            g_nntp.link.security != NntpSecurity::CLEAR_TEXT || nntp_start_tls() < 0)
        {
            return -2;
        }
    }

    const std::string auth_line = capabilities_refreshed ? "281 Authentication accepted" : g_nntp.ser_line;
    if (!capabilities_refreshed && last_command_save != "CAPABILITIES" && nntp_refresh_capabilities() < 0)
    {
        return -2;
    }
    if (last_command_save == "MODE READER")
    {
        g_nntp.last_command = last_command_save;
        g_nntp.ser_line = auth_line;
        return 1;
    }
    return g_nntp.command(last_command_save) > 0 ? 1 : -2;
}
