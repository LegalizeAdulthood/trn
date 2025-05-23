/* util.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/fdio.h>
#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/util.h"

#include "nntp/nntpclient.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/intrp.h"
#include "trn/search.h"
#include "trn/smisc.h" // g_s_default_cmd
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/univ.h"
#include "util/env.h"
#include "util/util2.h"

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
#include <string>
#include <system_error>

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
static char s_no_memory[] = "trn: out of memory!\n";
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
    putenv("NEWSACTIVE=");
    putenv("NEWSDESCRIPTIONS=");
    putenv("NNTPSERVER=");
    putenv("QUOTECHARS=");
    putenv("NNTP_FORCE_AUTH=");
    putenv("TRN_VERSION=");
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
    if (g_data_source && (g_data_source->flags & DF_REMOTE))
    {
        re_export(s_nntp_server_export,g_data_source->news_id,512);
        if (g_data_source->nntp_link.flags & NNTP_FORCE_AUTH_NEEDED)
        {
            re_export(s_nntp_force_export,"yes",3);
        }
        else
        {
            un_export(s_nntp_force_export);
        }
        if (g_data_source->auth_user)
        {
            int fd = open(g_nntp_auth_file.c_str(), O_WRONLY|O_CREAT, 0600);
            if (fd >= 0)
            {
                write(fd, g_data_source->auth_user, std::strlen(g_data_source->auth_user));
                write(fd, "\n", 1);
                if (g_data_source->auth_pass)
                {
                    write(fd, g_data_source->auth_pass, std::strlen(g_data_source->auth_pass));
                    write(fd, "\n", 1);
                }
                close(fd);
            }
        }
        if (g_nntp_link.port_number)
        {
            int len = std::strlen(s_nntp_server_export);
            std::sprintf(g_buf,";%d",g_nntp_link.port_number);
            if (len + (int)std::strlen(g_buf) < 511)
            {
                std::strcpy(s_nntp_server_export+len, g_buf);
            }
        }
        if (g_data_source->act_sf.fp)
        {
            re_export(s_newsa_ctive_export, g_data_source->extra_name, 512);
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
            re_export(s_newsa_ctive_export, g_data_source->news_id, 512);
        }
        else
        {
            un_export(s_newsa_ctive_export);
        }
    }
    if (g_data_source)
    {
        re_export(s_group_desc_export, g_data_source->group_desc, 512);
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
    if (g_data_source && g_data_source->auth_user)
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

void not_incl(const char *feature)
{
    std::printf("\nNo room for feature \"%s\" on this machine.\n",feature);
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
void safe_link(char *old_name, char *new_name)
{
#if 0
    extern int sys_nerr;
    extern char* sys_errlist[];
#endif

    if (link(old_name, new_name))
    {
        std::printf("Can't link backup (%s) to .newsrc (%s)\n", old_name, new_name);
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
    int i = do_shell(SH, file_exp("grep -s \"BEGIN PRIVACY-ENHANCED MESSAGE\" %A"));
    if (!i)     // found RIPEM
    {
        i = do_shell(SH,file_exp(get_val_const("VERIFY_RIPEM",VERIFY_RIPEM)));
        std::printf("\nReturned value: %d\n",i);
        return;
    }
    // PGP
    i = do_shell(SH,file_exp("grep -s \"BEGIN PGP\" %A"));
    if (!i)     // found PGP
    {
        i = do_shell(SH,file_exp(get_val_const("VERIFY_PGP",VERIFY_PGP)));
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

char *secs_to_text(std::time_t secs)
{
    char* s = g_buf;
    int items;

    if (!secs || (secs & 1))
    {
        return "never";
    }
    if (secs & 2)
    {
        return "missing";
    }

    secs /= 60;
    if (secs >= 24L * 60)
    {
        items = (int)(secs / (24*60));
        secs = secs % (24*60);
        std::sprintf(s, "%d day%s, ", items, plural(items));
        s += std::strlen(s);
    }
    if (secs >= 60L)
    {
        items = (int)(secs / 60);
        secs = secs % 60;
        std::sprintf(s, "%d hour%s, ", items, plural(items));
        s += std::strlen(s);
    }
    if (secs)
    {
        std::sprintf(s, "%d minute%s, ", (int)secs, plural(items));
        s += std::strlen(s);
    }
    s[-2] = '\0';
    return g_buf;
}

// returns a saved string representing a unique temporary filename
char *temp_filename()
{
    static int tmpfile_num = 0;
    char tmpbuf[CMD_BUF_LEN];
    extern long g_our_pid;
    std::sprintf(tmpbuf,"%s/trn%d.%ld",g_tmp_dir.c_str(),tmpfile_num++,g_our_pid);
    return save_str(tmpbuf);
}

char *get_auth_user()
{
    return g_data_source->auth_user;
}

char *get_auth_pass()
{
    return g_data_source->auth_pass;
}

char **prep_ini_words(IniWords words[])
{
    char* cp = (char*)ini_values(words);
    if (!cp)
    {
        int i;
        for (i = 1; words[i].item != nullptr; i++)
        {
            if (*words[i].item == '*')
            {
                words[i].checksum = -1;
                continue;
            }
            int checksum = 0;
            const char *item;
            for (item = words[i].item; *item; item++)
            {
                checksum += (std::isupper(*item)? std::tolower(*item) : *item);
            }
            words[i].checksum = (checksum << 8) + (item - words[i].item);
        }
        words[0].checksum = i;
        cp = safe_malloc(i * sizeof(char *));
        words[0].help_str = cp;
    }
    std::memset(cp,0,(words)[0].checksum * sizeof (char*));
    return (char**)cp;
}

void unprep_ini_words(IniWords words[])
{
    std::free(ini_values(words));
    words[0].checksum = 0;
    words[0].help_str = nullptr;
}

void prep_ini_data(char *cp, const char *filename)
{
    char* t = cp;

#ifdef DEBUG
    if (g_debug & DEB_RCFILES)
    {
        std::printf("Read %d bytes from %s\n", static_cast<int>(std::strlen(cp)),filename);
    }
#endif

    while (*cp)
    {
        cp = skip_space(cp);

        if (*cp == '[')
        {
            char* s = t;
            do
            {
                *t++ = *cp++;
            } while (*cp && *cp != ']' && *cp != '\n');
            if (*cp == ']' && t != s)
            {
                *t++ = '\0';
                cp++;
                if (parse_string(&t, &cp))
                {
                    cp++;
                }

                while (*cp)
                {
                    cp = skip_space(cp);
                    if (*cp == '[')
                    {
                        break;
                    }
                    if (*cp == '#')
                    {
                        s = cp;
                    }
                    else
                    {
                        s = t;
                        while (*cp && *cp != '\n')
                        {
                            if (*cp == '=')
                            {
                                break;
                            }
                            if (std::isspace(*cp))
                            {
                                if (s == t || t[-1] != ' ')
                                {
                                    *t++ = ' ';
                                }
                                cp++;
                            }
                            else
                            {
                                *t++ = *cp++;
                            }
                        }
                        if (*cp == '=' && t != s)
                        {
                            while (t != s && std::isspace(t[-1]))
                            {
                                t--;
                            }
                            *t++ = '\0';
                            cp++;
                            if (parse_string(&t, &cp))
                            {
                                s = nullptr;
                            }
                            else
                            {
                                s = cp;
                            }
                        }
                        else
                        {
                            s = cp;
                        }
                    }
                    cp++;
                    if (s)
                    {
                        for (cp = s; *cp && *cp++ != '\n'; )
                        {
                        }
                    }
                }
            }
            else
            {
                *t = '\0';
                std::printf("Invalid section in %s: %s\n", filename, s);
                t = s;
                cp = skip_ne(cp, '\n');
            }
        }
        else
        {
            cp = skip_ne(cp, '\n');
        }
    }
    *t = '\0';
}

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

    return inquote;     // return true if the string ended with a newline
}

char *next_ini_section(char *cp, char **section, char **cond)
{
    while (*cp != '[')
    {
        if (!*cp)
        {
            return nullptr;
        }
        cp += std::strlen(cp) + 1;
        cp += std::strlen(cp) + 1;
    }
    *section = cp+1;
    cp += std::strlen(cp) + 1;
    *cond = cp;
    cp += std::strlen(cp) + 1;
#ifdef DEBUG
    if (g_debug & DEB_RCFILES)
    {
        std::printf("Section [%s] (condition: %s)\n",*section,
               **cond? *cond : "<none>");
    }
#endif
    return cp;
}

char *parse_ini_section(char *cp, IniWords words[])
{
    if (!*cp)
    {
        return nullptr;
    }

    char* s;
    char** values = prep_ini_words(words);

    while (*cp && *cp != '[')
    {
        int checksum = 0;
        for (s = cp; *s; s++)
        {
            if (std::isupper(*s))
            {
                *s = std::tolower(*s);
            }
            checksum += *s;
        }
        checksum = (checksum << 8) + (s++ - cp);
        if (*s)
        {
            int i;
            for (i = 1; words[i].checksum; i++)
            {
                if (words[i].checksum == checksum //
                    && string_case_equal(cp, words[i].item))
                {
                    values[i] = s;
                    break;
                }
            }
            if (!words[i].checksum)
            {
                std::printf("Unknown option: `%s'.\n",cp);
            }
            cp = s + std::strlen(s) + 1;
        }
        else
        {
            cp = s + 1;
        }
    }

#ifdef DEBUG
    if (g_debug & DEB_RCFILES)
    {
        std::printf("Ini_words: %s\n", words[0].item);
        for (int i = 1; words[i].checksum; i++)
        {
            if (values[i])
            {
                std::printf("%s=%s\n",words[i].item,values[i]);
            }
        }
    }
#endif

    return cp;
}

bool check_ini_cond(char *cond)
{
    cond = do_interp(g_buf,sizeof g_buf,cond,"!=<>",nullptr);
    char *s = g_buf + std::strlen(g_buf);
    while (s != g_buf && std::isspace(s[-1]))
    {
        s--;
    }
    *s = '\0';
    const int negate = *cond == '!' ? 1 : 0;
    if (negate != 0)
    {
        cond++;
    }
    const int upordown = *cond == '<' ? -1 : (*cond == '>' ? 1 : 0);
    if (upordown != 0)
    {
        cond++;
    }
    bool equal = *cond == '=';
    if (equal)
    {
        cond++;
    }
    cond = skip_space(cond);
    if (upordown)
    {
        const int num = std::atoi(cond) - std::atoi(g_buf);
        if (!((equal && !num) || (upordown * num < 0)) ^ negate)
        {
            return false;
        }
    }
    else if (equal)
    {
        CompiledRegex condcompex;
        init_compex(&condcompex);
        s = compile(&condcompex,cond,true,true);
        if (s != nullptr)
        {
            // warning(s)
            equal = false;
        }
        else
        {
            equal = execute(&condcompex,g_buf) != nullptr;
        }
        free_compex(&condcompex);
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

    // XXX paranoia check on length
    std::sprintf(g_cmd_buf,"%s ",
            file_exp(get_val_const("VISUAL",get_val_const("EDITOR",DEFAULT_EDITOR))));
    std::strcat(g_cmd_buf, file_exp(fname));
    term_down(3);
    reset_tty();                  // make sure tty is friendly
    r = do_shell(SH,g_cmd_buf);  // invoke the shell
    no_echo();                   // and make terminal
    cr_mode();                   // unfriendly again
    return r;
}

// Consider a trn_pushdir, trn_popdir pair of functions
