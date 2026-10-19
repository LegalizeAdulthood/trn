/* nntpclient.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <nntp/nntpclient.h>

#include <config/common.h>
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

NNTPLink    g_nntp_link{}; // the current server's file handles
bool        g_nntp_allow_timeout{};
std::string g_ser_line;
std::string g_last_command;

static std::time_t s_last_command_diff{};

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

static std::string_view nntp_response_text()
{
    return g_ser_line.size() > 4 ? std::string_view{g_ser_line}.substr(4) : std::string_view{};
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

    if (g_nntp_link.connection)
    {
        return 1;
    }

    if (g_nntp_link.flags & NNTP_FORCE_AUTH_NEEDED)
    {
        g_nntp_link.flags |= NNTP_FORCE_AUTH_NOW;
    }
    g_nntp_link.flags |= NNTP_NEW_CMD_OK;
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
        if (nntp_response_code(g_ser_line) == response)
        {
            if (verbose)
            {
                fmt::print("failed: {}\n", nntp_response_text());
            }
            else
            {
                fmt::print("News server \"{}\" is unavailable: {}\n", machine, nntp_response_text());
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

int nntp_command(std::string_view bp)
{
    const std::string command{bp};
    std::time_t       now;
#if defined(DEBUG) && defined(FLUSH)
    if (g_debug & DEB_NNTP)
    {
        fmt::print(">{}\n", command);
    }
#endif
    g_last_command = command;
    if (!g_nntp_link.connection)
    {
        return nntp_handle_timeout();
    }
    if (g_nntp_link.flags & NNTP_FORCE_AUTH_NOW)
    {
        g_nntp_link.flags &= ~NNTP_FORCE_AUTH_NOW;
        return nntp_handle_auth_err();
    }
    if (!(g_nntp_link.flags & NNTP_NEW_CMD_OK))
    {
        int ret = nntp_handle_nested_lists();
        if (ret <= 0)
        {
            return ret;
        }
    }
    error_code_t ec;
    g_nntp_link.connection->write_line(command, ec);
    if (ec)
    {
        return nntp_handle_timeout();
    }
    now = std::time((std::time_t*)nullptr);
    s_last_command_diff = now - g_nntp_link.last_command;
    g_nntp_link.last_command = now;
    return 1;
}

static int unsupported_command(std::string_view command)
{
    g_ser_line = fmt::format("500 {} is unavailable.", command);
    return -1;
}

static int cached_command(std::string_view command, std::string_view arguments, bool variant = false)
{
    const std::string               name{command};
    const std::vector<std::string> &unsupported = g_nntp_link.unsupported_commands;
    if (std::binary_search(unsupported.begin(), unsupported.end(), name))
    {
        return unsupported_command(command);
    }
    if (nntp_command(arguments.empty() ? name : fmt::format("{} {}", command, arguments)) <= 0)
    {
        return -2;
    }

    const int status = nntp_check();
    const int code = nntp_response_code(g_ser_line);
    if (code == NNTP_BAD_COMMAND_VAL || (variant && code == NNTP_SYNTAX_VAL))
    {
        std::vector<std::string> &commands = g_nntp_link.unsupported_commands;
        commands.insert(std::lower_bound(commands.begin(), commands.end(), name), name);
    }
    if (status > 0)
    {
        g_nntp_link.flags &= ~NNTP_NEW_CMD_OK;
    }
    return status;
}

static int preferred_command(std::string_view modern, std::string_view legacy, std::string_view arguments,
                             bool advertised, bool variant = false)
{
    const bool try_modern = !g_nntp_link.capabilities.available || advertised;
    if (try_modern)
    {
        const int status = cached_command(modern, arguments, variant);
        const int code = nntp_response_code(g_ser_line);
        if (status == -2 || (code != NNTP_BAD_COMMAND_VAL && !(variant && code == NNTP_SYNTAX_VAL)))
        {
            return status;
        }
    }
    if (try_modern || g_nntp_link.capabilities.has(legacy))
    {
        return cached_command(legacy, arguments);
    }
    return unsupported_command(modern);
}

int nntp_overview(std::string_view range)
{
    return preferred_command("OVER", "XOVER", range, g_nntp_link.capabilities.has("OVER"));
}

int nntp_overview_format()
{
    const NntpCapabilities &capabilities = g_nntp_link.capabilities;
    if (capabilities.available && !capabilities.has("OVER") && !capabilities.has("LIST", "OVERVIEW.FMT"))
    {
        return unsupported_command("LIST OVERVIEW.FMT");
    }
    return cached_command("LIST OVERVIEW.FMT", {}, true);
}

int nntp_header_values(std::string_view header, std::string_view range)
{
    return preferred_command("HDR", "XHDR", fmt::format("{} {}", header, range), g_nntp_link.capabilities.has("HDR"));
}

int nntp_group_description(std::string_view group)
{
    return preferred_command("LIST NEWSGROUPS", "XGTITLE", group, g_nntp_link.capabilities.has("LIST", "NEWSGROUPS"),
                             true);
}

int nntp_group_descriptions(std::string_view pattern)
{
    if (g_nntp_link.capabilities.available && !g_nntp_link.capabilities.has("LIST", "NEWSGROUPS"))
    {
        return unsupported_command("LIST NEWSGROUPS");
    }
    return cached_command("LIST NEWSGROUPS", pattern, true);
}

int nntp_check()
{
    bool retried_timeout{};

read_it:
#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif
    errno = 0;
    error_code_t ec;
    g_ser_line = g_nntp_link.connection->read_line(ec);
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
        g_ser_line = "503 Server closed connection.";
    }
    if (!retried_timeout && nntp_response_code(g_ser_line) == NNTP_TMPERR_VAL //
        && g_nntp_allow_timeout && s_last_command_diff > 60)
    {
        ret = nntp_handle_timeout();
        switch (ret)
        {
        case 1:
            retried_timeout = true;
            goto read_it;

        case 0: // We're quitting, so pretend it's OK
            g_ser_line = "205 Ok";
            break;

        default:
            break;
        }
    }
    else if (!g_ser_line.empty() && g_ser_line.front() <= NNTP_CLASS_CONT && g_ser_line.front() >= NNTP_CLASS_INF)
    {
        ret = 1; // (this includes NNTP_CLASS_OK)
    }
    else if (!g_ser_line.empty() && g_ser_line.front() == NNTP_CLASS_FATAL)
    {
        ret = -1;
    }
    // Even though the following check doesn't catch all possible lists, the
    // bit will get set right when the caller checks nntp_at_list_end().
    if (nntp_response_code(g_ser_line) == NNTP_LIST_FOLLOWS_VAL ||
        nntp_response_code(g_ser_line) == NNTP_CAPABILITIES_VAL)
    {
        g_nntp_link.flags &= ~NNTP_NEW_CMD_OK;
    }
    else
    {
        g_nntp_link.flags |= NNTP_NEW_CMD_OK;
    }
    const std::size_t len = g_ser_line.size();
    if (len >= 2 && g_ser_line[len - 1] == '\n' && g_ser_line[len - 2] == '\r')
    {
        g_ser_line.erase(len - 2);
    }
#if defined(DEBUG) && defined(FLUSH)
    if (g_debug & DEB_NNTP)
    {
        fmt::print("<{}\n", g_ser_line);
    }
#endif
    if (nntp_response_code(g_ser_line) == NNTP_AUTH_NEEDED_VAL)
    {
        ret = nntp_handle_auth_err();
        if (ret > 0)
        {
            goto read_it;
        }
    }
    return ret;
}

int nntp_refresh_capabilities()
{
    g_nntp_link.capabilities = {};
    g_nntp_link.unsupported_commands.clear();
    if (nntp_command("CAPABILITIES") <= 0)
    {
        return -1;
    }
    const int result = nntp_check();
    const int code = nntp_response_code(g_ser_line);
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
        const std::string line = g_nntp_link.connection->read_line(ec);
        if (ec || line.empty())
        {
            g_ser_line = "503 Incomplete capability list.";
            nntp_close(false);
            return -1;
        }
        if (nntp_at_list_end(line))
        {
            capabilities.available = true;
            g_nntp_link.capabilities = std::move(capabilities);
            return 1;
        }
        capabilities.add(line);
    }
}

bool nntp_at_list_end(std::string_view s)
{
    if (!s.empty() && s[0] == '.' && (s.size() == 1 || s[1] == '\r'))
    {
        g_nntp_link.flags |= NNTP_NEW_CMD_OK;
        return true;
    }
    g_nntp_link.flags &= ~NNTP_NEW_CMD_OK;
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

static std::string s_nntp_gets_line;

// This returns 1 when it reads a full line, 0 if it reads a partial
// line, and -2 on EOF/error.  The maximum length includes a spot for
// the null-terminator, and we need room for our "\r\n"-stripping code
// to work right, so "len" MUST be at least 3.
//
NNTPGetsResult nntp_gets(std::string &line, int len)
{
    SigIntHolder holder;

    error_code_t ec;
    if (s_nntp_gets_line.empty())
    {
        s_nntp_gets_line = g_nntp_link.connection->read_line(ec);
        if (ec)
        {
            return NGSR_ERROR;
        }
        if (const std::size_t line_len = s_nntp_gets_line.size();
            line_len >= 2 && s_nntp_gets_line[line_len - 2] == '\r' && s_nntp_gets_line[line_len - 1] == '\n')
        {
            s_nntp_gets_line.erase(line_len - 2);
        }
    }

    if (len >= static_cast<int>(s_nntp_gets_line.length()))
    {
        line = s_nntp_gets_line;
        s_nntp_gets_line.clear();
        return NGSR_FULL_LINE;
    }

    line.assign(s_nntp_gets_line, 0, static_cast<std::size_t>(len - 1));
    s_nntp_gets_line = s_nntp_gets_line.substr(len);
    return NGSR_PARTIAL_LINE;
}

void nntp_gets_clear_buffer()
{
    s_nntp_gets_line.clear();
}

void nntp_close(bool send_quit)
{
    if (send_quit && g_nntp_link.connection)
    {
        if (nntp_command("QUIT") > 0)
        {
            nntp_check();
        }
    }
    g_nntp_link.connection.reset();
    g_nntp_link.capabilities = {};
    g_nntp_link.unsupported_commands.clear();
    g_nntp_link.flags |= NNTP_NEW_CMD_OK;
}
