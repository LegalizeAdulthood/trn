/* util.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/util.h>

#include <config/common.h>
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
// the strlen and the buffer length of "some_buf" after a call to:
//     some_buf = get_a_line(bufptr,bufsize,realloc,fp);
int        g_len_last_line_got{};
MemorySize g_buf_len_last_line_got{};

#ifndef USE_DEBUGGING_MALLOC
static constexpr char s_no_memory[] = "trn: out of memory!\n";
#endif

static char  s_null_export[] = "_=X"; // Just in case doshell precedes util_init
static char *s_newsa_ctive_export = s_null_export + 2;
static char *s_group_desc_export = s_null_export + 2;
static char *s_quote_chars_export = s_null_export + 2;
static char *s_nntp_server_export = s_null_export + 2;
static char *s_nntp_force_export = s_null_export + 2;

void util_init()
{
    extern std::string g_patch_level;

    char *cp = g_buf;
    for (int i = 0; i < 512; i++)
    {
        *cp++ = 'X';
    }
    *cp = '\0';
    s_newsa_ctive_export = export_var("NEWSACTIVE", g_buf);
    s_group_desc_export = export_var("NEWSDESCRIPTIONS", g_buf);
    s_nntp_server_export = export_var("NNTPSERVER", g_buf);
    g_buf[64] = '\0';
    s_quote_chars_export = export_var("QUOTECHARS",g_buf);
    g_buf[3] = '\0';
    s_nntp_force_export = export_var("NNTP_FORCE_AUTH", g_buf);
    export_var("TRN_VERSION", g_patch_level.c_str());
}

void util_final()
{
    const char *names[] = {
        "NEWSACTIVE", "NEWSDESCRIPTIONS", "NNTPSERVER", "QUOTECHARS", "NNTP_FORCE_AUTH", "TRN_VERSION",
    };

    for (const char *name : names)
    {
        export_var(name, "");
    }
}

// fork and exec a shell command

int do_shell(const char *shell, const char *cmd)
{
#ifndef MSDOS
    WaitStatus status;
    pid_t pid, w;
#endif
    int ret = 0;

    xmouse_off();

#ifdef SIGTSTP
    sigset(SIGTSTP,SIG_DFL);
    sigset(SIGTTOU,SIG_DFL);
    sigset(SIGTTIN,SIG_DFL);
#endif
    if (g_data_source && (g_data_source->m_flags & DF_REMOTE))
    {
        re_export(s_nntp_server_export, g_data_source->m_news_id.c_str(), 512);
        if (g_data_source->m_nntp_link.flags & NNTP_FORCE_AUTH_NEEDED)
        {
            re_export(s_nntp_force_export, "yes", 3);
        }
        else
        {
            un_export(s_nntp_force_export);
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
            int len = std::strlen(s_nntp_server_export);
            std::sprintf(g_buf, ";%d", g_nntp_link.port_number);
            if (len + (int) std::strlen(g_buf) < 511)
            {
                std::strcpy(s_nntp_server_export + len, g_buf);
            }
        }
        if (g_data_source->m_act_sf.m_fp)
        {
            re_export(s_newsa_ctive_export, g_data_source->m_extra_name.c_str(), 512);
        }
        else
        {
            re_export(s_newsa_ctive_export, "none", 512);
        }
    }
    else
    {
        un_export(s_nntp_server_export);
        un_export(s_nntp_force_export);
        if (g_data_source)
        {
            re_export(s_newsa_ctive_export, g_data_source->m_news_id.c_str(), 512);
        }
        else
        {
            un_export(s_newsa_ctive_export);
        }
    }
    if (g_data_source)
    {
        re_export(s_group_desc_export,
                  g_data_source->m_group_desc.empty() ? nullptr : g_data_source->m_group_desc.c_str(), 512);
    }
    else
    {
        un_export(s_group_desc_export);
    }
    interp(g_buf,64-1+2,"%I");
    g_buf[std::strlen(g_buf)-1] = '\0';
    re_export(s_quote_chars_export, g_buf+1, 64);
    if (shell == nullptr)
    {
        shell = get_val_const("SHELL", nullptr);
    }
    if (shell == nullptr)
    {
        shell = PREF_SHELL;
    }
    termlib_reset();
#ifdef MSDOS
    intptr_t status = spawnl(P_WAIT, shell, shell, "/c", cmd, nullptr);
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

        if (*cmd)
        {
            execl(shell, shell, "-c", cmd, nullptr);
        }
        else
        {
            execl(shell, shell, nullptr, nullptr, nullptr);
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
        remove(g_nntp_auth_file.c_str());
    }
    return ret;
}

// paranoid version of malloc

#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size)
{
    char *ptr = (char*)std::malloc(size ? size : (MemorySize)1);
    if (!ptr)
    {
        std::fputs(s_no_memory,stdout);
        sig_catcher(0);
    }
    return ptr;
}
#endif

// paranoid version of realloc.  If where is nullptr, call malloc

#ifndef USE_DEBUGGING_MALLOC
char *safe_realloc(char *where, MemorySize size)
{
    char* ptr;

    if (!where)
    {
        ptr = (char*) std::malloc(size ? size : (MemorySize)1);
    }
    else
    {
        ptr = (char*) std::realloc(where, size ? size : (MemorySize)1);
    }
    if (!ptr)
    {
        std::fputs(s_no_memory,stdout);
        sig_catcher(0);
    }
    return ptr;
}
#endif // !USE_DEBUGGING_MALLOC

// safe version of string concatenate, with \n deletion and space padding

char *safe_cat(char *to, const char *from, int len)
{
    char* dest = to;

    len--;                              // leave room for null
    if (*dest)
    {
        while (len && *dest++)
        {
            len--;
        }
        if (len)
        {
            len--;
            *(dest-1) = ' ';
        }
    }
    if (from)
    {
        while (len && (*dest++ = *from++))
        {
            len--;
        }
    }
    if (len)
    {
        dest--;
    }
    if (*(dest-1) == '\n')
    {
        dest--;
    }
    *dest = '\0';
    return to;
}

// effective access

#ifdef SETUIDGID
int
eaccess(filename, mod)
char* filename;
int mod;
{
    int protection, euid;

    mod &= 7;                           // remove extraneous garbage
    if (stat(filename, &g_filestat) < 0)
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
}
#endif

//
// Get working directory
//
char *trn_getwd(char *buf, int buflen)
{
    std::error_code       ec;
    std::filesystem::path cwd{std::filesystem::current_path(ec)};
    if (ec)
    {
        std::printf("Cannot determine current working directory!\n");
        finalize(1);
    }
    std::strncpy(buf, cwd.string().c_str(), buflen);
#ifdef _WIN32
    if (buf[1] == ':')
    {
        buf[0] = std::toupper(buf[0]);
    }
    for (int i = 0; i < buflen; ++i)
    {
        if (buf[i] == '\\')
        {
            buf[i] = '/';
        }
    }
#endif
    return buf;
}

// just like fgets but will make bigger buffer as necessary

char *get_a_line(char *buffer, int buffer_length, bool realloc_ok, std::FILE *fp)
{
    int bufix = 0;
    int nextch;

    do
    {
        if (bufix >= buffer_length)
        {
            buffer_length *= 2;
            if (realloc_ok)             // just grow in place, if possible
            {
                buffer = safe_realloc(buffer,(MemorySize)buffer_length+1);
            }
            else
            {
                char* tmp = safe_malloc((MemorySize)buffer_length+1);
                std::strncpy(tmp,buffer,buffer_length/2);
                buffer = tmp;
                realloc_ok = true;
            }
        }
        nextch = std::getc(fp);
        if ((nextch) == EOF)
        {
            if (!bufix)
            {
                return nullptr;
            }
            break;
        }
        buffer[bufix++] = (char)nextch;
    } while (nextch && nextch != '\n');
    buffer[bufix] = '\0';
    g_len_last_line_got = bufix;
    g_buf_len_last_line_got = buffer_length;
    return buffer;
}

bool make_dir(const char *dirname, MakeDirNameType nametype)
{
    std::filesystem::path dir{dirname};
    if (nametype == MD_FILE)
    {
        dir = dir.parent_path();
    }
    std::error_code ec;
    create_directories(dir, ec);
    return static_cast<bool>(ec);
}

void not_incl(std::string_view feature)
{
    std::printf("\nNo room for feature \"%.*s\" on this machine.\n", static_cast<int>(feature.size()),
                feature.empty() ? "" : feature.data());
}

// grow a static string to at least a certain length

void grow_str(char **strptr, int *curlen, int newlen)
{
    if (newlen > *curlen)               // need more room?
    {
        if (*curlen)
        {
            *strptr = safe_realloc(*strptr,(MemorySize)newlen);
        }
        else
        {
            *strptr = safe_malloc((MemorySize)newlen);
        }
        *curlen = newlen;
    }
}

void set_def(char *buffer, const char *dflt)
{
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    if (*buffer == ' '                        //
#ifndef STRICT_CR                              //
        || *buffer == '\n' || *buffer == '\r' //
#endif                                        //
    )
    {
        g_s_default_cmd = true;
        g_univ_default_cmd = true;
        if (*dflt == '^' && std::isupper(dflt[1]))
        {
            push_char(Ctl(dflt[1]));
        }
        else
        {
            push_char(*dflt);
        }
        get_cmd(buffer);
    }
}

#ifndef NO_FILELINKS
void safe_link(const char *old_name, const char *new_name)
{
    if (link(old_name, new_name))
    {
        std::printf("Can't link backup (%s) to .newsrc (%s)\n", old_name, new_name);
// Debug
#if 0
        if (errno>0 && errno<sys_nerr)
        {
            std::printf("%s\n", sys_errlist[errno]);
        }
#endif
        finalize(1);
    }
}
#endif

// attempts to verify a cryptographic signature.
void verify_sig()
{
    std::printf("\n");
    // RIPEM
    int i = do_shell(SH, file_exp("grep -s \"BEGIN PRIVACY-ENHANCED MESSAGE\" %A").c_str());
    if (!i)     // found RIPEM
    {
        i = do_shell(SH, file_exp(get_val_const("VERIFY_RIPEM", VERIFY_RIPEM)).c_str());
        std::printf("\nReturned value: %d\n",i);
        return;
    }
    // PGP
    i = do_shell(SH, file_exp("grep -s \"BEGIN PGP\" %A").c_str());
    if (!i)     // found PGP
    {
        i = do_shell(SH, file_exp(get_val_const("VERIFY_PGP", VERIFY_PGP)).c_str());
        std::printf("\nReturned value: %d\n",i);
        return;
    }
    std::printf("No PGP/RIPEM signatures detected.\n");
}

double current_time()
{
    using namespace std::chrono;
    auto result{duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()};
    return static_cast<double>(result) / 1000.0;
}

std::time_t text_to_secs(const char *s, std::time_t defSecs)
{
    std::time_t secs = 0;

    if (!std::isdigit(*s))
    {
        if (*s == 'm' || *s == 'M')     // "missing"
        {
            return 2;
        }
        if (*s == 'y' || *s == 'Y')     // "yes"
        {
            return defSecs;
        }
        return secs;                    // "never"
    }
    do
    {
        std::time_t item = std::atol(s);
        s = skip_digits(s);
        s = skip_space(s);
        if (std::isalpha(*s))
        {
            switch (*s)
            {
            case 'd': case 'D':
                item *= 24 * 60L;
                break;

            case 'h': case 'H':
                item *= 60L;
                break;

            case 'm': case 'M':
                break;

            default:
                item = 0;
                break;
            }
            s = skip_alpha(s);
            if (*s == ',')
            {
                s++;
            }
            s = skip_space(s);
        }
        secs += item;
    } while (std::isdigit(*s));

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
/// The parsed string is written to the output buffer, and the input pointer is updated to the next position.
///
/// @param to   Pointer to the output buffer where the parsed string will be written.
/// @param from Pointer to the input buffer to read and parse the string from.
/// @return True if the string ended with a newline, false otherwise.
///
bool parse_string(char **to, char **from)
{
    char inquote = 0;
    char* t = *to;
    char* f = *from;

    while (std::isspace(*f) && *f != '\n')
    {
        f++;
    }

    char* s;
    for (s = t; *f; f++)
    {
        if (inquote)
        {
            if (*f == inquote)
            {
                inquote = 0;
                s = t;
                continue;
            }
        }
        else if (*f == '\n')
        {
            break;
        }
        else if (*f == '\'' || *f == '"')
        {
            inquote = *f;
            continue;
        }
        else if (*f == '#')
        {
            f = skip_ne(f, '\n');
            break;
        }
        if (*f == '\\')
        {
            if (*++f == '\n')
            {
                continue;
            }
            f = interp_backslash(t, f);
            t++;
        }
        else
        {
            *t++ = *f;
        }
    }
// Debug
#if 0
    if (inquote)
    {
        std::printf("Unbalanced quotes.\n");
    }
#endif
    inquote = (*f != '\0');

    while (t != s && std::isspace(t[-1]))
    {
        t--;
    }
    *t++ = '\0';

    *to = t;
    *from = f;

    return inquote; // return true if the string ended with a newline
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
    const std::string condition{cond};
    const char       *cond_cursor = do_interp(g_buf, sizeof g_buf, condition.c_str(), "!=<>", nullptr);
    char             *s = g_buf + std::strlen(g_buf);
    while (s != g_buf && std::isspace(s[-1]))
    {
        s--;
    }
    *s = '\0';
    const int negate = *cond_cursor == '!' ? 1 : 0;
    if (negate != 0)
    {
        cond_cursor++;
    }
    const int upordown = *cond_cursor == '<' ? -1 : (*cond_cursor == '>' ? 1 : 0);
    if (upordown != 0)
    {
        cond_cursor++;
    }
    bool equal = *cond_cursor == '=';
    if (equal)
    {
        cond_cursor++;
    }
    cond_cursor = skip_space(cond_cursor);
    if (upordown)
    {
        const int num = std::atoi(cond_cursor) - std::atoi(g_buf);
        if (!((equal && !num) || (upordown * num < 0)) ^ negate)
        {
            return false;
        }
    }
    else if (equal)
    {
        CompiledRegex condcompex;
        condcompex.init_compex();
        const char *compile_error = condcompex.compile(cond_cursor, true, true);
        if (compile_error != nullptr)
        {
            // warning(s)
            equal = false;
        }
        else
        {
            equal = condcompex.execute(g_buf) != nullptr;
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
    std::printf("Enter your choice: ");
    std::fflush(stdout);
    eat_typeahead();
    get_cmd(g_buf);
    std::printf("%c\n",*g_buf);
    return(*g_buf);
}

// NOTE: kfile.c uses its own editor function
// used in a few places, now centralized
int edit_file(const char *fname)
{
    int r = -1;

    if (!fname || !*fname)
    {
        return r;
    }

    const std::string command = fmt::format(
        "{} {}", file_exp(get_val_const("VISUAL", get_val_const("EDITOR", DEFAULT_EDITOR))), file_exp(fname));
    safe_copy(g_cmd_buf, command.c_str(), sizeof g_cmd_buf);
    term_down(3);
    reset_tty();                  // make sure tty is friendly
    r = do_shell(SH, command.c_str());  // invoke the shell
    no_echo();                   // and make terminal
    cr_mode();                   // unfriendly again
    return r;
}

// Consider a trn_pushdir, trn_popdir pair of functions
