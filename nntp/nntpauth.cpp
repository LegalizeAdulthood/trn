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

enum class AuthMethod
{
    Unavailable,
    UserPass,
    Sasl
};

AuthMethod authentication_method()
{
    const NntpCapabilities &capabilities = g_nntp_link.capabilities;
    if (capabilities.has("AUTHINFO", "USER"))
    {
        return AuthMethod::UserPass;
    }
    if (capabilities.has("AUTHINFO", "SASL"))
    {
        return AuthMethod::Sasl;
    }
    return !capabilities.has("AUTHINFO") && g_nntp_link.allow_legacy_auth ? AuthMethod::UserPass
                                                                          : AuthMethod::Unavailable;
}

bool authenticate_user_pass()
{
    const std::string auth_user = get_auth_user();
    if (auth_user.empty() || nntp_command(fmt::format("AUTHINFO USER {}", auth_user)) <= 0 || nntp_check() <= 0)
    {
        return false;
    }
    if (nntp_response_code(g_ser_line) == AUTH_ACCEPTED)
    {
        return true;
    }
    if (nntp_response_code(g_ser_line) != PASSWORD_REQUIRED)
    {
        return false;
    }
    const std::string auth_pass = get_auth_pass();
    return !auth_pass.empty() && nntp_command(fmt::format("AUTHINFO PASS {}", auth_pass)) > 0 && nntp_check() > 0 &&
           nntp_response_code(g_ser_line) == AUTH_ACCEPTED;
}

} // namespace

int nntp_handle_auth_err()
{
    const std::string last_command_save{g_last_command};
    g_nntp_link.flags &= ~NNTP_FORCE_AUTH_NOW;
    if (g_nntp_link.auth_state != NntpAuthState::Unauthenticated)
    {
        return -2;
    }

    bool capabilities_refreshed{};
    for (;;)
    {
        if (g_nntp_link.require_tls_for_auth && nntp_require_tls() < 0)
        {
            return -2;
        }
        if (g_nntp_link.auth_state == NntpAuthState::Authenticated)
        {
            capabilities_refreshed = true;
            break;
        }
        switch (authentication_method())
        {
        case AuthMethod::UserPass:
            break;

        case AuthMethod::Sasl:
            // Dispatch future SASL mechanisms here, before obtaining credentials.
            g_ser_line = "503 No supported SASL authentication mechanism.";
            return -2;

        case AuthMethod::Unavailable:
            g_ser_line = "502 No permitted authentication method.";
            return -2;
        }

        g_nntp_link.auth_state = NntpAuthState::Authenticating;
        const bool authenticated = authenticate_user_pass();
        g_nntp_link.auth_state = authenticated ? NntpAuthState::Authenticated : NntpAuthState::Unauthenticated;
        if (authenticated)
        {
            break;
        }
        if (nntp_response_code(g_ser_line) != NNTP_ENCRYPTION_REQUIRED_VAL ||
            g_nntp_link.security != NntpSecurity::Cleartext || nntp_start_tls() < 0)
        {
            return -2;
        }
    }

    const std::string auth_line = capabilities_refreshed ? "281 Authentication accepted" : g_ser_line;
    if (!capabilities_refreshed && last_command_save != "CAPABILITIES" && nntp_refresh_capabilities() < 0)
    {
        return -2;
    }
    if (last_command_save == "MODE READER")
    {
        g_last_command = last_command_save;
        g_ser_line = auth_line;
        return 1;
    }
    return nntp_command(last_command_save) > 0 ? 1 : -2;
}
