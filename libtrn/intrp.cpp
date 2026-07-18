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
#include <optional>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

std::string g_orig_dir;    // cwd when rn invoked
std::string g_host_name;   // host name to match local postings
std::string g_head_name;
int         g_perform_count{};

#ifdef HAS_NEWS_ADMIN
const std::string g_news_admin{NEWS_ADMIN}; // news administrator
int               g_news_uid{};
#endif

static const char *skip_interp(const char *pattern, const char *stoppers);
static void abort_interp();

static const char   *s_regexp_specials = "^$.*[\\/?%";
static CompiledRegex s_cond_compex;
static constexpr char s_empty[]{""};
static std::string   s_last_input;

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

static const char *skip_interp(const char *pattern, const char *stoppers)
{
#ifdef DEBUG
    if (g_debug & DEB_INTRP)
    {
        std::printf("skipinterp %s (till %s)\n",pattern,stoppers?stoppers:"");
    }
#endif

    while (*pattern && (!stoppers || !std::strchr(stoppers, *pattern)))
    {
        if (*pattern == '%' && pattern[1])
        {
switch_again:
            switch (*++pattern)
            {
            case '^':
            case '_':
            case '\\':
            case '\'':
            case '>':
            case ')':
                goto switch_again;

            case ':':
                pattern++;
                while (*pattern //
                       && (*pattern == '.' || *pattern == '-' || isdigit(*pattern)))
                {
                    pattern++;
                }
                pattern--;
                goto switch_again;

            case '{':
                for (pattern++; *pattern && *pattern != '}'; pattern++)
                {
                    if (*pattern == '\\')
                    {
                        pattern++;
                    }
                }
                break;

            case '[':
                for (pattern++; *pattern && *pattern != ']'; pattern++)
                {
                    if (*pattern == '\\')
                    {
                        pattern++;
                    }
                }
                break;

            case '(':
            {
                pattern = skip_interp(pattern+1,"!=");
                if (!*pattern)
                {
                    goto getout;
                }
                for (pattern++; *pattern && *pattern != '?'; pattern++)
                {
                    if (*pattern == '\\')
                    {
                        pattern++;
                    }
                }
                if (!*pattern)
                {
                    goto getout;
                }
                pattern = skip_interp(pattern+1,":)");
                if (*pattern == ':')
                {
                    pattern = skip_interp(pattern + 1, ")");
                }
                break;
            }

            case '`':
            {
                pattern = skip_interp(pattern+1,"`");
                break;
            }

            case '"':
                pattern = skip_interp(pattern+1,"\"");
                break;

            default:
                break;
            }
            pattern++;
        }
        else
        {
            if (*pattern == '^' //
                && ((Uchar) pattern[1] >= '?' || pattern[1] == '(' || pattern[1] == ')'))
            {
                pattern += 2;
            }
            else if (*pattern == '\\' && pattern[1])
            {
                pattern += 2;
            }
            else
            {
                pattern++;
            }
        }
    }
getout:
    return pattern;                     // where we left off
}

