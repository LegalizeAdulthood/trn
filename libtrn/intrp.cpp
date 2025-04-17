/* intrp.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/pipe_io.h>

#include "config/common.h"
#include "trn/intrp.h"

#include "config/user_id.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/artio.h"
#include "trn/artsrch.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/init.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/respond.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/search.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#ifdef HAS_UNAME
#include <sys/utsname.h>
struct utsname utsn;
#endif

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

std::string g_orig_dir;    /* cwd when rn invoked */
char       *g_host_name{}; /* host name to match local postings */
std::string g_head_name;
int         g_perform_count{};

#ifdef HAS_NEWS_ADMIN
const std::string g_news_admin{NEWS_ADMIN}; /* news administrator */
int g_news_uid{};
#endif

static char *skipinterp(char *pattern, const char *stoppers);
static void abort_interp();

static const char *s_regexp_specials = "^$.*[\\/?%";
static CompiledRegex      s_cond_compex;
static char        s_empty[]{""};
static std::string s_last_input;

void interp_init(char *tcbuf, int tcbuf_len)
{
    s_last_input.clear();
    init_compex(&s_cond_compex);

    /* get environmental stuff */

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
        char tildenews[2+sizeof NEWS_ADMIN];
        std::strcpy(tildenews, "~");
        std::strcat(tildenews, NEWS_ADMIN);
        (void) filexp(tildenews);
#else
        ... "Define either HAS_GETPWENT or TILDENAME to get NEWS_ADMIN"
#endif  /* TILDENAME */
#endif  /* HAS_GETPWENT */
    }

    /* if this is the news admin then load his UID into g_news_uid */

    if (!g_login_name.empty())
        g_news_uid = getuid();
#endif

    if (g_check_flag)             /* that getwd below takes ~1/3 sec. */
    {
        return;                  /* and we do not need it for -c */
    }
    trn_getwd(tcbuf, tcbuf_len); /* find working directory name */
    g_orig_dir = tcbuf;           /* and remember it */

    /* name of header file (%h) */

    g_head_name = filexp(HEADNAME);

    /* the hostname to use in local-article comparisons */
#if HOSTBITS != 0
    int i = (HOSTBITS < 2? 2 : HOSTBITS);
    static char buff[128];
    std::strcpy(buff, g_p_host_name.c_str());
    g_host_name = buff+std::strlen(buff)-1;
    while (i && g_host_name != buff)
    {
        if (*--g_host_name == '.')
        {
            i--;
        }
    }
    if (*g_host_name == '.')
    {
        g_host_name++;
    }
#else
    g_hostname = g_p_host_name.c_str();
#endif
}

void interp_final()
{
    g_head_name.clear();
    g_host_name = nullptr;
    g_orig_dir.clear();
}

/* skip interpolations */

static char *skipinterp(char *pattern, const char *stoppers)
{
#ifdef DEBUG
    if (debug & DEB_INTRP)
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
                pattern = skipinterp(pattern+1,"!=");
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
                pattern = skipinterp(pattern+1,":)");
                if (*pattern == ':')
                {
                    pattern = skipinterp(pattern + 1, ")");
                }
                break;
            }

            case '`':
            {
                pattern = skipinterp(pattern+1,"`");
                break;
            }

            case '"':
                pattern = skipinterp(pattern+1,"\"");
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
    return pattern;                     /* where we left off */
}

