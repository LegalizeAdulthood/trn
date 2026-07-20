/* intrp.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/intrp.h>

#include <config/common.h>
#include <config/env.h>
#include <config/pipe_io.h>
#include <config/user_id.h>
#include <trn/artio.h>
#include <trn/artsrch.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/respond.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/search.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#ifdef HAS_UNAME
#include <sys/utsname.h>
struct utsname utsn;
#endif

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

std::string g_orig_dir;    // cwd when rn invoked
std::string g_host_name;   // host name to match local postings
std::string g_head_name;
int         g_perform_count{};

#ifdef HAS_NEWS_ADMIN
const std::string g_news_admin{NEWS_ADMIN}; // news administrator
int               g_news_uid{};
#endif

static void        skip_interp_cursor(std::string_view &pattern, std::string_view stoppers);

static const char   *s_regexp_specials = "^$.*[\\/?%";
static CompiledRegex s_cond_compex;
static std::string   s_last_input;
static int           s_interp_counter{};

void interp_init(char *tcbuf, int tcbuf_len)
{
    s_last_input.clear();
    s_cond_compex.init_compex();

    // get environmental stuff

#ifdef HAS_NEWS_ADMIN
    {
#ifdef HAS_GETPWENT
        struct passwd* pwd = getpwnam(NEWS_ADMIN);

        if (pwd != nullptr)
        {
            g_news_uid = pwd->pw_uid;
        }
#else
#ifdef TILDENAME
        (void) file_exp(std::string{"~"} + NEWS_ADMIN);
#else
        ... "Define either HAS_GETPWENT or TILDENAME to get NEWS_ADMIN"
#endif  // TILDENAME
#endif  // HAS_GETPWENT
    }

    // if this is the news admin then load his UID into g_news_uid

    if (!g_login_name.empty())
        g_news_uid = getuid();
#endif

    if (g_check_flag)             // that getwd below takes ~1/3 sec.
    {
        return;                  // and we do not need it for -c
    }
    (void) tcbuf;
    (void) tcbuf_len;
    g_orig_dir = trn_getwd();     // find working directory name

    // name of header file (%h)

    g_head_name = file_exp(HEADNAME);

    // the hostname to use in local-article comparisons
#if HOST_BITS != 0
    int i = (HOST_BITS < 2 ? 2 : HOST_BITS);
    g_host_name = g_p_host_name;
    std::size_t suffix_start = g_host_name.size();
    while (i && suffix_start != 0U)
    {
        suffix_start--;
        if (g_host_name[suffix_start] == '.')
        {
            i--;
        }
    }
    if (suffix_start < g_host_name.size() && g_host_name[suffix_start] == '.')
    {
        suffix_start++;
    }
    g_host_name.erase(0, suffix_start);
#else
    g_host_name = g_p_host_name;
#endif
}

void interp_final()
{
    g_head_name.clear();
    g_host_name.clear();
    g_orig_dir.clear();
}

// skip interpolations

static bool at_skip_stopper(std::string_view pattern, std::string_view stoppers)
{
    return !pattern.empty() && !stoppers.empty() && stoppers.find(pattern.front()) != std::string_view::npos;
}

static void skip_escaped_until(std::string_view &pattern, char stopper)
{
    while (!pattern.empty() && pattern.front() != stopper)
    {
        pattern.remove_prefix(pattern.front() == '\\' && pattern.size() > 1 ? 2U : 1U);
    }
}

static void skip_interp_cursor(std::string_view &pattern, std::string_view stoppers)
{
#ifdef DEBUG
    if (g_debug & DEB_INTRP)
    {
        std::printf("skipinterp %.*s (till %.*s)\n", static_cast<int>(pattern.size()),
                    pattern.empty() ? "" : pattern.data(), static_cast<int>(stoppers.size()),
                    stoppers.empty() ? "" : stoppers.data());
    }
#endif

    while (!pattern.empty() && !at_skip_stopper(pattern, stoppers))
    {
        if (pattern.front() == '%' && pattern.size() > 1)
        {
            pattern.remove_prefix(1);
            for (;;)
            {
                if (pattern.empty())
                {
                    return;
                }
                switch (pattern.front())
                {
                case '^':
                case '_':
                case '\\':
                case '\'':
                case '>':
                case ')':
                    pattern.remove_prefix(1);
                    continue;

                case ':':
                    pattern.remove_prefix(1);
                    while (!pattern.empty() //
                           && (pattern.front() == '.' || pattern.front() == '-' ||
                               std::isdigit(static_cast<unsigned char>(pattern.front()))))
                    {
                        pattern.remove_prefix(1);
                    }
                    continue;

                default:
                    break;
                }
                break;
            }
            switch (pattern.front())
            {
            case '{':
                pattern.remove_prefix(1);
                skip_escaped_until(pattern, '}');
                break;

            case '[':
                pattern.remove_prefix(1);
                skip_escaped_until(pattern, ']');
                break;

            case '(':
                pattern.remove_prefix(1);
                skip_interp_cursor(pattern, "!=");
                if (pattern.empty())
                {
                    return;
                }
                pattern.remove_prefix(1);
                skip_escaped_until(pattern, '?');
                if (pattern.empty())
                {
                    return;
                }
                pattern.remove_prefix(1);
                skip_interp_cursor(pattern, ":)");
                if (!pattern.empty() && pattern.front() == ':')
                {
                    pattern.remove_prefix(1);
                    skip_interp_cursor(pattern, ")");
                }
                break;

            case '`':
                pattern.remove_prefix(1);
                skip_interp_cursor(pattern, "`");
                break;

            case '"':
                pattern.remove_prefix(1);
                skip_interp_cursor(pattern, "\"");
                break;

            default:
                break;
            }
            if (!pattern.empty())
            {
                pattern.remove_prefix(1);
            }
        }
        else
        {
            if (pattern.front() == '^' && pattern.size() > 1 //
                && (static_cast<Uchar>(pattern[1]) >= '?' || pattern[1] == '(' || pattern[1] == ')'))
            {
                pattern.remove_prefix(2);
            }
            else if (pattern.front() == '\\' && pattern.size() > 1)
            {
                pattern.remove_prefix(2);
            }
            else
            {
                pattern.remove_prefix(1);
            }
        }
    }
}

std::size_t skip_interp(std::string_view pattern, std::string_view stoppers)
{
    const std::string_view start{pattern};

    skip_interp_cursor(pattern, stoppers);
    return start.size() - pattern.size();
}

std::string do_interp(std::string_view pattern)
{
    std::string_view cursor{pattern};
    return do_interp(cursor, {}, {});
}

std::string do_interp(std::string_view &pattern, std::string_view stoppers, std::string_view cmd)
{
    std::optional<std::string>        subj_buf;
    std::optional<std::string>        ngs_buf;
    std::optional<std::string>        refs_buf;
    std::optional<std::string>        artid_buf;
    std::optional<std::string>        reply_buf;
    std::optional<std::string>        from_buf;
    std::optional<std::string>        path_buf;
    std::optional<std::string>        follow_buf;
    std::optional<std::string>        dist_buf;
    std::optional<std::string>        line_buf;
    std::optional<std::size_t>        line_split;
    static constexpr std::size_t      scratch_size = 8192;
    static constexpr std::size_t      format_size = 512;
    static constexpr std::string_view search_cmd_chars{"/?g"};
    static constexpr std::string_view search_cmd_tail_chars{"/?"};
    std::string                       result;
    std::string                       scratch;
    const char                       *noname_text = "noname";
    int                               metabit = 0;

    result.reserve(CMD_BUF_LEN);
    scratch.reserve(scratch_size);
    const auto read_backslash = [](std::string_view &cursor)
    {
        if (cursor.empty() || cursor.front() == '\0')
        {
            return '\\';
        }
        if (cursor.front() >= '0' && cursor.front() <= '7')
        {
            int i = 0;
            while (i < 01000 && !cursor.empty() && cursor.front() >= '0' && cursor.front() <= '7')
            {
                i <<= 3;
                i += cursor.front() - '0';
                cursor.remove_prefix(1);
            }
            return static_cast<char>(i & 0377);
        }
        const char escaped = cursor.front();
        cursor.remove_prefix(1);
        switch (escaped)
        {
        case 'a':
            return '\a';

        case 'b':
            return '\b';

        case 'f':
            return '\f';

        case 'n':
            return '\n';

        case 'r':
            return '\r';

        case 't':
            return '\t';

        case 'v':
            return '\v';

        case 'x':
            if (!cursor.empty() && std::isxdigit(static_cast<unsigned char>(cursor.front())))
            {
                int i = 0;
                while (i < 01000 && !cursor.empty() && std::isxdigit(static_cast<unsigned char>(cursor.front())))
                {
                    static constexpr char hex_digits[]{"0123456789ABCDEF"};
                    i <<= 4;
                    i += std::strchr(hex_digits, std::toupper(static_cast<unsigned char>(cursor.front()))) - hex_digits;
                    cursor.remove_prefix(1);
                }
                return static_cast<char>(i & 0377);
            }
            return escaped;

        default:
            return escaped;
        }
    };
    const auto copy_till_view = [](std::string_view &from, char delim)
    {
        std::string text;
        text.reserve(from.size());
        while (!from.empty() && from.front() != '\0')
        {
            if (from.front() == '\\' && from.size() > 1 && from[1] == delim)
            {
                from.remove_prefix(1);
            }
            else if (from.front() == delim)
            {
                break;
            }
            text.push_back(from.front());
            from.remove_prefix(1);
        }
        return text;
    };
    const auto read_scratch_line = [&scratch](std::FILE *fp) -> bool
    {
        scratch.assign(scratch_size, '\0');
        if (std::fgets(scratch.data(), static_cast<int>(scratch.size()), fp) == nullptr)
        {
            scratch.clear();
            return false;
        }
        const std::size_t end = scratch.find('\0');
        if (end != std::string::npos)
        {
            scratch.resize(end);
        }
        return true;
    };

    while (!pattern.empty() && pattern.front() != '\0' && !at_skip_stopper(pattern, stoppers))
    {
        if (pattern.front() == '%' && pattern.size() > 1 && pattern[1] != '\0')
        {
            pattern.remove_prefix(1);
            std::string      env_value;
            std::string      format_spec;
            std::string      search_command;
            std::string      owned_value;
            std::string      transform_text;
            std::string_view value;
            bool             upper = false;
            bool             lastcomp = false;
            bool             re_quote = false;
            int              tick_quote = 0;
            bool             address_parse = false;
            bool             comment_parse = false;
            bool             proc_sprintf = false;
            bool             has_value = false;
            const auto       set_value = [&has_value, &value](std::string_view text)
            {
                value = text;
                has_value = true;
            };
            const auto set_owned_value = [&owned_value, &set_value](std::string text)
            {
                owned_value = std::move(text);
                set_value(owned_value);
            };
            const auto make_mutable_text = [&transform_text, &value]()
            {
                transform_text.assign(value);
                return transform_text.data();
            };
            const auto format_value = [](const char *format, std::string_view text)
            {
                const std::string input{text};
                const int         size = std::snprintf(nullptr, 0, format, input.c_str());
                if (size < 0)
                {
                    return std::string{};
                }
                std::string result_text(static_cast<std::size_t>(size) + 1, '\0');
                std::snprintf(result_text.data(), result_text.size(), format, input.c_str());
                result_text.resize(static_cast<std::size_t>(size));
                return result_text;
            };
            while (!has_value)
            {
                if (pattern.empty() || pattern.front() == '\0')
                {
                    set_value({});
                    break;
                }
                switch (pattern.front())
                {
                case '^':
                    upper = true;
                    pattern.remove_prefix(1);
                    break;

                case '_':
                    lastcomp = true;
                    pattern.remove_prefix(1);
                    break;

                case '\\':
                    re_quote = true;
                    pattern.remove_prefix(1);
                    break;

                case '\'':
                    tick_quote++;
                    pattern.remove_prefix(1);
                    break;

                case '>':
                    address_parse = true;
                    pattern.remove_prefix(1);
                    break;

                case ')':
                    comment_parse = true;
                    pattern.remove_prefix(1);
                    break;

                case ':':
                {
                    proc_sprintf = true;
                    format_spec.reserve(format_size);
                    format_spec = '%';
                    pattern.remove_prefix(1); // Skip over ':'
                    while (!pattern.empty()   //
                           && (pattern.front() == '.' || pattern.front() == '-' ||
                               std::isdigit(static_cast<unsigned char>(pattern.front()))))
                    {
                        format_spec += pattern.front();
                        pattern.remove_prefix(1);
                    }
                    format_spec += 's';
                    break;
                }

                case '/':
                {
                    search_command.reserve(scratch_size);
                    if (cmd.empty() || search_cmd_chars.find(cmd.front()) == std::string_view::npos)
                    {
                        search_command += '/';
                    }
                    search_command += g_last_pat;
                    if (cmd.empty() || cmd.front() != 'g')
                    {
                        if (!cmd.empty() && search_cmd_tail_chars.find(cmd.front()) != std::string_view::npos)
                        {
                            search_command += cmd.front();
                        }
                        else
                        {
                            search_command += '/';
                        }
                        if (g_art_do_read)
                        {
                            search_command += 'r';
                        }
                        if (g_art_how_much != ARTSCOPE_SUBJECT)
                        {
                            search_command += g_scope_str[g_art_how_much];
                            if (g_art_how_much == ARTSCOPE_ONE_HDR)
                            {
                                const std::string_view header_name{g_header_type[g_art_srch_hdr].name};
                                const std::size_t      colon = header_name.find(':');
                                search_command +=
                                    header_name.substr(0, colon == std::string_view::npos ? colon : colon + 1);
                            }
                        }
                    }
                    set_value(search_command);
                    break;
                }

                case '{':
                {
                    pattern.remove_prefix(1);
                    scratch = copy_till_view(pattern, '}');
                    std::string_view  name{scratch};
                    std::string_view  default_value;
                    const std::size_t dash = name.find('-');
                    if (dash != std::string_view::npos)
                    {
                        default_value = name.substr(dash + 1);
                        name = name.substr(0, dash);
                    }
                    env_value = get_env_var(name, default_value);
                    set_value(env_value);
                    break;
                }

                case '<':
                {
                    pattern.remove_prefix(1);
                    scratch = copy_till_view(pattern, '>');
                    std::string_view  name{scratch};
                    std::string_view  default_value;
                    const std::size_t dash = name.find('-');
                    if (dash != std::string_view::npos)
                    {
                        default_value = name.substr(dash + 1);
                        name = name.substr(0, dash);
                    }
                    env_value = get_env_var(name, default_value);
                    std::string_view env_cursor{env_value};
                    env_value = do_interp(env_cursor, {}, {});
                    set_value(env_value);
                    break;
                }

                case '[':
                {
                    pattern.remove_prefix(1);
                    scratch = copy_till_view(pattern, ']');
                    if (g_in_ng)
                    {
                        HeaderLineType which_line;
                        if (!scratch.empty() && (which_line = get_header_num(scratch)) != SOME_LINE)
                        {
                            line_buf = fetch_lines(g_art, which_line);
                            set_value(*line_buf);
                        }
                        else
                        {
                            set_value({});
                        }
                    }
                    else
                    {
                        set_value({});
                    }
                    break;
                }

                case '(':
                {
                    CompiledRegex *oldbra_compex = g_bra_compex;
                    char           rch;
                    bool           matched;

                    pattern.remove_prefix(1);
                    std::string_view  condition_cursor{pattern};
                    const std::string condition_text = do_interp(condition_cursor, "!=", cmd);
                    pattern = condition_cursor;
                    rch = condition_cursor.empty() ? '\0' : condition_cursor.front();
                    if (rch == '!')
                    {
                        condition_cursor.remove_prefix(1);
                        pattern = condition_cursor;
                    }
                    if (condition_cursor.empty() || condition_cursor.front() != '=')
                    {
                        goto getout;
                    }
                    condition_cursor.remove_prefix(1);
                    std::string regex_text = copy_till_view(condition_cursor, '?');
                    pattern = condition_cursor;
                    if (condition_cursor.empty())
                    {
                        goto getout;
                    }
                    format_spec.clear();
                    format_spec.reserve(format_size);
                    proc_sprintf = false;
                    bool interp_regex = false;
                    for (const char ch : regex_text)
                    {
                        switch (ch)
                        {
                        case '^':
                            format_spec += '\\';
                            break;

                        case '\\':
                            format_spec += '\\';
                            format_spec += '\\';
                            break;

                        case '%':
                            interp_regex = true;
                            break;
                        }
                        format_spec += ch;
                    }
                    if (interp_regex)
                    {
                        std::string_view regex_cursor{format_spec};
                        regex_text = do_interp(regex_cursor, {}, cmd);
                    }
                    const char *compile_error = s_cond_compex.compile(regex_text.c_str(), true, true);
                    if (compile_error != nullptr)
                    {
                        fmt::print("{}: {}\n", regex_text, compile_error);
                        pattern.remove_prefix(pattern.size());
                        s_cond_compex.free_compex();
                        goto getout;
                    }
                    matched = s_cond_compex.execute(condition_text.c_str()) != nullptr;
                    if (s_cond_compex.get_bracket(0)) // were there brackets?
                    {
                        g_bra_compex = &s_cond_compex;
                    }
                    condition_cursor.remove_prefix(1);
                    std::string branch_text;
                    if (matched == (rch == '='))
                    {
                        branch_text = do_interp(condition_cursor, ":)", cmd);
                        if (!condition_cursor.empty() && condition_cursor.front() == ':')
                        {
                            condition_cursor.remove_prefix(1);
                            skip_interp_cursor(condition_cursor, ")");
                        }
                    }
                    else
                    {
                        skip_interp_cursor(condition_cursor, ":)");
                        if (!condition_cursor.empty() && condition_cursor.front() == ':')
                        {
                            condition_cursor.remove_prefix(1);
                        }
                        branch_text = do_interp(condition_cursor, ")", cmd);
                    }
                    pattern = condition_cursor;
                    set_owned_value(std::move(branch_text));
                    g_bra_compex = oldbra_compex;
                    s_cond_compex.free_compex();
                    break;
                }

                case '`':
                {
                    pattern.remove_prefix(1);
                    scratch = do_interp(pattern, "`", cmd);
                    std::FILE *pipefp = popen(scratch.c_str(), "r");
                    if (pipefp != nullptr)
                    {
                        scratch.assign(scratch_size, '\0');
                        const std::size_t len = std::fread(scratch.data(), sizeof(char), scratch.size() - 1, pipefp);
                        scratch.resize(len);
                        pclose(pipefp);
                    }
                    else
                    {
                        fmt::print("\nCan't run {}\n", scratch);
                        scratch.clear();
                    }
                    for (std::size_t i = 0; i < scratch.size(); i++)
                    {
                        if (scratch[i] == '\n')
                        {
                            if (i + 1 < scratch.size())
                            {
                                scratch[i] = ' ';
                            }
                            else
                            {
                                scratch.resize(i);
                                break;
                            }
                        }
                    }
                    set_value(scratch);
                    break;
                }

                case '"':
                {
                    pattern.remove_prefix(1);
                    scratch = do_interp(pattern, "\"", cmd);
                    fmt::print("{}", scratch);
                    reset_tty();
                    read_scratch_line(stdin);
                    no_echo();
                    cr_mode();
                    if (!scratch.empty() && scratch.back() == '\n')
                    {
                        scratch.pop_back();
                    }
                    s_last_input = scratch;
                    set_value(scratch);
                    break;
                }

                case '~':
                    set_value(g_home_dir);
                    break;

                case '.':
                    set_value(g_dot_dir);
                    break;

                case '+':
                    set_value(g_trn_dir);
                    break;

                case '$':
                    set_owned_value(std::to_string(g_our_pid));
                    break;

                case '#':
                    if (upper)
                    {
                        set_owned_value(std::to_string(++s_interp_counter));
                    }
                    else
                    {
                        set_owned_value(std::to_string(g_perform_count));
                    }
                    break;

                case '?':
                    set_value(" ");
                    line_split = result.size();
                    break;

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    const char *bracket = g_bra_compex->get_bracket(pattern.front() - '0');
                    set_value(bracket == nullptr ? std::string_view{} : std::string_view{bracket});
                    break;
                }

                case 'a':
                    if (g_in_ng)
                    {
                        set_owned_value(std::to_string(g_art.value_of()));
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'A':
                    if (g_in_ng)
                    {
                        if (g_data_source->m_flags & DF_REMOTE)
                        {
                            if (art_open(g_art, (ArticlePosition) 0))
                            {
                                nntp_finish_body(FB_SILENT);
                                set_owned_value(
                                    fmt::format("{}/{}", g_data_source->m_spool_dir, nntp_art_name(g_art, false)));
                            }
                            else
                            {
                                set_value({});
                            }
                        }
                        else
                        {
                            set_owned_value(
                                fmt::format("{}/{}/{}", g_data_source->m_spool_dir, g_newsgroup_dir, g_art.value_of()));
                        }
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'b':
                    set_value(g_save_dest);
                    break;

                case 'B':
                    set_owned_value(std::to_string(g_save_from.value_of()));
                    break;

                case 'c':
                    set_value(g_newsgroup_dir);
                    break;

                case 'C':
                    set_value(g_newsgroup_name);
                    break;

                case 'd':
                    if (!g_newsgroup_dir.empty())
                    {
                        set_owned_value(fmt::format("{}/{}", g_data_source->m_spool_dir, g_newsgroup_dir));
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'D':
                    if (g_in_ng)
                    {
                        dist_buf = fetch_lines(g_art, DIST_LINE);
                        set_value(*dist_buf);
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'e':
                {
                    if (g_extract_prog.empty())
                    {
                        set_value("-");
                    }
                    else
                    {
                        set_value(g_extract_prog);
                    }
                    break;
                }

                case 'E':
                    if (g_extract_dest.empty())
                    {
                        set_value({});
                    }
                    else
                    {
                        set_value(g_extract_dest);
                    }
                    break;

                case 'f': // from line
                    if (g_in_ng)
                    {
                        parse_header(g_art);
                        if (g_header_type[REPLY_LINE].min_pos >= 0 && !comment_parse)
                        {
                            // was there a reply line?
                            if (!reply_buf)
                            {
                                reply_buf = fetch_lines(g_art, REPLY_LINE);
                            }
                            set_value(*reply_buf);
                        }
                        else
                        {
                            if (!from_buf)
                            {
                                from_buf = fetch_lines(g_art, FROM_LINE);
                            }
                            set_value(*from_buf);
                        }
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'F':
                    if (g_in_ng)
                    {
                        parse_header(g_art);
                        if (g_header_type[FOLLOW_LINE].min_pos >= 0)
                        // is there a Followup-To line?
                        {
                            follow_buf = fetch_lines(g_art, FOLLOW_LINE);
                            set_value(*follow_buf);
                        }
                        else
                        {
                            ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
                            set_value(*ngs_buf);
                        }
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'g': // general mode
                    set_owned_value(std::string(1, static_cast<char>(g_general_mode)));
                    break;

                case 'h': // header file name
                    set_value(g_head_name);
                    break;

                case 'H': // host name in postings
                    set_value(g_p_host_name);
                    break;

                case 'i':
                    if (g_in_ng)
                    {
                        if (!artid_buf)
                        {
                            artid_buf = fetch_lines(g_art, MSG_ID_LINE);
                        }
                        if (!artid_buf->empty() && artid_buf->front() != '<')
                        {
                            set_owned_value(fmt::format("<{}>", *artid_buf));
                        }
                        else
                        {
                            set_value(*artid_buf);
                        }
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'I': // indent string for quoting
                    set_owned_value(fmt::format("'{}'", g_indent_string));
                    break;

                case 'j':
                    set_owned_value(std::to_string(g_just_a_sec * 10));
                    break;

                case 'l': // news admin login
#ifdef HAS_NEWS_ADMIN
                    set_value(g_news_admin);
#else
                    set_value("???");
#endif
                    break;

                case 'L': // login id
                    set_value(g_login_name);
                    break;

                case 'm': // current mode
                    set_owned_value(std::string(1, static_cast<char>(g_mode)));
                    break;

                case 'M':
                    set_owned_value(std::to_string(g_dm_count));
                    break;

                case 'n': // newsgroups
                    if (g_in_ng)
                    {
                        ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
                        set_value(*ngs_buf);
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'N': // full name
                    env_value = get_env_var("NAME", g_real_name);
                    set_value(env_value);
                    break;

                case 'o': // organization
                {
#ifdef IGNORE_ORG
                    env_value = get_env_var("NEWSORG", s_orgname);
#else
                    env_value = get_env_var("NEWSORG");
                    if (env_value.empty())
                    {
                        env_value = get_env_var("ORGANIZATION", ORG_NAME);
                    }
#endif
                    const std::string org_file = file_exp(env_value);
                    if (FILE_REF(org_file.c_str()))
                    {
                        std::ifstream input{org_file};
                        if (input)
                        {
                            std::getline(input, owned_value);
                            set_value(owned_value);
                        }
                        else
                        {
                            set_value({});
                        }
                    }
                    else
                    {
                        set_value(env_value);
                    }
                    break;
                }

                case 'O':
                    set_value(g_orig_dir);
                    break;

                case 'p':
                    set_value(g_priv_dir);
                    break;

                case 'P':
                    if (g_data_source)
                    {
                        set_value(g_data_source->m_spool_dir);
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'q':
                    scratch.assign(s_last_input);
                    set_value(scratch);
                    break;

                case 'r':
                    if (g_in_ng)
                    {
                        parse_header(g_art);
                        refs_buf.reset();
                        if (g_header_type[REFS_LINE].min_pos >= 0)
                        {
                            refs_buf = fetch_lines(g_art, REFS_LINE);
                            normalize_refs(refs_buf->data());
                            const std::size_t normalized_size = refs_buf->find('\0');
                            if (normalized_size != std::string::npos)
                            {
                                refs_buf->resize(normalized_size);
                            }
                            const std::string_view refs_text{refs_buf->data(), refs_buf->size()};
                            const std::size_t      last_ref = refs_text.rfind('<');
                            if (last_ref != std::string_view::npos)
                            {
                                set_value(refs_text.substr(last_ref));
                                break;
                            }
                        }
                    }
                    set_value({});
                    break;

                case 'R':
                {
                    if (!g_in_ng)
                    {
                        set_value({});
                        break;
                    }
                    parse_header(g_art);
                    refs_buf.reset();
                    if (g_header_type[REFS_LINE].min_pos >= 0)
                    {
                        refs_buf = fetch_lines(g_art, REFS_LINE);
                        normalize_refs(refs_buf->data());
                        const std::size_t normalized_size = refs_buf->find('\0');
                        if (normalized_size != std::string::npos)
                        {
                            refs_buf->resize(normalized_size);
                        }
                        // no more than 3 prior references PLUS the
                        // root article allowed, including the one
                        // concatenated below
                        const std::size_t last_ref = refs_buf->rfind('<');
                        if (last_ref != std::string::npos && last_ref > 0)
                        {
                            const std::size_t prior_ref = refs_buf->rfind('<', last_ref - 1);
                            if (prior_ref != std::string::npos && prior_ref > 0)
                            {
                                const std::size_t second_ref = refs_buf->find('<', 1);
                                if (second_ref < prior_ref)
                                {
                                    refs_buf->erase(second_ref, prior_ref - second_ref);
                                }
                            }
                        }
                    }
                    if (!artid_buf)
                    {
                        artid_buf = fetch_lines(g_art, MSG_ID_LINE);
                    }
                    if (!refs_buf)
                    {
                        refs_buf.emplace();
                    }
                    if (!refs_buf->empty())
                    {
                        refs_buf->push_back(' ');
                    }
                    if (!artid_buf->empty() && artid_buf->front() == '<')
                    {
                        refs_buf->append(*artid_buf);
                    }
                    else if (!artid_buf->empty())
                    {
                        refs_buf->push_back('<');
                        refs_buf->append(*artid_buf);
                        refs_buf->push_back('>');
                    }
                    set_value(*refs_buf);
                    break;
                }

                case 's':
                case 'S':
                {
                    if (!g_in_ng || !g_art || !g_artp)
                    {
                        set_value({});
                        break;
                    }
                    if (!subj_buf)
                    {
                        subj_buf = fetch_subj_copy(g_art);
                    }
                    std::string_view subject{*subj_buf};
                    if (pattern.front() == 's')
                    {
                        subject_has_re(subject, subject);
                        const std::size_t notes_file = subject.find("- (nf");
                        if (notes_file != std::string_view::npos)
                        {
                            subject = subject.substr(0, notes_file);
                        }
                    }
                    set_value(subject);
                    break;
                }

                case 't':
                case 'T':
                {
                    if (!g_in_ng)
                    {
                        set_value({});
                        break;
                    }
                    parse_header(g_art);
                    std::string_view author;
                    if (g_header_type[REPLY_LINE].min_pos >= 0)
                    {
                        // was there a reply line?
                        if (!reply_buf)
                        {
                            reply_buf = fetch_lines(g_art, REPLY_LINE);
                        }
                        author = *reply_buf;
                    }
                    else if (!from_buf)
                    {
                        from_buf = fetch_lines(g_art, FROM_LINE);
                        author = *from_buf;
                    }
                    else
                    {
                        author = noname_text;
                    }
                    if (pattern.front() == 'T')
                    {
                        if (g_header_type[PATH_LINE].min_pos >= 0)
                        {
                            // should we substitute path?
                            path_buf = fetch_lines(g_art, PATH_LINE);
                            author = *path_buf;
                        }
                        const std::string_view host{g_p_host_name};
                        if (author.size() > host.size() && author.substr(0, host.size()) == host &&
                            author[host.size()] == '!')
                        {
                            author.remove_prefix(host.size() + 1);
                        }
                    }
                    set_value(author);
                    address_parse = true; // just the good part
                    break;
                }

                case 'u':
                    if (g_in_ng)
                    {
                        set_owned_value(std::to_string(g_newsgroup_ptr->m_to_read));
                    }
                    else
                    {
                        set_value({});
                    }
                    break;

                case 'U':
                {
                    if (!g_in_ng)
                    {
                        set_value({});
                        break;
                    }
                    const bool unseen = g_art <= g_last_art && !was_read(g_art);
                    if (g_selected_only)
                    {
                        const bool selected = g_curr_artp != nullptr && (g_curr_artp->m_flags & AF_SEL) != AF_NONE;
                        set_owned_value(std::to_string(g_selected_count - (selected && unseen ? 1 : 0)));
                    }
                    else
                    {
                        set_owned_value(std::to_string(g_newsgroup_ptr->m_to_read - (unseen ? 1 : 0)));
                    }
                    break;
                }

                case 'v':
                {
                    if (g_in_ng)
                    {
                        const bool selected = g_curr_artp && g_curr_artp->m_flags & AF_SEL;
                        const bool unseen = g_art <= g_last_art && !was_read(g_art);
                        set_owned_value(std::to_string(g_newsgroup_ptr->m_to_read - g_selected_count -
                                                       (!selected && unseen ? 1 : 0)));
                    }
                    else
                    {
                        set_value({});
                    }
                    break;
                }

                case 'V':
                    set_value(g_patch_level);
                    break;

                case 'x': // news library
                    set_value(g_lib);
                    break;

                case 'X': // rn library
                    set_value(g_rn_lib);
                    break;

                case 'y': // from line with *-shortening
                    if (!g_in_ng)
                    {
                        set_value({});
                        break;
                    }
                    {
                        from_buf = fetch_lines(g_art, FROM_LINE);
                        const std::string_view from{*from_buf};
                        const std::size_t      at_pos = from.find_first_of("@ ");
                        if (at_pos != std::string_view::npos && from[at_pos] == '@') // we have normal form...
                        {
                            const std::size_t domain_start = at_pos + 1;
                            const std::size_t address_end = from.find(' ', domain_start);
                            const std::size_t domain_end =
                                address_end == std::string_view::npos ? from.size() : address_end;
                            const std::size_t last_dot =
                                domain_start < domain_end ? from.rfind('.', domain_end - 1) : std::string_view::npos;
                            if (last_dot != std::string_view::npos && last_dot >= domain_start)
                            {
                                const std::size_t suffix_pos = from.rfind('.', last_dot - 1);
                                if (suffix_pos != std::string_view::npos && suffix_pos >= domain_start)
                                {
                                    std::string shortened_from;
                                    shortened_from.reserve(1024);
                                    shortened_from.append(from, 0, domain_start);
                                    shortened_from.push_back('*');
                                    shortened_from.append(from, suffix_pos, std::string::npos);
                                    set_owned_value(std::move(shortened_from));
                                    break;
                                }
                            }
                        }
                        set_value(from);
                    }
                    break;

                case 'Y':
                    set_value(g_tmp_dir);
                    break;

                case 'z':
                    if (!g_in_ng)
                    {
                        set_value({});
                        break;
                    }
                    set_owned_value(fmt::format("{:>5}", fs::file_size(std::to_string(g_art.value_of()))));
                    break;

                case 'Z':
                    if (!g_in_ng)
                    {
                        set_value({});
                    }
                    else
                    {
                        set_owned_value(std::to_string(g_selected_count));
                    }
                    break;

                default:
                    set_owned_value(std::string(1, pattern.front()));
                    break;
                }
            }
            if (proc_sprintf)
            {
                set_owned_value(format_value(format_spec.c_str(), value));
            }
            if (!pattern.empty() && pattern.front() != '\0')
            {
                pattern.remove_prefix(1);
            }
            if (upper || lastcomp)
            {
                char       *mutable_s = make_mutable_text();
                std::size_t pos = upper ? 0 : transform_text.rfind('/');
                if (pos == std::string::npos)
                {
                    pos = 0;
                }
                while (pos < transform_text.size() && !std::isalpha(transform_text[pos]))
                {
                    pos++;
                }
                if (pos < transform_text.size() && std::islower(transform_text[pos]))
                {
                    transform_text[pos] = static_cast<char>(std::toupper(transform_text[pos]));
                }
                set_value(mutable_s);
            }

            // A maze of twisty little conditions, all alike...
            if (address_parse || comment_parse)
            {
                char *mutable_s = make_mutable_text();
                decode_header(mutable_s, mutable_s);
                char *start = mutable_s;
                if (address_parse)
                {
                    char *h = std::strchr(mutable_s, '<');
                    if (h != nullptr) // grab the good part
                    {
                        char *value_start = h + 1;
                        start = value_start;
                        if ((h = std::strchr(value_start, '>')) != nullptr)
                        {
                            *h = '\0';
                        }
                    }
                    else if ((h = std::strchr(mutable_s, '(')) != nullptr)
                    {
                        while (h-- != start && *h == ' ')
                        {
                        }
                        h[1] = '\0'; // or strip the comment
                    }
                }
                else
                {
                    char *name = extract_name(mutable_s);
                    if (name != nullptr)
                    {
                        start = name;
                    }
                    else
                    {
                        start = mutable_s + std::strlen(mutable_s);
                    }
                }
                set_value(start);
            }
            if (metabit)
            {
                const int meta_mask = metabit;
                for (char ch : value)
                {
                    result.push_back(static_cast<char>(ch | meta_mask));
                }
            }
            else if (re_quote || tick_quote)
            {
                for (std::size_t pos = 0; pos < value.size(); pos++)
                {
                    const char ch = value[pos];
                    if ((re_quote && std::strchr(s_regexp_specials, ch)) //
                        || (tick_quote == 2 && ch == '"'))
                    {
                        result.push_back('\\');
                    }
                    else if (re_quote && ch == ' ' && pos + 1 < value.size() && value[pos + 1] == ' ')
                    {
                        result += " *";
                        while (pos + 1 < value.size() && value[pos + 1] == ' ')
                        {
                            pos++;
                        }
                        continue;
                    }
                    else if (tick_quote && ch == '\'')
                    {
                        result += "'\\'";
                    }
                    result.push_back(ch);
                }
            }
            else
            {
                result.append(value);
            }
        }
        else
        {
            if (pattern.front() == '^' && pattern.size() > 1 && pattern[1] != '\0')
            {
                const char escaped = pattern[1];
                if (escaped == '?')
                {
                    result.push_back(static_cast<char>('\177' | metabit));
                    pattern.remove_prefix(2);
                }
                else if (escaped == '(')
                {
                    metabit = 0200;
                    pattern.remove_prefix(2);
                }
                else if (escaped == ')')
                {
                    metabit = 0;
                    pattern.remove_prefix(2);
                }
                else if (escaped >= '@')
                {
                    result.push_back(static_cast<char>((escaped & 037) | metabit));
                    pattern.remove_prefix(2);
                }
                else
                {
                    result.push_back(static_cast<char>(pattern.front() | metabit));
                    pattern.remove_prefix(1);
                }
            }
            else if (pattern.front() == '\\' && pattern.size() > 1 && pattern[1] != '\0')
            {
                pattern.remove_prefix(1);
                result.push_back(static_cast<char>(read_backslash(pattern) | metabit));
            }
            else
            {
                result.push_back(static_cast<char>(pattern.front() | metabit));
                pattern.remove_prefix(1);
            }
        }
    }
    if (line_split && result.size() > 79)
    {
        result[*line_split] = '\n';
    }
getout:
    return result;
}

std::string interp_search(std::string_view pattern, std::string_view cmd)
{
    std::string_view cursor{pattern};
    return do_interp(cursor, {}, cmd);
}

/// @brief Converts escape sequences in a pattern to their corresponding characters.
///
/// This function processes escape sequences in the input pattern and converts them
/// to their corresponding characters, such as '\n' to newline, '\t' to tab, etc.
/// It also handles octal and hexadecimal escape sequences.
///
/// @param dest Pointer to the destination buffer where the converted character will be stored.
/// @param pattern Pointer to the input pattern containing escape sequences.
/// @return Pointer to the next character in the pattern after the processed escape sequence.
///
const char *interp_backslash(char *dest, const char *pattern)
{
    int i = *pattern;

    if (i >= '0' && i <= '7')
    {
        i = 0;
        while (i < 01000 && *pattern >= '0' && *pattern <= '7')
        {
            i <<= 3;
            i += *pattern++ - '0';
        }
        *dest = (char) (i & 0377);
        return pattern - 1;
    }
    switch (i)
    {
    case 'a':
        *dest = '\a';
        break;

    case 'b':
        *dest = '\b';
        break;

    case 'f':
        *dest = '\f';
        break;

    case 'n':
        *dest = '\n';
        break;

    case 'r':
        *dest = '\r';
        break;

    case 't':
        *dest = '\t';
        break;

    case 'v':
        *dest = '\v';
        break;

    case 'x':
        if (std::isxdigit(pattern[1]))
        {
            i = 0;
            while (i < 01000 && std::isxdigit(*++pattern))
            {
                static constexpr char hex_digits[]{"0123456789ABCDEF"};
                i <<= 4;
                i += std::strchr(hex_digits, std::toupper(*pattern)) - hex_digits;
            }
            *dest = static_cast<char>(i & 0377);
            return pattern - 1;
        }
        break;

    case '\0':
        *dest = '\\';
        return pattern - 1;

    default:
        *dest = (char) i;
        break;
    }
    return pattern;
}

char *interp_backslash(char *dest, char *pattern)
{
    return pattern + (interp_backslash(dest, static_cast<const char *>(pattern)) - pattern);
}

// normalize a references line in place

void normalize_refs(char *refs)
{
    char* t = refs;

    for (char *f = refs; *f;)
    {
        if (*f == '<')
        {
            while (*f && (*t++ = *f++) != '>')
            {
            }
            while (is_hor_space(*f) || *f == '\n' || *f == ',')
            {
                f++;
            }
            if (f != t)
            {
                *t++ = ' ';
            }
        }
        else
        {
            f++;
        }
    }
    if (t != refs && t[-1] == ' ')
    {
        t--;
    }
    *t = '\0';
}