// interpret interpolations
const char *do_interp(char *dest, int dest_size, const char *pattern, const char *stoppers, const char *cmd)
{
    std::optional<std::string> subj_buf;
    std::optional<std::string> ngs_buf;
    std::optional<std::string> refs_buf;
    std::optional<std::string> artid_buf;
    std::optional<std::string> reply_buf;
    std::optional<std::string> from_buf;
    std::optional<std::string> path_buf;
    std::optional<std::string> follow_buf;
    std::optional<std::string> dist_buf;
    std::optional<std::string> line_buf;
    char                      *line_split = nullptr;
    char                      *orig_dest = dest;
    const std::size_t          scratch_size = 8192;
    const std::size_t          format_size = 512;
    std::string                scratch;
    const char                *space_text = " ";
    const char                *noname_text = "noname";
    int                        metabit = 0;

    scratch.reserve(scratch_size);
    const auto assign_scratch = [&scratch](std::string_view text) -> const char *
    {
        scratch.assign(text);
        return scratch.c_str();
    };
    const auto make_scratch_buffer = [&scratch, scratch_size]() -> char *
    {
        scratch.assign(scratch_size, '\0');
        return scratch.data();
    };
    const auto trim_scratch = [&scratch]()
    {
        const std::size_t end = scratch.find('\0');
        if (end != std::string::npos)
        {
            scratch.resize(end);
        }
    };
    const auto copy_till_scratch = [&scratch](const char *from, int delim) -> const char *
    {
        scratch.clear();
        while (*from)
        {
            if (*from == '\\' && from[1] == delim)
            {
                from++;
            }
            else if (*from == delim)
            {
                break;
            }
            scratch.push_back(*from++);
        }
        return from;
    };
    const auto do_interp_scratch = [&make_scratch_buffer, &scratch, &trim_scratch,
                                    cmd](const char *interp_pattern, const char *interp_stoppers) -> const char *
    {
        char       *buffer = make_scratch_buffer();
        const int   buffer_size = static_cast<int>(scratch.size());
        const char *next = do_interp(buffer, buffer_size, interp_pattern, interp_stoppers, cmd);
        trim_scratch();
        return next;
    };
    const auto read_scratch_line = [&make_scratch_buffer, &scratch, &trim_scratch](std::FILE *fp) -> bool
    {
        char     *buffer = make_scratch_buffer();
        const int buffer_size = static_cast<int>(scratch.size());
        if (std::fgets(buffer, buffer_size, fp) == nullptr)
        {
            scratch.clear();
            return false;
        }
        trim_scratch();
        return true;
    };

    while (*pattern && (!stoppers || !std::strchr(stoppers, *pattern)))
    {
        if (*pattern == '%' && pattern[1])
        {
            std::string env_value;
            std::string format_spec;
            std::string search_command;
            std::string transform_text;
            std::string format_input;
            bool        upper = false;
            bool        lastcomp = false;
            bool        re_quote = false;
            int         tick_quote = 0;
            bool        address_parse = false;
            bool        comment_parse = false;
            bool        proc_sprintf = false;
            const char *s = nullptr;
            const auto  make_mutable_text = [&s, &scratch, &transform_text]() -> char *
            {
                if (s != scratch.data() && s != transform_text.data())
                {
                    transform_text = s;
                    s = transform_text.c_str();
                }
                return s == scratch.data() ? scratch.data() : transform_text.data();
            };
            const auto format_scratch = [&scratch](const char *format, const char *value) -> const char *
            {
                const int size = std::snprintf(nullptr, 0, format, value);
                if (size < 0)
                {
                    scratch.clear();
                    return scratch.c_str();
                }
                scratch.assign(static_cast<std::size_t>(size) + 1, '\0');
                std::snprintf(scratch.data(), scratch.size(), format, value);
                scratch.resize(static_cast<std::size_t>(size));
                return scratch.c_str();
            };
            while (s == nullptr)
            {
                switch (*++pattern)
                {
                case '^':
                    upper = true;
                    break;

                case '_':
                    lastcomp = true;
                    break;

                case '\\':
                    re_quote = true;
                    break;

                case '\'':
                    tick_quote++;
                    break;

                case '>':
                    address_parse = true;
                    break;

                case ')':
                    comment_parse = true;
                    break;

                case ':':
                {
                    proc_sprintf = true;
                    format_spec.reserve(format_size);
                    format_spec = '%';
                    pattern++;      // Skip over ':'
                    while (*pattern //
                           && (*pattern == '.' || *pattern == '-' || isdigit(*pattern)))
                    {
                        format_spec += *pattern++;
                    }
                    format_spec += 's';
                    pattern--;
                    break;
                }

                case '/':
                {
                    search_command.reserve(scratch_size);
                    if (!cmd || !std::strchr("/?g", *cmd))
                    {
                        search_command += '/';
                    }
                    search_command += g_last_pat;
                    if (!cmd || *cmd != 'g')
                    {
                        if (cmd && std::strchr("/?", *cmd))
                        {
                            search_command += *cmd;
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
                    s = search_command.c_str();
                    break;
                }

                case '{':
                {
                    const char *pattern_start = pattern + 1;
                    pattern = copy_till_scratch(pattern_start, '}');
                    const char *m = std::strchr(scratch.c_str(), '-');
                    if (m != nullptr)
                    {
                        scratch[static_cast<std::size_t>(m - scratch.c_str())] = '\0';
                        m++;
                    }
                    else
                    {
                        m = s_empty;
                    }
                    env_value = get_env_var(scratch.c_str(), m);
                    s = env_value.c_str();
                    break;
                }

                case '<':
                {
                    const char *pattern_start = pattern + 1;
                    pattern = copy_till_scratch(pattern_start, '>');
                    s = std::strchr(scratch.c_str(), '-');
                    if (s != nullptr)
                    {
                        scratch[static_cast<std::size_t>(s - scratch.c_str())] = '\0';
                        s++;
                    }
                    else
                    {
                        s = s_empty;
                    }
                    env_value = get_env_var(scratch.c_str(), s);
                    env_value = do_interp(env_value);
                    s = env_value.c_str();
                    break;
                }

                case '[':
                {
                    const char *pattern_start = pattern + 1;
                    pattern = copy_till_scratch(pattern_start, ']');
                    if (g_in_ng)
                    {
                        HeaderLineType which_line;
                        if (!scratch.empty() && (which_line = get_header_num(scratch.c_str())) != SOME_LINE)
                        {
                            line_buf = fetch_lines(g_art, which_line);
                            s = line_buf->c_str();
                        }
                        else
                        {
                            s = s_empty;
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;
                }

                case '(':
                {
                    CompiledRegex *oldbra_compex = g_bra_compex;
                    char rch;
                    bool matched;

                    pattern = do_interp(dest,dest_size,pattern+1,"!=",cmd);
                    rch = *pattern;
                    if (rch == '!')
                    {
                        pattern++;
                    }
                    if (*pattern != '=')
                    {
                        goto getout;
                    }
                    const char *pattern_start = pattern + 1;
                    pattern = copy_till_scratch(pattern_start, '?');
                    if (!*pattern)
                    {
                        goto getout;
                    }
                    s = scratch.c_str();
                    format_spec.clear();
                    format_spec.reserve(format_size);
                    proc_sprintf = false;
                    for (const char *scan = s; *scan; scan++)
                    {
                        switch (*scan)
                        {
                        case '^':
                            format_spec += '\\';
                            break;

                        case '\\':
                            format_spec += '\\';
                            format_spec += '\\';
                            break;

                        case '%':
                            proc_sprintf = true;
                            break;
                        }
                        format_spec += *scan;
                    }
                    if (proc_sprintf)
                    {
                        do_interp_scratch(format_spec.c_str(), nullptr);
                        proc_sprintf = false;
                    }
                    const char *compile_error = s_cond_compex.compile(scratch.c_str(), true, true);
                    if (compile_error != nullptr)
                    {
                        fmt::print("{}: {}\n", scratch, compile_error);
                        pattern += std::strlen(pattern);
                        s_cond_compex.free_compex();
                        goto getout;
                    }
                    matched = s_cond_compex.execute(dest) != nullptr;
                    if (s_cond_compex.get_bracket(0)) // were there brackets?
                    {
                        g_bra_compex = &s_cond_compex;
                    }
                    if (matched == (rch == '='))
                    {
                        pattern = do_interp(dest, dest_size, pattern + 1, ":)", cmd);
                        if (*pattern == ':')
                        {
                            const char *pattern_start = pattern + 1;
                            pattern = pattern_start + (skip_interp(pattern_start, ")") - pattern_start);
                        }
                    }
                    else
                    {
                        const char *pattern_start = pattern + 1;
                        pattern = pattern_start + (skip_interp(pattern_start, ":)") - pattern_start);
                        if (*pattern == ':')
                        {
                            pattern++;
                        }
                        pattern = do_interp(dest, dest_size, pattern, ")", cmd);
                    }
                    s = dest;
                    g_bra_compex = oldbra_compex;
                    s_cond_compex.free_compex();
                    break;
                }

                case '`':
                {
                    pattern = do_interp_scratch(pattern + 1, "`");
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
                    s = scratch.c_str();
                    break;
                }

                case '"':
                {
                    pattern = do_interp_scratch(pattern + 1, "\"");
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
                    s = scratch.c_str();
                    break;
                }

                case '~':
                    s = assign_scratch(g_home_dir);
                    break;

                case '.':
                    s = assign_scratch(g_dot_dir);
                    break;

                case '+':
                    s = assign_scratch(g_trn_dir);
                    break;

                case '$':
                    s = assign_scratch(std::to_string(g_our_pid));
                    break;

                case '#':
                    if (upper)
                    {
                        static int counter = 0;
                        s = assign_scratch(std::to_string(++counter));
                    }
                    else
                    {
                        s = assign_scratch(std::to_string(g_perform_count));
                    }
                    break;

                case '?':
                    s = space_text;
                    line_split = dest;
                    break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    s = assign_scratch(g_bra_compex->get_bracket(*pattern - '0'));
                    break;

                case 'a':
                    if (g_in_ng)
                    {
                        s = assign_scratch(std::to_string(g_art.value_of()));
                    }
                    else
                    {
                        s = s_empty;
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
                                s = assign_scratch(
                                    fmt::format("{}/{}", g_data_source->m_spool_dir, nntp_art_name(g_art, false)));
                            }
                            else
                            {
                                s = s_empty;
                            }
                        }
                        else
                        {
                            s = assign_scratch(
                                fmt::format("{}/{}/{}", g_data_source->m_spool_dir, g_newsgroup_dir, g_art.value_of()));
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'b':
                    s = assign_scratch(g_save_dest);
                    break;

                case 'B':
                    s = assign_scratch(std::to_string(g_save_from.value_of()));
                    break;

                case 'c':
                    s = assign_scratch(g_newsgroup_dir);
                    break;

                case 'C':
                    s = assign_scratch(g_newsgroup_name);
                    break;

                case 'd':
                    if (!g_newsgroup_dir.empty())
                    {
                        s = assign_scratch(fmt::format("{}/{}", g_data_source->m_spool_dir, g_newsgroup_dir));
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'D':
                    if (g_in_ng)
                    {
                        dist_buf = fetch_lines(g_art, DIST_LINE);
                        s = dist_buf->c_str();
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'e':
                {
                    if (g_extract_prog.empty())
                    {
                        s = "-";
                    }
                    else
                    {
                        s = assign_scratch(g_extract_prog);
                    }
                    break;
                }

                case 'E':
                    if (g_extract_dest.empty())
                    {
                        s = s_empty;
                    }
                    else
                    {
                        s = assign_scratch(g_extract_dest);
                    }
                    break;

                case 'f':                       // from line
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
                            s = reply_buf->c_str();
                        }
                        else
                        {
                            if (!from_buf)
                            {
                                from_buf = fetch_lines(g_art, FROM_LINE);
                            }
                            s = from_buf->c_str();
                        }
                    }
                    else
                    {
                        s = s_empty;
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
                            s = follow_buf->c_str();
                        }
                        else
                        {
                            ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
                            s = ngs_buf->c_str();
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'g':                       // general mode
                    scratch.assign(1, static_cast<char>(g_general_mode));
                    s = scratch.c_str();
                    break;

                case 'h':                       // header file name
                    s = assign_scratch(g_head_name);
                    break;

                case 'H':                       // host name in postings
                    s = assign_scratch(g_p_host_name);
                    break;

                case 'i':
                    if (g_in_ng)
                    {
                        if (!artid_buf)
                        {
                            artid_buf = fetch_lines(g_art, MSG_ID_LINE);
                        }
                        s = artid_buf->c_str();
                        if (*s && *s != '<')
                        {
                            s = assign_scratch(fmt::format("<{}>", *artid_buf));
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'I':                       // indent string for quoting
                    s = assign_scratch(fmt::format("'{}'", g_indent_string));
                    break;

                case 'j':
                    s = assign_scratch(std::to_string(g_just_a_sec * 10));
                    break;

                case 'l':                       // news admin login
#ifdef HAS_NEWS_ADMIN
                    s = assign_scratch(g_news_admin);
#else
                    s = "???";
#endif
                    break;

                case 'L':                       // login id
                    s = assign_scratch(g_login_name);
                    break;

                case 'm':               // current mode
                    scratch.assign(1, static_cast<char>(g_mode));
                    s = scratch.c_str();
                    break;

                case 'M':
                    s = assign_scratch(std::to_string(g_dm_count));
                    break;

                case 'n':                       // newsgroups
                    if (g_in_ng)
                    {
                        ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
                        s = ngs_buf->c_str();
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'N':                       // full name
                    env_value = get_env_var("NAME", g_real_name);
                    s = env_value.c_str();
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
                    s = env_value.c_str();
                    const std::string org_file = file_exp(s);
                    if (FILE_REF(org_file.c_str()))
                    {
                        std::FILE *ofp = std::fopen(org_file.c_str(), "r");

                        if (ofp)
                        {
                            read_scratch_line(ofp);
                            std::fclose(ofp);
                            if (!scratch.empty() && scratch.back() == '\n')
                            {
                                scratch.pop_back();
                            }
                            s = scratch.c_str();
                        }
                        else
                        {
                            s = s_empty;
                        }
                    }
                    break;
                }

                case 'O':
                    s = assign_scratch(g_orig_dir);
                    break;

                case 'p':
                    s = assign_scratch(g_priv_dir);
                    break;

                case 'P':
                    if (g_data_source)
                    {
                        s = assign_scratch(g_data_source->m_spool_dir);
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'q':
                    s = assign_scratch(s_last_input);
                    break;

                case 'r':
                    if (g_in_ng)
                    {
                        parse_header(g_art);
                        refs_buf.reset();
                        if (g_header_type[REFS_LINE].min_pos >= 0)
                        {
                            refs_buf = fetch_lines(g_art,REFS_LINE);
                            normalize_refs(refs_buf->data());
                            s = std::strrchr(refs_buf->data(), '<');
                            if (s != nullptr)
                            {
                                break;
                            }
                        }
                    }
                    s = s_empty;
                    break;

                case 'R':
                {
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    parse_header(g_art);
                    refs_buf.reset();
                    if (g_header_type[REFS_LINE].min_pos >= 0)
                    {
                        refs_buf = fetch_lines(g_art,REFS_LINE);
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
                    if (!artid_buf->empty() && (*artid_buf)[0] == '<')
                    {
                        refs_buf->append(*artid_buf);
                    }
                    else if (!artid_buf->empty())
                    {
                        refs_buf->push_back('<');
                        refs_buf->append(*artid_buf);
                        refs_buf->push_back('>');
                    }
                    s = refs_buf->c_str();
                    break;
                }

                case 's':
                case 'S':
                {
                    if (!g_in_ng || !g_art || !g_artp)
                    {
                        s = s_empty;
                        break;
                    }
                    if (!subj_buf)
                    {
                        subj_buf = fetch_subj_copy(g_art);
                    }
                    std::string_view subject{*subj_buf};
                    if (*pattern == 's')
                    {
                        subject_has_re(subject, subject);
                    }
                    char *str = subj_buf->data() + (subj_buf->size() - subject.size());
                    char *h;
                    if (*pattern == 's' && (h = in_string(str, "- (nf", true)) != nullptr)
                    {
                        *h = '\0';
                    }
                    s = str;
                    break;
                }

                case 't':
                case 'T':
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    parse_header(g_art);
                    if (g_header_type[REPLY_LINE].min_pos >= 0)
                    {
                                        // was there a reply line?
                        if (!reply_buf)
                        {
                            reply_buf = fetch_lines(g_art, REPLY_LINE);
                        }
                        s = reply_buf->c_str();
                    }
                    else if (!from_buf)
                    {
                        from_buf = fetch_lines(g_art, FROM_LINE);
                        s = from_buf->c_str();
                    }
                    else
                    {
                        s = noname_text;
                    }
                    if (*pattern == 'T')
                    {
                        if (g_header_type[PATH_LINE].min_pos >= 0)
                        {
                                        // should we substitute path?
                            path_buf = fetch_lines(g_art, PATH_LINE);
                            s = path_buf->c_str();
                        }
                        int i = std::strlen(g_p_host_name.c_str());
                        if (!std::strncmp(g_p_host_name.c_str(),s,i) && s[i] == '!')
                        {
                            s += i + 1;
                        }
                    }
                    address_parse = true;       // just the good part
                    break;

                case 'u':
                    if (g_in_ng)
                    {
                        s = assign_scratch(std::to_string(g_newsgroup_ptr->m_to_read));
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'U':
                {
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    const bool unseen = g_art <= g_last_art && !was_read(g_art);
                    if (g_selected_only)
                    {
                        const bool selected = g_curr_artp != nullptr && (g_curr_artp->m_flags & AF_SEL) != AF_NONE;
                        s = assign_scratch(std::to_string(g_selected_count - (selected && unseen ? 1 : 0)));
                    }
                    else
                    {
                        s = assign_scratch(std::to_string(g_newsgroup_ptr->m_to_read - (unseen ? 1 : 0)));
                    }
                    break;
                }

                case 'v':
                {
                    if (g_in_ng)
                    {
                        const bool selected = g_curr_artp && g_curr_artp->m_flags & AF_SEL;
                        const bool unseen = g_art <= g_last_art && !was_read(g_art);
                        s = assign_scratch(std::to_string(g_newsgroup_ptr->m_to_read - g_selected_count -
                                                          (!selected && unseen ? 1 : 0)));
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;
                }

                case 'V':
                    s = assign_scratch(g_patch_level);
                    break;

                case 'x':                           // news library
                    s = assign_scratch(g_lib);
                    break;

                case 'X':                           // rn library
                    s = assign_scratch(g_rn_lib);
                    break;

                case 'y':       // from line with *-shortening
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    {
                        from_buf = fetch_lines(g_art, FROM_LINE);
                        char *at = from_buf->data();
                        char *s3 = nullptr;
                        int   i = 0;

                        for (; (*at && (*at != '@') && (*at != ' ')); at++)
                        {
                        }
                        if (*at == '@')         // we have normal form...
                        {
                            for (s3 = at + 1; (*s3 && (*s3 != ' ')); s3++)
                            {
                                if (*s3 == '.')
                                {
                                    i++;
                                }
                            }
                        }
                        if (i > 1)   // more than one dot
                        {
                            s3 = at;    // will be incremented before use
                            while (i >= 2)
                            {
                                s3++;
                                if (*s3 == '.')
                                {
                                    i--;
                                }
                            }
                            const std::size_t replace_pos = static_cast<std::size_t>(at + 1 - from_buf->data());
                            const std::size_t suffix_pos = static_cast<std::size_t>(s3 - from_buf->data());
                            std::string       shortened_from;
                            shortened_from.reserve(1024);
                            shortened_from.append(*from_buf, 0, replace_pos);
                            shortened_from.push_back('*');
                            shortened_from.append(*from_buf, suffix_pos, std::string::npos);
                            from_buf = std::move(shortened_from);
                        }
                        s = from_buf->c_str();
                    }
                    break;

                case 'Y':
                    s = assign_scratch(g_tmp_dir);
                    break;

                case 'z':
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    s = assign_scratch(fmt::format("{:>5}", fs::file_size(std::to_string(g_art.value_of()))));
                    break;

                case 'Z':
                    if (!g_in_ng)
                    {
                        s = s_empty;
                    }
                    else
                    {
                        s = assign_scratch(std::to_string(g_selected_count));
                    }
                    break;

                case '\0':
                    s = s_empty;
                    break;

                default:
                    if (--dest_size <= 0)
                    {
                        abort_interp();
                    }
                    *dest++ = *pattern | metabit;
                    s = s_empty;
                    break;
                }
            }
            if (proc_sprintf)
            {
                if (s == scratch.data())
                {
                    format_input.reserve(scratch_size);
                    format_input = scratch;
                    s = format_input.c_str();
                }
                s = format_scratch(format_spec.c_str(), s);
            }
            if (*pattern)
            {
                pattern++;
            }
            if (upper || lastcomp)
            {
                char *mutable_s = make_mutable_text();
                char *t;
                if (upper || !(t = std::strrchr(mutable_s, '/')))
                {
                    t = mutable_s;
                }
                while (*t && !std::isalpha(*t))
                {
                    t++;
                }
                if (std::islower(*t))
                {
                    *t = std::toupper(*t);
                }
            }
            // Do we have room left?
            int i = std::strlen(s);
            if (dest_size <= i)
            {
                abort_interp();
            }
            dest_size -= i;      // adjust the size now.

            // A maze of twisty little conditions, all alike...
            if (address_parse || comment_parse)
            {
                char *mutable_s = make_mutable_text();
                decode_header(mutable_s, mutable_s);
                s = mutable_s;
                if (address_parse)
                {
                    char *h = std::strchr(mutable_s, '<');
                    if (h != nullptr)   // grab the good part
                    {
                        char *value_start = h + 1;
                        s = value_start;
                        if ((h = std::strchr(value_start, '>')) != nullptr)
                        {
                            *h = '\0';
                        }
                    }
                    else if ((h = std::strchr(mutable_s, '(')) != nullptr)
                    {
                        while (h-- != s && *h == ' ')
                        {
                        }
                        h[1] = '\0';            // or strip the comment
                    }
                }
                else
                {
                    char *name = extract_name(mutable_s);
                    if (name != nullptr)
                    {
                        s = name;
                    }
                    else
                    {
                        s = s_empty;
                    }
                }
            }
            if (metabit)
            {
                // set meta bit while copying.
                i = metabit;            // maybe get into register
                if (s == dest)
                {
                    while (*dest)
                    {
                        *dest++ |= i;
                    }
                }
                else
                {
                    while (*s)
                    {
                        *dest++ = *s++ | i;
                    }
                }
            }
            else if (re_quote || tick_quote)
            {
                // put a backslash before regexp specials while copying.
                if (s == dest)
                {
                    // copy out so we can copy in.
                    transform_text.reserve(scratch_size);
                    transform_text = s;
                    s = transform_text.c_str();
                }
                while (*s)
                {
                    if ((re_quote && std::strchr(s_regexp_specials, *s)) //
                        || (tick_quote == 2 && *s == '"'))
                    {
                        if (--dest_size <= 0)
                        {
                            abort_interp();
                        }
                        *dest++ = '\\';
                    }
                    else if (re_quote && *s == ' ' && s[1] == ' ')
                    {
                        *dest++ = ' ';
                        *dest++ = '*';
                        s = skip_eq(++s, ' ');
                        continue;
                    }
                    else if (tick_quote && *s == '\'')
                    {
                        if ((dest_size -= 3) <= 0)
                        {
                            abort_interp();
                        }
                        *dest++ = '\'';
                        *dest++ = '\\';
                        *dest++ = '\'';
                    }
                    *dest++ = *s++;
                }
            }
            else
            {
                // straight copy.
                if (s == dest)
                {
                    dest += i;
                }
                else
                {
                    while (*s)
                    {
                        *dest++ = *s++;
                    }
                }
            }
        }
        else
        {
            if (--dest_size <= 0)
            {
                abort_interp();
            }
            if (*pattern == '^' && pattern[1])
            {
                pattern++;
                if (*pattern == '?')
                {
                    *dest++ = '\177' | metabit;
                }
                else if (*pattern == '(')
                {
                    metabit = 0200;
                    dest_size++;
                }
                else if (*pattern == ')')
                {
                    metabit = 0;
                    dest_size++;
                }
                else if (*pattern >= '@')
                {
                    *dest++ = (*pattern & 037) | metabit;
                }
                else
                {
                    *dest++ = *--pattern | metabit;
                }
                pattern++;
            }
            else if (*pattern == '\\' && pattern[1])
            {
                ++pattern;              // skip backslash
                pattern = interp_backslash(dest, pattern) + 1;
                *dest++ |= metabit;
            }
            else if (*pattern)
            {
                *dest++ = *pattern++ | metabit;
            }
        }
    }
    *dest = '\0';
    if (line_split != nullptr)
    {
        if ((int) std::strlen(orig_dest) > 79)
        {
            *line_split = '\n';
        }
    }
getout:
    return pattern; // where we left off
}

std::string do_interp(std::string_view pattern)
{
    std::string pattern_text{pattern};
    std::string result(CMD_BUF_LEN, '\0');

    do_interp(result.data(), static_cast<int>(result.size()), pattern_text.c_str(), nullptr, nullptr);
    const std::size_t result_end = result.find('\0');
    if (result_end != std::string::npos)
    {
        result.resize(result_end);
    }
    return result;
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

// helper functions

const char *interp(char *dest, int dest_size, const char *pattern)
{
    return do_interp(dest, dest_size, pattern, nullptr, nullptr);
}

const char *interp_search(char *dest, int dest_size, const char *pattern, const char *cmd)
{
    return do_interp(dest, dest_size, pattern, nullptr, cmd);
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

static void abort_interp()
{
    std::fputs("\n% interp buffer overflow!\n",stdout);
    sig_catcher(0);
}