/* interpret interpolations */
char *do_interp(char *dest, int dest_size, char *pattern, const char *stoppers, const char *cmd)
{
    char* subj_buf = nullptr;
    char* ngs_buf = nullptr;
    char* refs_buf = nullptr;
    char* artid_buf = nullptr;
    char* reply_buf = nullptr;
    char* from_buf = nullptr;
    char* path_buf = nullptr;
    char* follow_buf = nullptr;
    char* dist_buf = nullptr;
    char* line_buf = nullptr;
    char* line_split = nullptr;
    char* orig_dest = dest;
    char scrbuf[8192];
    int metabit = 0;

    while (*pattern && (!stoppers || !std::strchr(stoppers, *pattern)))
    {
        if (*pattern == '%' && pattern[1])
        {
            char spfbuf[512];
            bool upper = false;
            bool lastcomp = false;
            bool re_quote = false;
            int tick_quote = 0;
            bool address_parse = false;
            bool comment_parse = false;
            bool proc_sprintf = false;
            char *s = nullptr;
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
                    char *h = spfbuf;
                    *h++ = '%';
                    pattern++;  /* Skip over ':' */
                    while (*pattern //
                           && (*pattern == '.' || *pattern == '-' || isdigit(*pattern)))
                    {
                        *h++ = *pattern++;
                     }
                    *h++ = 's';
                    *h++ = '\0';
                    pattern--;
                    break;
                }

                case '/':
                    s = scrbuf;
                    if (!cmd || !std::strchr("/?g",*cmd))
                    {
                        *s++ = '/';
                    }
                    std::strcpy(s,g_last_pat.c_str());
                    s += std::strlen(s);
                    if (!cmd || *cmd != 'g')
                    {
                        if (cmd && std::strchr("/?",*cmd))
                        {
                            *s++ = *cmd;
                        }
                        else
                        {
                            *s++ = '/';
                        }
                        if (g_art_do_read)
                        {
                            *s++ = 'r';
                        }
                        if (g_art_how_much != ARTSCOPE_SUBJECT)
                        {
                            *s++ = g_scope_str[g_art_how_much];
                            if (g_art_how_much == ARTSCOPE_ONE_HDR)
                            {
                                safecpy(s,g_htype[g_art_srch_hdr].name,
                                        (sizeof scrbuf) - (s-scrbuf));
                                if (!(s = std::strchr(s,':')))
                                {
                                    s = scrbuf + (sizeof scrbuf) - 1;
                                }
                                else
                                {
                                    s++;
                                }
                            }
                        }
                    }
                    *s = '\0';
                    s = scrbuf;
                    break;

                case '{':
                {
                    pattern = cpytill(scrbuf,pattern+1,'}');
                    char *m = std::strchr(scrbuf, '-');
                    if (m != nullptr)
                    {
                        *m++ = '\0';
                    }
                    else
                    {
                        m = s_empty;
                    }
                    s = get_val(scrbuf,m);
                    break;
                }

                case '<':
                    pattern = cpytill(scrbuf,pattern+1,'>');
                    s = std::strchr(scrbuf, '-');
                    if (s != nullptr)
                    {
                        *s++ = '\0';
                    }
                    else
                    {
                        s = s_empty;
                    }
                    interp(scrbuf, 8192, get_val(scrbuf,s));
                    s = scrbuf;
                    break;

                case '[':
                    pattern = cpytill(scrbuf, pattern+1, ']');
                    if (g_in_ng)
                    {
                        HeaderLineType which_line;
                        if (*scrbuf && (which_line = get_header_num(scrbuf)) != SOME_LINE)
                        {
                            safefree(line_buf);
                            line_buf = fetch_lines(g_art, which_line);
                            s = line_buf;
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
                    pattern = cpytill(scrbuf,pattern+1,'?');
                    if (!*pattern)
                    {
                        goto getout;
                    }
                    s = scrbuf;
                    char *h = spfbuf;
                    proc_sprintf = false;
                    do
                    {
                        switch (*s)
                        {
                        case '^':
                            *h++ = '\\';
                            break;

                        case '\\':
                            *h++ = '\\';
                            *h++ = '\\';
                            break;

                        case '%':
                            proc_sprintf = true;
                            break;
                        }
                        *h++ = *s;
                    } while (*s++);
                    if (proc_sprintf)
                    {
                        do_interp(scrbuf,sizeof scrbuf,spfbuf,nullptr,cmd);
                        proc_sprintf = false;
                    }
                    s = compile(&s_cond_compex, scrbuf, true, true);
                    if (s != nullptr)
                    {
                        std::printf("%s: %s\n",scrbuf,s);
                        pattern += std::strlen(pattern);
                        free_compex(&s_cond_compex);
                        goto getout;
                    }
                    matched = (execute(&s_cond_compex,dest) != nullptr);
                    if (getbracket(&s_cond_compex, 0)) /* were there brackets? */
                    {
                        g_bra_compex = &s_cond_compex;
                    }
                    if (matched == (rch == '='))
                    {
                        pattern = do_interp(dest,dest_size,pattern+1,":)",cmd);
                        if (*pattern == ':')
                        {
                            pattern = skipinterp(pattern + 1, ")");
                        }
                    }
                    else
                    {
                        pattern = skipinterp(pattern+1,":)");
                        if (*pattern == ':')
                        {
                            pattern++;
                        }
                        pattern = do_interp(dest,dest_size,pattern,")",cmd);
                    }
                    s = dest;
                    g_bra_compex = oldbra_compex;
                    free_compex(&s_cond_compex);
                    break;
                }

                case '`':
                {
                    pattern = do_interp(scrbuf,(sizeof scrbuf),pattern+1,"`",cmd);
                    std::FILE* pipefp = popen(scrbuf,"r");
                    if (pipefp != nullptr)
                    {
                        int len = std::fread(scrbuf, sizeof(char), (sizeof scrbuf) - 1, pipefp);
                        scrbuf[len] = '\0';
                        pclose(pipefp);
                    }
                    else
                    {
                        std::printf("\nCan't run %s\n",scrbuf);
                        *scrbuf = '\0';
                    }
                    for (char *t = scrbuf; *t; t++)
                    {
                        if (*t == '\n')
                        {
                            if (t[1])
                            {
                                *t = ' ';
                            }
                            else
                            {
                                *t = '\0';
                            }
                        }
                    }
                    s = scrbuf;
                    break;
                }

                case '"':
                {
                    pattern = do_interp(scrbuf,(sizeof scrbuf),pattern+1,"\"",cmd);
                    std::fputs(scrbuf,stdout);
                    resetty();
                    std::fgets(scrbuf, sizeof scrbuf, stdin);
                    noecho();
                    crmode();
                    int i = std::strlen(scrbuf);
                    if (scrbuf[i - 1] == '\n')
                    {
                        scrbuf[--i] = '\0';
                    }
                    s_last_input = scrbuf;
                    s = scrbuf;
                    break;
                }

                case '~':
                    s = g_home_dir;
                    break;

                case '.':
                    std::strcpy(scrbuf, g_dot_dir.c_str());
                    s = scrbuf;
                    break;

                case '+':
                    std::strcpy(scrbuf, g_trn_dir.c_str());
                    s = scrbuf;
                    break;

                case '$':
                    std::sprintf(scrbuf, "%ld", g_our_pid);
                    s = scrbuf;
                    break;

                case '#':
                    if (upper)
                    {
                        static int counter = 0;
                        std::sprintf(scrbuf, "%d", ++counter);
                    }
                    else
                    {
                        std::sprintf(scrbuf, "%d", g_perform_count);
                    }
                    s = scrbuf;
                    break;

                case '?':
                    s = " ";
                    line_split = dest;
                    break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    std::strcpy(scrbuf, getbracket(g_bra_compex,*pattern - '0'));
                    s = scrbuf;
                    break;

                case 'a':
                    if (g_in_ng)
                    {
                        s = scrbuf;
                        std::sprintf(s,"%ld",(long)g_art);
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'A':
                    if (g_in_ng)
                    {
                        if (g_data_source->flags & DF_REMOTE)
                        {
                            if (art_open(g_art, (ArticlePosition) 0))
                            {
                                nntp_finish_body(FB_SILENT);
                                std::sprintf(s = scrbuf,"%s/%s",g_data_source->spool_dir,
                                        nntp_art_name(g_art, false));
                            }
                            else
                            {
                                s = s_empty;
                            }
                        }
                        else
                        {
                            std::sprintf(s = scrbuf, "%s/%s/%ld", g_data_source->spool_dir, g_ngdir.c_str(), (long) g_art);
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'b':
                    std::strcpy(scrbuf, g_save_dest.c_str());
                    s = scrbuf;
                    break;

                case 'B':
                    s = scrbuf;
                    std::sprintf(s,"%ld",(long)g_save_from);
                    break;

                case 'c':
                    std::strcpy(scrbuf, g_ngdir.c_str());
                    s = scrbuf;
                    break;

                case 'C':
                    std::strcpy(scrbuf, g_ngname.c_str());
                    s = scrbuf;
                    break;

                case 'd':
                    if (!g_ngdir.empty())
                    {
                        std::sprintf(scrbuf, "%s/%s", g_data_source->spool_dir, g_ngdir.c_str());
                        s = scrbuf;
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
                        s = dist_buf;
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'e':
                {
                    static char dash[]{"-"};
                    if (g_extract_prog.empty())
                    {
                        s = dash;
                    }
                    else
                    {
                        std::strcpy(scrbuf, g_extract_prog.c_str());
                        s = scrbuf;
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
                        std::strcpy(scrbuf, g_extract_dest.c_str());
                        s = scrbuf;
                    }
                    break;

                case 'f':                       /* from line */
                    if (g_in_ng)
                    {
                        parse_header(g_art);
                        if (g_htype[REPLY_LINE].min_pos >= 0 && !comment_parse)
                        {
                                                /* was there a reply line? */
                            if (!(s=reply_buf))
                            {
                                reply_buf = fetch_lines(g_art, REPLY_LINE);
                                s = reply_buf;
                            }
                        }
                        else if (!(s = from_buf))
                        {
                            from_buf = fetch_lines(g_art, FROM_LINE);
                            s = from_buf;
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
                        if (g_htype[FOLLOW_LINE].min_pos >= 0)
                                        /* is there a Followup-To line? */
                        {
                            follow_buf = fetch_lines(g_art, FOLLOW_LINE);
                            s = follow_buf;
                        }
                        else
                        {
                            ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
                            s = ngs_buf;
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'g':                       /* general mode */
                    scrbuf[0] = static_cast<char>(g_general_mode);
                    scrbuf[1] = '\0';
                    s = scrbuf;
                    break;

                case 'h':                       /* header file name */
                    std::strcpy(scrbuf, g_head_name.c_str());
                    s = scrbuf;
                    break;

                case 'H':                       /* host name in postings */
                    std::strcpy(scrbuf, g_p_host_name.c_str());
                    s = scrbuf;
                    break;

                case 'i':
                    if (g_in_ng)
                    {
                        if (!(s=artid_buf))
                        {
                            artid_buf = fetch_lines(g_art, MSG_ID_LINE);
                            s = artid_buf;
                        }
                        if (*s && *s != '<')
                        {
                            std::sprintf(scrbuf,"<%s>",artid_buf);
                            s = scrbuf;
                        }
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'I':                       /* indent string for quoting */
                    std::sprintf(scrbuf,"'%s'",g_indent_string.c_str());
                    s = scrbuf;
                    break;

                case 'j':
                    s = scrbuf;
                    std::sprintf(scrbuf,"%d",g_just_a_sec*10);
                    break;

                case 'l':                       /* news admin login */
#ifdef HAS_NEWS_ADMIN
                    std::strcpy(scrbuf, g_news_admin.c_str());
#else
                    std::strcpy(scrbuf, "???");
#endif
                    s = scrbuf;
                    break;

                case 'L':                       /* login id */
                    std::strcpy(scrbuf, g_login_name.c_str());
                    s = scrbuf;
                    break;

                case 'm':               /* current mode */
                    s = scrbuf;
                    *s = static_cast<char>(g_mode);
                    s[1] = '\0';
                    break;

                case 'M':
                    std::sprintf(scrbuf,"%ld",(long)g_dm_count);
                    s = scrbuf;
                    break;

                case 'n':                       /* newsgroups */
                    if (g_in_ng)
                    {
                        ngs_buf = fetch_lines(g_art, NEWSGROUPS_LINE);
                        s = ngs_buf;
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;

                case 'N':                       /* full name */
                    std::strcpy(scrbuf, g_real_name.c_str());
                    s = get_val("NAME", scrbuf);
                    break;

                case 'o':                       /* organization */
#ifdef IGNOREORG
                    s = get_val("NEWSORG",s_orgname);
#else
                    s = get_val("NEWSORG",nullptr);
                    if (s == nullptr)
                    {
                        std::strcpy(scrbuf, ORGNAME);
                        s = get_val("ORGANIZATION", scrbuf);
                    }
#endif
                    s = filexp(s);
                    if (FILE_REF(s))
                    {
                        std::FILE* ofp = std::fopen(s,"r");

                        if (ofp)
                        {
                            if (std::fgets(scrbuf,sizeof scrbuf,ofp) == nullptr)
                            {
                                *scrbuf = '\0';
                            }
                            std::fclose(ofp);
                            s = scrbuf+std::strlen(scrbuf)-1;
                            if (*scrbuf && *s == '\n')
                            {
                                *s = '\0';
                            }
                            s = scrbuf;
                        }
                        else
                        {
                            s = s_empty;
                        }
                    }
                    break;

                case 'O':
                    std::strcpy(scrbuf, g_orig_dir.c_str());
                    s = scrbuf;
                    break;

                case 'p':
                    std::strcpy(scrbuf, g_priv_dir.c_str());
                    s = scrbuf;
                    break;

                case 'P':
                    s = g_data_source ? g_data_source->spool_dir : s_empty;
                    break;

                case 'q':
                    std::strcpy(scrbuf, s_last_input.c_str());
                    s = scrbuf;
                    break;

                case 'r':
                    if (g_in_ng)
                    {
                        parse_header(g_art);
                        safefree0(refs_buf);
                        if (g_htype[REFS_LINE].min_pos >= 0)
                        {
                            refs_buf = fetch_lines(g_art,REFS_LINE);
                            normalize_refs(refs_buf);
                            s = std::strrchr(refs_buf, '<');
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
                    safefree0(refs_buf);
                    int len;
                    if (g_htype[REFS_LINE].min_pos >= 0)
                    {
                        refs_buf = fetch_lines(g_art,REFS_LINE);
                        len = std::strlen(refs_buf)+1;
                        normalize_refs(refs_buf);
                        /* no more than 3 prior references PLUS the
                        ** root article allowed, including the one
                        ** concatenated below */
                        s = std::strrchr(refs_buf, '<');
                        if (s != nullptr && s > refs_buf)
                        {
                            *s = '\0';
                            char *h = std::strrchr(refs_buf,'<');
                            *s = '<';
                            if (h && h > refs_buf)
                            {
                                s = std::strchr(refs_buf+1,'<');
                                if (s < h)
                                {
                                    safecpy(s, h, len);
                                }
                            }
                        }
                    }
                    else
                    {
                        len = 0;
                    }
                    if (!artid_buf)
                    {
                        artid_buf = fetch_lines(g_art, MSG_ID_LINE);
                    }
                    int i = refs_buf? std::strlen(refs_buf) : 0;
                    int j = std::strlen(artid_buf) + (i? 1 : 0)
                      + (artid_buf[0] == '<'? 0 : 2) + 1;
                    if (len < i + j)
                    {
                        refs_buf = saferealloc(refs_buf, i + j);
                    }
                    if (i)
                    {
                        refs_buf[i++] = ' ';
                    }
                    if (artid_buf[0] == '<')
                    {
                        std::strcpy(refs_buf + i, artid_buf);
                    }
                    else if (artid_buf[0])
                    {
                        std::sprintf(refs_buf + i, "<%s>", artid_buf);
                    }
                    else
                    {
                        refs_buf[i] = '\0';
                    }
                    s = refs_buf;
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
                    char *str = subj_buf;
                    if (str == nullptr)
                    {
                        subj_buf = fetch_subj(g_art, true);
                        str = subj_buf;
                    }
                    if (*pattern == 's')
                    {
                        subject_has_re(str, &str);
                    }
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
                    if (g_htype[REPLY_LINE].min_pos >= 0)
                    {
                                        /* was there a reply line? */
                        if (!(s=reply_buf))
                        {
                            reply_buf = fetch_lines(g_art, REPLY_LINE);
                            s = reply_buf;
                        }
                    }
                    else if (!(s = from_buf))
                    {
                        from_buf = fetch_lines(g_art, FROM_LINE);
                        s = from_buf;
                    }
                    else
                    {
                        s = "noname";
                    }
                    if (*pattern == 'T')
                    {
                        if (g_htype[PATH_LINE].min_pos >= 0)
                        {
                                        /* should we substitute path? */
                            path_buf = fetch_lines(g_art, PATH_LINE);
                            s = path_buf;
                        }
                        int i = std::strlen(g_p_host_name.c_str());
                        if (!std::strncmp(g_p_host_name.c_str(),s,i) && s[i] == '!')
                        {
                            s += i + 1;
                        }
                    }
                    address_parse = true;       /* just the good part */
                    break;

                case 'u':
                    if (g_in_ng)
                    {
                        std::sprintf(scrbuf, "%ld", g_newsgroup_ptr->to_read);
                        s = scrbuf;
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
                        const bool selected = g_curr_artp != nullptr && (g_curr_artp->flags & AF_SEL) != AF_NONE;
                        std::sprintf(scrbuf, "%ld", g_selected_count - (selected && unseen ? 1 : 0));
                    }
                    else
                    {
                        std::sprintf(scrbuf, "%ld", g_newsgroup_ptr->to_read - (unseen ? 1 : 0));
                    }
                    s = scrbuf;
                    break;
                }

                case 'v':
                {
                    if (g_in_ng)
                    {
                        const bool selected = g_curr_artp && g_curr_artp->flags & AF_SEL;
                        const bool unseen = g_art <= g_last_art && !was_read(g_art);
                        std::sprintf(scrbuf, "%ld",
                                g_newsgroup_ptr->to_read - g_selected_count - (!selected && unseen ? 1 : 0));
                        s = scrbuf;
                    }
                    else
                    {
                        s = s_empty;
                    }
                    break;
                }

                case 'V':
                    std::strcpy(scrbuf, g_patchlevel.c_str());
                    s = scrbuf;
                    break;

                case 'x':                           /* news library */
                    std::strcpy(scrbuf, g_lib.c_str());
                    s = scrbuf;
                    break;

                case 'X':                           /* rn library */
                    std::strcpy(scrbuf, g_rn_lib.c_str());
                    s = scrbuf;
                    break;

                case 'y':       /* from line with *-shortening */
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    /* XXX Rewrite this! */
                    {   /* sick, but I don't want to hunt down a buf... */
                        static char tmpbuf[1024];
                        char* s2;
                        char* s3;
                        int i = 0;

                        s2 = fetch_lines(g_art,FROM_LINE);
                        std::strcpy(tmpbuf,s2);
                        std::free(s2);
                        for (s2 = tmpbuf; (*s2 && (*s2 != '@') && (*s2 != ' ')); s2++)
                        {
                        }
                        if (*s2 == '@')         /* we have normal form... */
                        {
                            for (s3 = s2 + 1; (*s3 && (*s3 != ' ')); s3++)
                            {
                                if (*s3 == '.')
                                {
                                    i++;
                                }
                            }
                        }
                        if (i>1)   /* more than one dot */
                        {
                            s3 = s2;    /* will be incremented before use */
                            while (i >= 2)
                            {
                                s3++;
                                if (*s3 == '.')
                                {
                                    i--;
                                }
                            }
                            s2++;
                            *s2 = '*';
                            s2++;
                            *s2 = '\0';
                            from_buf = (char*)safemalloc(
                                (std::strlen(tmpbuf)+std::strlen(s3)+1)*sizeof(char));
                            std::strcpy(from_buf,tmpbuf);
                            std::strcat(from_buf,s3);
                        }
                        else
                        {
                            from_buf = savestr(tmpbuf);
                        }
                        s = from_buf;
                    }
                    break;

                case 'Y':
                    std::strcpy(scrbuf, g_tmp_dir.c_str());
                    s = scrbuf;
                    break;

                case 'z':
                    if (!g_in_ng)
                    {
                        s = s_empty;
                        break;
                    }
                    std::sprintf(scrbuf, "%5s", std::to_string(std::filesystem::file_size(std::to_string(g_art))).c_str());
                    s = scrbuf;
                    break;

                case 'Z':
                    if (!g_in_ng)
                    {
                        s = s_empty;
                    }
                    else
                    {
                        std::sprintf(scrbuf,"%ld",(long)g_selected_count);
                        s = scrbuf;
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
                if (s == scrbuf)
                {
                    static char scratch[sizeof(scrbuf)];
                    std::strcpy(scratch, scrbuf);
                    s = scratch;
                }
                std::sprintf(scrbuf, spfbuf, s);
                s = scrbuf;
            }
            if (*pattern)
            {
                pattern++;
            }
            if (upper || lastcomp)
            {
                if (s != scrbuf)
                {
                    safecpy(scrbuf,s,sizeof scrbuf);
                    s = scrbuf;
                }
                char* t;
                if (upper || !(t = std::strrchr(s,'/')))
                {
                    t = s;
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
            /* Do we have room left? */
            int i = std::strlen(s);
            if (dest_size <= i)
            {
                abort_interp();
            }
            dest_size -= i;      /* adjust the size now. */

            /* A maze of twisty little conditions, all alike... */
            if (address_parse || comment_parse)
            {
                if (s != scrbuf)
                {
                    safecpy(scrbuf,s,sizeof scrbuf);
                    s = scrbuf;
                }
                decode_header(s, s, std::strlen(s));
                if (address_parse)
                {
                    char *h = std::strchr(s, '<');
                    if (h != nullptr)   /* grab the good part */
                    {
                        s = h+1;
                        if ((h=std::strchr(s,'>')) != nullptr)
                        {
                            *h = '\0';
                        }
                    }
                    else if ((h = std::strchr(s, '(')) != nullptr)
                    {
                        while (h-- != s && *h == ' ')
                        {
                        }
                        h[1] = '\0';            /* or strip the comment */
                    }
                }
                else
                {
                    if (!(s = extract_name(s)))
                    {
                        s = s_empty;
                    }
                }
            }
            if (metabit)
            {
                /* set meta bit while copying. */
                i = metabit;            /* maybe get into register */
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
                /* put a backslash before regexp specials while copying. */
                if (s == dest)
                {
                    /* copy out so we can copy in. */
                    safecpy(scrbuf, s, sizeof scrbuf);
                    s = scrbuf;
                    if (i > sizeof scrbuf)      /* we truncated, ack! */
                    {
                        abort_interp();
                    }
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
                /* straight copy. */
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
                ++pattern;              /* skip backslash */
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
    safefree(subj_buf);         /* return any checked out storage */
    safefree(ngs_buf);
    safefree(refs_buf);
    safefree(artid_buf);
    safefree(reply_buf);
    safefree(from_buf);
    safefree(path_buf);
    safefree(follow_buf);
    safefree(dist_buf);
    safefree(line_buf);

    return pattern; /* where we left off */
}

char *interp_backslash(char *dest, char *pattern)
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
        *dest = (char)(i & 0377);
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
                static char hex_digits[17]{"0123456789ABCDEF"};
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
        *dest = (char)i;
        break;
    }
    return pattern;
}

/* helper functions */

char *interp(char *dest, int dest_size, char *pattern)
{
    return do_interp(dest,dest_size,pattern,nullptr,nullptr);
}

char *interp_search(char *dest, int dest_size, char *pattern, const char *cmd)
{
    return do_interp(dest,dest_size,pattern,nullptr,cmd);
}

/* normalize a references line in place */

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
