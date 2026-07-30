/* util.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/util.h>

#include <config/common.h>
#include <config/env.h>
#include <config/fdio.h>
#include <nntp/nntpclient.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/IniDocument.h>
#include <trn/IniSection.h>
#include <trn/IniSectionValues.h>
#include <trn/intrp.h>
#include <trn/search.h>
#include <trn/smisc.h> // g_s_default_cmd
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/univ.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#ifdef I_SYS_TIMEB
#include <sys/timeb.h>
#endif
#ifdef I_SYS_WAIT
#include <sys/wait.h>
#endif
#ifdef MSDOS
#include <process.h>
#endif
#ifdef _WIN32
#include <sys/types.h>
#endif

#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

#ifdef UNION_WAIT
using WaitStatus = union wait;
#else
using WaitStatus = int;
#endif

bool g_waiting{}; // waiting for subprocess (in doshell)?
bool g_no_wait_fork{};

#ifndef USE_DEBUGGING_MALLOC
static constexpr char s_no_memory[] = "trn: out of memory!\n";
#endif

static constexpr std::string_view s_newsactive_env{"NEWSACTIVE"};
static constexpr std::string_view s_newsdescriptions_env{"NEWSDESCRIPTIONS"};
static constexpr std::string_view s_nntp_server_env{"NNTPSERVER"};
static constexpr std::string_view s_nntp_force_auth_env{"NNTP_FORCE_AUTH"};
static constexpr std::string_view s_quotechars_env{"QUOTECHARS"};
static constexpr std::string_view s_trn_version_env{"TRN_VERSION"};

void util_init()
{
    extern std::string g_patch_level;

    set_env_var(s_trn_version_env, g_patch_level);
}

void util_final()
{
    const std::string_view names[] = {
        s_newsactive_env, s_newsdescriptions_env, s_nntp_server_env,
        s_quotechars_env, s_nntp_force_auth_env,  s_trn_version_env,
    };

    for (const std::string_view name : names)
    {
        unset_env_var(name);
    }
}

// fork and exec a shell command

int do_shell(std::string_view shell, std::string_view cmd)
{
#ifndef MSDOS
    WaitStatus status;
    pid_t pid, w;
#endif
    int ret = 0;
    const std::string command_text{cmd};

    xmouse_off();

#ifdef SIGTSTP
    sigset(SIGTSTP,SIG_DFL);
    sigset(SIGTTOU,SIG_DFL);
    sigset(SIGTTIN,SIG_DFL);
#endif
    if (g_data_source && (g_data_source->m_flags & DF_REMOTE))
    {
        set_env_var(s_nntp_server_env, g_data_source->m_news_id);
        if (g_data_source->m_nntp_link.flags & NNTP_FORCE_AUTH_NEEDED)
        {
            set_env_var(s_nntp_force_auth_env, "yes");
        }
        else
        {
            unset_env_var(s_nntp_force_auth_env);
        }
        if (!g_data_source->m_auth_user.empty())
        {
            int fd = open(g_nntp_auth_file.c_str(), O_WRONLY | O_CREAT, 0600);
            if (fd >= 0)
            {
                write(fd, g_data_source->m_auth_user.c_str(), g_data_source->m_auth_user.size());
                write(fd, "\n", 1);
                if (!g_data_source->m_auth_pass.empty())
                {
                    write(fd, g_data_source->m_auth_pass.c_str(), g_data_source->m_auth_pass.size());
                    write(fd, "\n", 1);
                }
                close(fd);
            }
        }
        if (g_nntp_link.port_number)
        {
            const std::string nntp_server = fmt::format("{};{}", g_data_source->m_news_id, g_nntp_link.port_number);
            set_env_var(s_nntp_server_env, nntp_server);
        }
        if (g_data_source->m_act_sf.m_fp)
        {
            set_env_var(s_newsactive_env, g_data_source->m_extra_name);
        }
        else
        {
            set_env_var(s_newsactive_env, "none");
        }
    }
    else
    {
        unset_env_var(s_nntp_server_env);
        unset_env_var(s_nntp_force_auth_env);
        if (g_data_source)
        {
            set_env_var(s_newsactive_env, g_data_source->m_news_id);
        }
        else
        {
            unset_env_var(s_newsactive_env);
        }
    }
    if (g_data_source)
    {
        set_env_var(s_newsdescriptions_env, g_data_source->m_group_desc);
    }
    else
    {
        unset_env_var(s_newsdescriptions_env);
    }
    const std::string quotechars = do_interp("%I");
    TRN_ASSERT(quotechars.size() >= 2);
    set_env_var(s_quotechars_env, quotechars.substr(1, quotechars.size() - 2));
    const std::string shell_text = shell.empty() ? get_env_var("SHELL", PREF_SHELL) : std::string{shell};
    const char *const shell_path = shell_text.c_str();
    termlib_reset();
#ifdef MSDOS
    intptr_t status = spawnl(P_WAIT, shell_path, shell_path, "/c", command_text.c_str(), nullptr);
#else
    pid = vfork();
    if (pid == 0)
    {
        if (g_no_wait_fork)
        {
            close(1);
            close(2);
            dup(open("/dev/null",1));
        }

        if (!command_text.empty())
        {
            execl(shell_path, shell_path, "-c", command_text.c_str(), nullptr);
        }
        else
        {
            execl(shell_path, shell_path, nullptr, nullptr, nullptr);
        }
        _exit(127);
    }
    sigignore(SIGINT);
#ifdef SIGQUIT
    sigignore(SIGQUIT);
#endif
    g_waiting = true;
    while ((w = wait(&status)) != pid)
    {
        if (w == -1 && errno != EINTR)
        {
            break;
        }
    }
    if (w == -1)
    {
        ret = -1;
    }
    else
    {
#ifdef USE_WIFSTAT
        ret = WEXITSTATUS(status);
#else
#ifdef UNION_WAIT
        ret = status.w_status >> 8;
#else
        ret = status;
#endif // UNION_WAIT
#endif // USE_WIFSTAT
    }
#endif // !MSDOS
    termlib_init();
    xmouse_check();
    g_waiting = false;
    sigset(SIGINT,int_catcher);
#ifdef SIGQUIT
    sigset(SIGQUIT,SIG_DFL);
#endif
#ifdef SIGTSTP
    sigset(SIGTSTP,stop_catcher);
    sigset(SIGTTOU,stop_catcher);
    sigset(SIGTTIN,stop_catcher);
#endif
    if (g_data_source && !g_data_source->m_auth_user.empty())
    {
        std::error_code error;
        fs::remove(g_nntp_auth_file, error);
    }
    return ret;
}

// paranoid version of malloc

#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size)
{
    char *ptr = (char *) std::malloc(size ? size : (MemorySize) 1);
    if (!ptr)
    {
        fmt::print("{}", s_no_memory);
        sig_catcher(0);
    }
    return ptr;
}
#endif

// paranoid version of realloc.  If where is nullptr, call malloc

#ifndef USE_DEBUGGING_MALLOC
char *safe_realloc(char *where, MemorySize size)
{
    char *ptr;

    if (!where)
    {
        ptr = (char *) std::malloc(size ? size : (MemorySize) 1);
    }
    else
    {
        ptr = (char *) std::realloc(where, size ? size : (MemorySize) 1);
    }
    if (!ptr)
    {
        fmt::print("{}", s_no_memory);
        sig_catcher(0);
    }
    return ptr;
}
#endif // !USE_DEBUGGING_MALLOC

// effective access

int eaccess(const fs::path &filename, int mod)
{
#ifdef SETUIDGID
    int protection, euid;

    mod &= 7;                           // remove extraneous garbage
    if (stat(filename.string().c_str(), &g_filestat) < 0)
    {
        return -1;
    }
    euid = geteuid();
    if (euid == ROOT_UID)
    {
        return 0;
    }
    protection = 7 & ( g_filestat.st_mode >> (g_filestat.st_uid == euid ?
                        6 : (g_filestat.st_gid == getegid() ? 3 : 0)) );
    if ((mod & protection) == mod)
    {
        return 0;
    }
    errno = EACCES;
    return -1;
#else
    return access(filename.string().c_str(), mod);
#endif
}

//
// Get working directory
//
std::string trn_getwd()
{
    std::error_code ec;
    std::string     cwd = fs::current_path(ec).string();
    if (ec)
    {
        fmt::print("Cannot determine current working directory!\n");
        finalize(1);
    }
#ifdef _WIN32
    if (cwd.size() > 1 && cwd[1] == ':')
    {
        cwd[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(cwd[0])));
    }
    for (char &ch : cwd)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
    }
#endif
    return cwd;
}

std::string get_a_line(std::FILE *fp)
{
    std::string line;
    line.reserve(LINE_BUF_LEN);
    while (true)
    {
        const int nextch = std::getc(fp);
        if (nextch == EOF)
        {
            break;
        }
        line.push_back(static_cast<char>(nextch));
        if (nextch == '\0' || nextch == '\n')
        {
            break;
        }
    }
    return line;
}

bool make_dir(const fs::path &dirname, MakeDirNameType nametype)
{
    fs::path dir{dirname};
    if (nametype == MD_FILE)
    {
        dir = dir.parent_path();
    }
    std::error_code ec;
    fs::create_directories(dir, ec);
    return static_cast<bool>(ec);
}

void not_incl(std::string_view feature)
{
    fmt::print("\nNo room for feature \"{}\" on this machine.\n", feature);
}

std::string set_def(std::string_view command, std::string_view dflt)
{
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    if ((!command.empty() && command.front() == ' ')                                                      //
#ifndef STRICT_CR                                                                                         //
        || (!command.empty() && command.front() == '\n') || (!command.empty() && command.front() == '\r') //
#endif                                                                                                    //
    )
    {
        g_s_default_cmd = true;
        g_univ_default_cmd = true;
        if (dflt.size() > 1 && dflt.front() == '^' && std::isupper(dflt[1]))
        {
            push_char(Ctl(dflt[1]));
        }
        else
        {
            push_char(dflt.empty() ? '\0' : dflt.front());
        }
        return get_cmd();
    }
    return std::string{command};
}

// attempts to verify a cryptographic signature.
void verify_sig()
{
    fmt::print("\n");
    // RIPEM
    int i = do_shell(SH, file_exp("grep -s \"BEGIN PRIVACY-ENHANCED MESSAGE\" %A"));
    if (!i) // found RIPEM
    {
        i = do_shell(SH, file_exp(get_env_var("VERIFY_RIPEM", VERIFY_RIPEM)));
        fmt::print("\nReturned value: {}\n", i);
        return;
    }
    // PGP
    i = do_shell(SH, file_exp("grep -s \"BEGIN PGP\" %A"));
    if (!i) // found PGP
    {
        i = do_shell(SH, file_exp(get_env_var("VERIFY_PGP", VERIFY_PGP)));
        fmt::print("\nReturned value: {}\n", i);
        return;
    }
    fmt::print("No PGP/RIPEM signatures detected.\n");
}

double current_time()
{
    using namespace std::chrono;
    auto result{duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()};
    return static_cast<double>(result) / 1000.0;
}

std::time_t text_to_secs(std::string_view text, std::time_t defSecs)
{
    const auto is_digit = [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; };

    std::string_view s = text;
    std::time_t      secs = 0;

    if (s.empty() || !is_digit(s.front()))
    {
        if (!s.empty() && (s.front() == 'm' || s.front() == 'M')) // "missing"
        {
            return 2;
        }
        if (!s.empty() && (s.front() == 'y' || s.front() == 'Y')) // "yes"
        {
            return defSecs;
        }
        return secs; // "never"
    }
    do
    {
        std::time_t                  item = 0;
        const char                  *start = s.data();
        const char                  *end = s.data() + s.size();
        const std::from_chars_result result = std::from_chars(start, end, item);
        s.remove_prefix(static_cast<std::size_t>(result.ptr - start));
        s = skip_space(s);
        if (!s.empty() && std::isalpha(static_cast<unsigned char>(s.front())))
        {
            switch (s.front())
            {
            case 'd':
            case 'D':
                item *= 24 * 60L;
                break;

            case 'h':
            case 'H':
                item *= 60L;
                break;

            case 'm':
            case 'M':
                break;

            default:
                item = 0;
                break;
            }
            s = skip_alpha(s);
            if (!s.empty() && s.front() == ',')
            {
                s.remove_prefix(1);
            }
            s = skip_space(s);
        }
        secs += item;
    } while (!s.empty() && is_digit(s.front()));

    return secs * 60;
}

std::string secs_to_text(std::time_t secs)
{
    if (!secs || (secs & 1))
    {
        return "never";
    }
    if (secs & 2)
    {
        return "missing";
    }

    secs /= 60;
    std::string text;
    if (secs >= 24L * 60)
    {
        const int items = static_cast<int>(secs / (24 * 60));
        secs = secs % (24 * 60);
        fmt::format_to(std::back_inserter(text), "{} day{}", items, plural(items));
    }
    if (secs >= 60L)
    {
        const int items = static_cast<int>(secs / 60);
        secs = secs % 60;
        if (!text.empty())
        {
            text += ", ";
        }
        fmt::format_to(std::back_inserter(text), "{} hour{}", items, plural(items));
    }
    if (secs)
    {
        const int items = static_cast<int>(secs);
        if (!text.empty())
        {
            text += ", ";
        }
        fmt::format_to(std::back_inserter(text), "{} minute{}", items, plural(items));
    }
    return text;
}

// returns an owned string representing a unique temporary filename
std::string temp_filename()
{
    static int  tmpfile_num = 0;
    extern long g_our_pid;
    return (fs::path{g_tmp_dir} / fmt::format("trn{}.{}", tmpfile_num++, g_our_pid)).string();
}

std::string get_auth_user()
{
    return g_data_source->m_auth_user;
}

std::string get_auth_pass()
{
    return g_data_source->m_auth_pass;
}

/// @brief Parses a string from the input buffer, handling quotes, comments, and escape sequences.
///
/// This function processes a string from the input buffer, handling:
/// - Quoted strings (both single and double quotes).
/// - Comments (lines starting with '#').
/// - Escape sequences (e.g., '\n').
/// - Trimming trailing whitespace.
///
/// The parsed string is written to the output string, and the input view
/// is updated to the next position.
///
/// @param to   Output string where the parsed string will be written.
/// @param from Input cursor to read and parse the string from.
///
void parse_string(std::string &to, std::string_view &from)
{
    char inquote = 0;
    to.clear();
    std::size_t trim_boundary = 0;

    while (!from.empty() && from.front() != '\n' && std::isspace(static_cast<unsigned char>(from.front())))
    {
        from.remove_prefix(1);
    }

    while (!from.empty())
    {
        const char ch = from.front();
        if (inquote)
        {
            if (ch == inquote)
            {
                inquote = 0;
                trim_boundary = to.size();
                from.remove_prefix(1);
                continue;
            }
        }
        else if (ch == '\0' || ch == '\n')
        {
            break;
        }
        else if (ch == '\'' || ch == '"')
        {
            inquote = ch;
            from.remove_prefix(1);
            continue;
        }
        else if (ch == '#')
        {
            while (!from.empty() && from.front() != '\0' && from.front() != '\n')
            {
                from.remove_prefix(1);
            }
            break;
        }
        if (ch == '\\')
        {
            from.remove_prefix(1);
            if (from.empty())
            {
                to.push_back('\\');
                break;
            }
            if (from.front() == '\0')
            {
                to.push_back('\\');
                break;
            }
            if (from.front() == '\n')
            {
                from.remove_prefix(1);
                continue;
            }
            std::string_view  escape{from};
            const std::size_t original_size = escape.size();
            to.push_back(interp_backslash(escape));
            from.remove_prefix(original_size - escape.size());
        }
        else
        {
            to.push_back(ch);
            from.remove_prefix(1);
        }
    }
// Debug
#if 0
    if (inquote)
    {
        fmt::print("Unbalanced quotes.\n");
    }
#endif

    while (to.size() != trim_boundary && std::isspace(static_cast<unsigned char>(to.back())))
    {
        to.pop_back();
    }
}

bool parse_ini_section(const IniSection &section, const IniSchema &schema, IniSectionValues &values)
{
    values.reset();
    bool saw_value = false;
    for (const IniSetting setting : section)
    {
        const std::string value = setting.value();
        if (value.empty())
        {
            continue;
        }
        saw_value = true;
        const IniField *field = schema.find(setting.name());
        if (field == nullptr)
        {
            fmt::print("Unknown option: `{}'.\n", setting.name());
            continue;
        }
        values.set(*field, value);
    }
    return saw_value;
}

bool check_ini_cond(std::string_view cond)
{
    std::string_view cond_cursor{cond};
    std::string      condition_text = do_interp(cond_cursor, "!=<>", {});
    while (!condition_text.empty() && std::isspace(static_cast<unsigned char>(condition_text.back())))
    {
        condition_text.pop_back();
    }
    const bool negate = !cond_cursor.empty() && cond_cursor.front() == '!';
    if (negate)
    {
        cond_cursor.remove_prefix(1);
    }
    const int upordown = !cond_cursor.empty() && cond_cursor.front() == '<'
                             ? -1
                             : (!cond_cursor.empty() && cond_cursor.front() == '>' ? 1 : 0);
    if (upordown != 0)
    {
        cond_cursor.remove_prefix(1);
    }
    bool equal = !cond_cursor.empty() && cond_cursor.front() == '=';
    if (equal)
    {
        cond_cursor.remove_prefix(1);
    }
    cond_cursor = skip_space(cond_cursor);
    const auto parse_condition_number = [](std::string_view number_text)
    {
        number_text = skip_space(number_text);
        if (!number_text.empty() && number_text.front() == '+')
        {
            if (number_text.size() == 1 || !std::isdigit(static_cast<unsigned char>(number_text[1])))
            {
                return 0;
            }
            number_text.remove_prefix(1);
        }
        int value{};
        (void) std::from_chars(number_text.data(), number_text.data() + number_text.size(), value);
        return value;
    };
    if (upordown)
    {
        const int  num = parse_condition_number(cond_cursor) - parse_condition_number(condition_text);
        const bool comparison = (equal && num == 0) || (upordown * num < 0);
        if (comparison == negate)
        {
            return false;
        }
    }
    else if (equal)
    {
        CompiledRegex condcompex;
        condcompex.init_compex();
        const std::string regex_text{cond_cursor};
        const std::string_view compile_error = condcompex.compile(regex_text, true, true);
        if (!compile_error.empty())
        {
            // warning(s)
            equal = false;
        }
        else
        {
            equal = condcompex.execute(condition_text);
        }
        condcompex.free_compex();
        return equal;
    }
    else
    {
        return false;
    }
    return true;
}

// TODO: might get replaced soonish...
// Ask for a single character (improve the prompt?)
char menu_get_char()
{
    fmt::print("Enter your choice: ");
    std::fflush(stdout);
    eat_typeahead();
    const std::string command = get_cmd();
    const char        ch = command.empty() ? '\0' : command.front();
    fmt::print("{}\n", ch);
    return ch;
}

// NOTE: kfile.c uses its own editor function
// used in a few places, now centralized
int edit_file(std::string_view fname)
{
    int r = -1;

    if (fname.empty())
    {
        return r;
    }

    const std::string command = fmt::format(
        "{} {}", file_exp(get_env_var("VISUAL", get_env_var("EDITOR", DEFAULT_EDITOR))), file_exp(fname));
    term_down(3);
    reset_tty();                  // make sure tty is friendly
    r = do_shell(SH, command);          // invoke the shell
    no_echo();                   // and make terminal
    cr_mode();                   // unfriendly again
    return r;
}

// Consider a trn_pushdir, trn_popdir pair of functions
