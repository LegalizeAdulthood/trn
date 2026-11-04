/* nntpclient.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <nntp/nntpclient.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpauth.h>
#include <nntp/nntpinit.h>
#include <trn/nntp.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

NntpSession g_nntp{};

static std::string capability_key(std::string_view value)
{
    std::string key{value};
    for (char &ch : key)
    {
        if (ch >= 'a' && ch <= 'z')
        {
            ch -= 'a' - 'A';
        }
    }
    return key;
}

void NntpCapabilities::add(std::string_view line)
{
    if (line.empty() || !((line.front() >= 'A' && line.front() <= 'Z') || (line.front() >= 'a' && line.front() <= 'z')))
    {
        return;
    }

    std::istringstream input{std::string{line}};
    std::string        token;
    input >> token;
    std::vector<std::string> &arguments = m_entries[capability_key(token)];
    arguments.clear();
    while (input >> token)
    {
        arguments.push_back(token);
    }
}

bool NntpCapabilities::has(std::string_view capability) const
{
    return m_entries.find(capability_key(capability)) != m_entries.end();
}

bool NntpCapabilities::has(std::string_view capability, std::string_view argument) const
{
    const auto entry = m_entries.find(capability_key(capability));
    if (entry == m_entries.end())
    {
        return false;
    }
    const std::string key = capability_key(argument);
    return std::any_of(entry->second.begin(), entry->second.end(),
                       [&key](const std::string &value) { return capability_key(value) == key; });
}

std::string_view NntpSession::response_text()
{
    return ser_line.size() > 4 ? std::string_view{ser_line}.substr(4) : std::string_view{};
}

int nntp_response_code(std::string_view response)
{
    int                          code{};
    const char                  *begin = response.data();
    const char                  *end = begin + response.size();
    const std::from_chars_result result = std::from_chars(begin, end, code);
    return result.ec == std::errc{} ? code : 0;
}

int nntp_connect(std::string_view machine, bool verbose)
{
    int response;

    if (g_nntp.link.connection)
    {
        return 1;
    }

    if (g_nntp.link.flags & NNTP_FORCE_AUTH_NEEDED)
    {
        g_nntp.link.flags |= NNTP_FORCE_AUTH_NOW;
    }
    g_nntp.link.flags |= NNTP_NEW_CMD_OK;
#if 0
try_to_connect:
#endif
    if (verbose)
    {
        fmt::print("Connecting to {}...", machine);
        std::fflush(stdout);
    }
    switch (response = server_init(machine))
    {
    case NNTP_GOODBYE_VAL:
        if (nntp_response_code(g_nntp.ser_line) == response)
        {
            if (verbose)
            {
                fmt::print("failed: {}\n", g_nntp.response_text());
            }
            else
            {
                fmt::print("News server \"{}\" is unavailable: {}\n", machine, g_nntp.response_text());
            }
            response = 0;
            break;
        }

    case -1:
        if (verbose)
        {
            fmt::print("failed.\n");
        }
        else
        {
            fmt::print("News server \"{}\" is unavailable.\n", machine);
        }
        response = 0;
        break;

    case NNTP_ACCESS_VAL:
        if (verbose)
        {
            fmt::print("access denied.\n");
        }
        else
        {
            fmt::print("This machine does not have permission to use the {} news server.\n\n", machine);
        }
        response = -1;
        break;

    case NNTP_NOPOSTOK_VAL:
        if (verbose)
        {
            fmt::print("Done (but no posting).\n\n");
        }
        response = 1;
        break;

    case NNTP_POSTOK_VAL:
        if (verbose)
        {
            fmt::print("Done.\n");
        }
        response = 1;
        break;

    default:
        if (verbose)
        {
            fmt::print("unknown response: {}.\n", response);
        }
        else
        {
            fmt::print("\nUnknown response code {} from {}.\n", response, machine);
        }
        response = 0;
        break;
    }
#if 0
    if (!response)
    {
        if (handle_no_connect() > 0)
        {
            goto try_to_connect;
        }
    }
#endif
    return response;
}

std::string nntp_server_name(std::string_view name)
{
    std::string result{name};
    if (!file_ref(result))
    {
        return result;
    }

    std::ifstream input{result};
    if (!input)
    {
        return result;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        return line;
    }
    return result;
}

int NntpSession::command(std::string_view bp)
{
    const std::string command{bp};
    std::time_t       now;
#if defined(DEBUG) && defined(FLUSH)
    if (g_debug & DEB_NNTP)
    {
        fmt::print(">{}\n", command);
    }
#endif
    last_command = command;
    if (!link.connection)
    {
        return nntp_handle_timeout();
    }
    const std::string_view verb = bp.substr(0, bp.find(' '));
    if (((link.require_tls_for_auth && string_case_equal(verb, "AUTHINFO")) ||
         (link.require_tls_for_posting && string_case_equal(verb, "POST"))) &&
        nntp_require_tls() < 0)
    {
        return -2;
    }
    last_command = command;
    const bool setup_command = command == "CAPABILITIES" || command == "MODE READER" || command == "STARTTLS";
    if ((link.flags & NNTP_FORCE_AUTH_NOW) && !setup_command)
    {
        link.flags &= ~NNTP_FORCE_AUTH_NOW;
        return nntp_handle_auth_err();
    }
    if (!(link.flags & NNTP_NEW_CMD_OK))
    {
        int ret = nntp_handle_nested_lists();
        if (ret <= 0)
        {
            return ret;
        }
    }
    error_code_t ec;
    link.connection->write_line(command, ec);
    if (ec)
    {
        return nntp_handle_timeout();
    }
    now = std::time((std::time_t*)nullptr);
    last_command_diff = now - link.last_command;
    link.last_command = now;
    return 1;
}

int NntpSession::unsupported_command(std::string_view command)
{
    ser_line = fmt::format("500 {} is unavailable.", command);
    return -1;
}

static int cached_command(std::string_view command, std::string_view arguments, bool variant = false)
{
    const std::string               name{command};
    const std::vector<std::string> &unsupported = g_nntp.link.unsupported_commands;
    if (std::binary_search(unsupported.begin(), unsupported.end(), name))
    {
        return g_nntp.unsupported_command(command);
    }
    if (g_nntp.command(arguments.empty() ? name : fmt::format("{} {}", command, arguments)) <= 0)
    {
        return -2;
    }

    const int status = nntp_check();
    const int code = nntp_response_code(g_nntp.ser_line);
    if (code == NNTP_BAD_COMMAND_VAL || (variant && code == NNTP_SYNTAX_VAL))
    {
        std::vector<std::string> &commands = g_nntp.link.unsupported_commands;
        commands.insert(std::lower_bound(commands.begin(), commands.end(), name), name);
    }
    if (status > 0)
    {
        g_nntp.link.flags &= ~NNTP_NEW_CMD_OK;
    }
    return status;
}

static int preferred_command(std::string_view modern, std::string_view legacy, std::string_view arguments,
                             bool advertised, bool variant = false)
{
    const bool try_modern = !g_nntp.link.capabilities.available || advertised;
    if (try_modern)
    {
        const int status = cached_command(modern, arguments, variant);
        const int code = nntp_response_code(g_nntp.ser_line);
        if (status == -2 || (code != NNTP_BAD_COMMAND_VAL && !(variant && code == NNTP_SYNTAX_VAL)))
        {
            return status;
        }
    }
    if (try_modern || g_nntp.link.capabilities.has(legacy))
    {
        return cached_command(legacy, arguments);
    }
    return g_nntp.unsupported_command(modern);
}

int nntp_overview(std::string_view range)
{
    return preferred_command("OVER", "XOVER", range, g_nntp.link.capabilities.has("OVER"));
}

int nntp_overview_format()
{
    const NntpCapabilities &capabilities = g_nntp.link.capabilities;
    if (capabilities.available && !capabilities.has("OVER") && !capabilities.has("LIST", "OVERVIEW.FMT"))
    {
        return g_nntp.unsupported_command("LIST OVERVIEW.FMT");
    }
    return cached_command("LIST OVERVIEW.FMT", {}, true);
}

int nntp_header_values(std::string_view header, std::string_view range)
{
    return preferred_command("HDR", "XHDR", fmt::format("{} {}", header, range), g_nntp.link.capabilities.has("HDR"));
}

int nntp_group_description(std::string_view group)
{
    return preferred_command("LIST NEWSGROUPS", "XGTITLE", group, g_nntp.link.capabilities.has("LIST", "NEWSGROUPS"),
                             true);
}

int nntp_group_descriptions(std::string_view pattern)
{
    if (g_nntp.link.capabilities.available && !g_nntp.link.capabilities.has("LIST", "NEWSGROUPS"))
    {
        return g_nntp.unsupported_command("LIST NEWSGROUPS");
    }
    return cached_command("LIST NEWSGROUPS", pattern, true);
}

int nntp_require_tls()
{
    if (g_nntp.link.security != NntpSecurity::Cleartext)
    {
        return 1;
    }
    if (!g_nntp.link.allow_starttls ||
        (g_nntp.link.capabilities.available && !g_nntp.link.capabilities.has("STARTTLS")))
    {
        g_nntp.ser_line = "483 TLS is required by policy.";
        return -2;
    }
    return nntp_start_tls();
}

int nntp_start_tls()
{
    if (g_nntp.link.security != NntpSecurity::Cleartext)
    {
        return 1;
    }
    if (g_nntp.link.auth_state == NntpAuthState::Authenticated)
    {
        g_nntp.ser_line = "483 TLS must be negotiated before authentication.";
        return -1;
    }
    if (!g_nntp.link.allow_starttls || g_nntp.link.tls_attempted || !g_nntp.link.connection ||
        !(g_nntp.link.flags & NNTP_NEW_CMD_OK))
    {
        return -1;
    }
    const std::string selected_group = g_nntp.link.selected_group;
    const long        selected_article = g_nntp.link.selected_article;
    g_nntp.link.tls_attempted = true;
    const NNTPFlags force_auth = g_nntp.link.flags & NNTP_FORCE_AUTH_NOW;
    g_nntp.link.flags &= ~NNTP_FORCE_AUTH_NOW;
    if (g_nntp.command("STARTTLS") <= 0 || nntp_check() <= 0 ||
        nntp_response_code(g_nntp.ser_line) != NNTP_STARTTLS_VAL)
    {
        nntp_close(false);
        return -1;
    }
    error_code_t ec;
    g_nntp.link.connection->start_tls(ec);
    if (ec)
    {
        g_nntp.ser_line = fmt::format("580 TLS negotiation failed: {}", ec.message());
        nntp_close(false);
        return -1;
    }
    g_nntp.link.security = NntpSecurity::StartTls;
    g_nntp.link.selected_group.clear();
    g_nntp.link.selected_article = 0;
    g_nntp.link.trailing_cr = false;
    g_nntp.gets_clear_buffer();
    if (nntp_refresh_capabilities() < 0)
    {
        nntp_close(false);
        return -1;
    }
    if (!selected_group.empty() &&
        (g_nntp.command(fmt::format("GROUP {}", selected_group)) <= 0 || nntp_check() <= 0 ||
         (selected_article > 0 &&
          (g_nntp.command(fmt::format("STAT {}", selected_article)) <= 0 || nntp_check() <= 0))))
    {
        nntp_close(false);
        return -1;
    }
    g_nntp.link.flags |= force_auth;
    return 1;
}

void NntpSession::remember_selection()
{
    const std::string_view command{last_command};
    const int              code = nntp_response_code(ser_line);
    std::istringstream     response{ser_line};
    int                    status{};
    if (code == 211 && string_case_equal(command.substr(0, command.find(' ')), "GROUP"))
    {
        long        count{}, first{}, last{};
        std::string group;
        if (response >> status >> count >> first >> last >> group)
        {
            link.selected_group = std::move(group);
            link.selected_article = count > 0 ? first : 0;
        }
    }
    else if (code >= 220 && code <= 223 && command.find('<') == std::string_view::npos)
    {
        long article{};
        if (response >> status >> article)
        {
            link.selected_article = article;
        }
    }
}

int nntp_check()
{
    bool retried_timeout{};
    bool retried_tls{};

read_it:
#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif
    errno = 0;
    error_code_t ec;
    g_nntp.ser_line = g_nntp.link.connection->read_line(ec);
    int ret = ec ? -2 : 0;
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    if (ret < 0)
    {
        if (errno == EINTR)
        {
            goto read_it;
        }
        g_nntp.ser_line = "503 Server closed connection.";
    }
    if (!retried_timeout && nntp_response_code(g_nntp.ser_line) == NNTP_TMPERR_VAL //
        && g_nntp.allow_timeout && g_nntp.last_command_diff > 60)
    {
        ret = nntp_handle_timeout();
        switch (ret)
        {
        case 1:
            retried_timeout = true;
            goto read_it;

        case 0: // We're quitting, so pretend it's OK
            g_nntp.ser_line = "205 Ok";
            break;

        default:
            break;
        }
    }
    else if (!g_nntp.ser_line.empty() && g_nntp.ser_line.front() <= NNTP_CLASS_CONT &&
             g_nntp.ser_line.front() >= NNTP_CLASS_INF)
    {
        ret = 1; // (this includes NNTP_CLASS_OK)
    }
    else if (ret >= 0 && !g_nntp.ser_line.empty() && g_nntp.ser_line.front() == NNTP_CLASS_FATAL)
    {
        ret = -1;
    }
    // Even though the following check doesn't catch all possible lists, the
    // bit will get set right when the caller checks NNTPLink::at_list_end().
    if (nntp_response_code(g_nntp.ser_line) == NNTP_LIST_FOLLOWS_VAL ||
        nntp_response_code(g_nntp.ser_line) == NNTP_CAPABILITIES_VAL)
    {
        g_nntp.link.flags &= ~NNTP_NEW_CMD_OK;
    }
    else
    {
        g_nntp.link.flags |= NNTP_NEW_CMD_OK;
    }
    const std::size_t len = g_nntp.ser_line.size();
    if (len >= 2 && g_nntp.ser_line[len - 1] == '\n' && g_nntp.ser_line[len - 2] == '\r')
    {
        g_nntp.ser_line.erase(len - 2);
    }
#if defined(DEBUG) && defined(FLUSH)
    if (g_debug & DEB_NNTP)
    {
        fmt::print("<{}\n", g_nntp.ser_line);
    }
#endif
    if (!retried_tls && nntp_response_code(g_nntp.ser_line) == NNTP_ENCRYPTION_REQUIRED_VAL &&
        g_nntp.link.auth_state != NntpAuthState::Authenticating && g_nntp.link.security == NntpSecurity::Cleartext &&
        g_nntp.link.allow_starttls && !g_nntp.link.tls_attempted)
    {
        const std::string command = g_nntp.last_command;
        if (nntp_start_tls() < 0 || g_nntp.command(command) <= 0)
        {
            return -2;
        }
        retried_tls = true;
        goto read_it;
    }
    if (nntp_response_code(g_nntp.ser_line) == NNTP_AUTH_NEEDED_VAL && g_nntp.last_command != "STARTTLS" &&
        g_nntp.link.auth_state == NntpAuthState::Unauthenticated &&
        !string_case_equal(std::string_view{g_nntp.last_command}.substr(0, g_nntp.last_command.find(' ')), "AUTHINFO"))
    {
        const bool reader_mode = g_nntp.last_command == "MODE READER";
        ret = nntp_handle_auth_err();
        if (ret > 0 && !reader_mode)
        {
            goto read_it;
        }
    }
    if (ret > 0)
    {
        g_nntp.remember_selection();
    }
    return ret;
}

int nntp_refresh_capabilities()
{
    g_nntp.link.capabilities = {};
    g_nntp.link.unsupported_commands.clear();
    if (g_nntp.command("CAPABILITIES") <= 0)
    {
        return -1;
    }
    const int result = nntp_check();
    const int code = nntp_response_code(g_nntp.ser_line);
    if (code == NNTP_BAD_COMMAND_VAL || code == NNTP_SYNTAX_VAL)
    {
        return 0;
    }
    if (result <= 0 || code != NNTP_CAPABILITIES_VAL)
    {
        return -1;
    }

    NntpCapabilities capabilities;
    for (;;)
    {
        error_code_t      ec;
        const std::string line = g_nntp.link.connection->read_line(ec);
        if (ec || line.empty())
        {
            g_nntp.ser_line = "503 Incomplete capability list.";
            nntp_close(false);
            return -1;
        }
        if (g_nntp.link.at_list_end(line))
        {
            capabilities.available = true;
            g_nntp.link.capabilities = std::move(capabilities);
            return 1;
        }
        capabilities.add(line);
    }
}

bool NNTPLink::at_list_end(std::string_view s)
{
    if (!s.empty() && s[0] == '.' && (s.size() == 1 || s[1] == '\r'))
    {
        flags |= NNTP_NEW_CMD_OK;
        return true;
    }
    flags &= ~NNTP_NEW_CMD_OK;
    return false;
}

class SigIntHolder
{
public:
    SigIntHolder()
    {
#ifdef HAS_SIGHOLD
        sighold(SIGINT);
#endif
    }
    ~SigIntHolder()
    {
#ifdef HAS_SIGHOLD
        sigrelse(SIGINT);
#endif
    }
};

// This returns 1 when it reads a full line, 0 if it reads a partial
// line, and -2 on EOF/error.  The maximum length includes a spot for
// the null-terminator, and we need room for our "\r\n"-stripping code
// to work right, so "len" MUST be at least 3.
//
NNTPGetsResult NntpSession::gets(std::string &line, int len)
{
    SigIntHolder holder;

    error_code_t ec;
    if (gets_line.empty())
    {
        gets_line = link.connection->read_line(ec);
        if (ec)
        {
            return NGSR_ERROR;
        }
        if (const std::size_t line_len = gets_line.size();
            line_len >= 2 && gets_line[line_len - 2] == '\r' && gets_line[line_len - 1] == '\n')
        {
            gets_line.erase(line_len - 2);
        }
    }

    if (len >= static_cast<int>(gets_line.length()))
    {
        line = gets_line;
        gets_line.clear();
        return NGSR_FULL_LINE;
    }

    line.assign(gets_line, 0, static_cast<std::size_t>(len - 1));
    gets_line = gets_line.substr(len);
    return NGSR_PARTIAL_LINE;
}

void NntpSession::gets_clear_buffer()
{
    gets_line.clear();
}

void nntp_close(bool send_quit)
{
    if (send_quit && g_nntp.link.connection)
    {
        if (g_nntp.command("QUIT") > 0)
        {
            nntp_check();
        }
    }
    g_nntp.link.connection.reset();
    g_nntp.link.capabilities = {};
    g_nntp.link.unsupported_commands.clear();
    g_nntp.link.flags |= NNTP_NEW_CMD_OK;
    g_nntp.link.security = NntpSecurity::Cleartext;
    g_nntp.link.auth_state = NntpAuthState::Unauthenticated;
    g_nntp.link.tls_attempted = false;
    g_nntp.link.selected_group.clear();
    g_nntp.link.selected_article = 0;
}
