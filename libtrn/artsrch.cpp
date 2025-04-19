/* artsrch.c
 */
// This software is copyrighted as detailed in the LICENSE file.


#include "config/common.h"
#include "trn/artsrch.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/artio.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/ngstuff.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/search.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

static CompiledRegex s_sub_compex{}; // last compiled subject search
static CompiledRegex s_art_compex{}; // last compiled normal search

static bool wanted(CompiledRegex *compex, ArticleNum art_num, ArtScope scope);

std::string    g_last_pat;                  // last search pattern
CompiledRegex *g_bra_compex{&s_art_compex}; // current compex with brackets
const char    *g_scope_str{"sfHhbBa"};      //
ArtScope       g_art_how_much{};            // search scope
HeaderLineType g_art_srch_hdr{};            // specific header number to search
bool           g_art_do_read{};             // search read articles?
bool           g_kill_thru_kludge{true};    // -k

void art_search_init()
{
    init_compex(&s_sub_compex);
    init_compex(&s_art_compex);
}

// search for an article containing some pattern

// if patbuf != g_buf, get_cmd must be set to false!!!
ArtSearchResult art_search(char *pat_buf, int pat_buf_siz, bool get_cmd)
{
    char* pattern;                      // unparsed pattern
    char cmd_chr = *pat_buf;              // what kind of search?
    bool backward = cmd_chr == '?' || cmd_chr == Ctl('p');
                                        // direction of search
    CompiledRegex* compex;                     // which compiled expression
    char* cmd_lst = nullptr;             // list of commands to do
    ArtSearchResult ret = SRCH_NOT_FOUND; // assume no commands
    int salt_away = 0;                   // store in KILL file?
    ArtScope how_much;                  // search scope: subj/from/Hdr/head/art
    HeaderLineType search_header;           // header to search if Hdr scope
    bool top_start = false;
    bool do_read;                        // search read articles?
    bool fold_case = true;               // fold upper and lower case?
    int ignore_thru = 0;                 // should we ignore the thru line?
    bool output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    ArticleNum search_first;

    g_int_count = 0;
    if (cmd_chr == '/' || cmd_chr == '?')         // normal search?
    {
        if (get_cmd && g_buf == pat_buf)
        {
            if (!finish_command(false)) // get rest of command
            {
                return SRCH_ABORT;
            }
        }
        compex = &s_art_compex;
        if (pat_buf[1])
        {
            how_much = ARTSCOPE_SUBJECT;
            search_header = SOME_LINE;
            do_read = false;
        }
        else
        {
            how_much = g_art_how_much;
            search_header = g_art_srch_hdr;
            do_read = g_art_do_read;
        }
        char *s = copy_till(g_buf,pat_buf+1,cmd_chr);// ok to cpy g_buf+1 to g_buf
        pattern = g_buf;
        if (*pattern)
        {
            g_last_pat = pattern;
        }
        if (*s)                         // modifiers or commands?
        {
            while (*++s)
            {
                switch (*s)
                {
                case 'f':               // scan the From line
                    how_much = ARTSCOPE_FROM;
                    break;

                case 'H':               // scan a specific header
                    how_much = ARTSCOPE_ONE_HDR;
                    s = copy_till(g_msg, s+1, ':');
                    search_header = get_header_num(g_msg);
                    goto loop_break;

                case 'h':               // scan header
                    how_much = ARTSCOPE_HEAD;
                    break;

                case 'b':               // scan body sans signature
                    how_much = ARTSCOPE_BODY_NO_SIG;
                    break;

                case 'B':               // scan body
                    how_much = ARTSCOPE_BODY;
                    break;

                case 'a':               // scan article
                    how_much = ARTSCOPE_ARTICLE;
                    break;

                case 't':               // start from the top
                    top_start = true;
                    break;

                case 'r':               // scan read articles
                    do_read = true;
                    break;

                case 'K':               // put into KILL file
                    salt_away = 1;
                    break;

                case 'c':               // make search case sensitive
                    fold_case = false;
                    break;

                case 'I':               // ignore the kill file thru line
                    ignore_thru = 1;
                    break;

                case 'N':               // override ignore if -k was used
                    ignore_thru = -1;
                    break;

                default:
                    goto loop_break;
                }
            }
          loop_break:;
        }
        while (std::isspace(*s) || *s == ':')
        {
            s++;
        }
        if (*s)
        {
            if (*s == 'm')
            {
                do_read = true;
            }
            if (*s == 'k')              // grandfather clause
            {
                *s = 'j';
            }
            cmd_lst = save_str(s);
            ret = SRCH_DONE;
        }
        g_art_how_much = how_much;
        g_art_srch_hdr = search_header;
        g_art_do_read = do_read;
        if (g_search_ahead)
        {
            g_search_ahead = ArticleNum{-1};
        }
    }
    else
    {
        int salt_mode = pat_buf[2] == 'g'? 2 : 1;
        const char *finding_str = pat_buf[1] == 'f' ? "author" : "subject";

        how_much = pat_buf[1] == 'f'? ARTSCOPE_FROM : ARTSCOPE_SUBJECT;
        search_header = SOME_LINE;
        do_read = (cmd_chr == Ctl('p'));
        if (cmd_chr == Ctl('n'))
        {
            ret = SRCH_SUBJ_DONE;
        }
        compex = &s_sub_compex;
        pattern = pat_buf+1;
        char *h;
        if (how_much == ARTSCOPE_SUBJECT)
        {
            std::strcpy(pattern,": *");
            h = pattern + std::strlen(pattern);
            interp(h,pat_buf_siz - (h-pat_buf),"%\\s");  // fetch current subject
        }
        else
        {
            h = pattern;
            // TODO: if using thread files, make this "%\\)f"
            interp(pattern, pat_buf_siz - 1, "%\\>f");
        }
        if (cmd_chr == 'k' || cmd_chr == 'K' || cmd_chr == ',' //
            || cmd_chr == '+' || cmd_chr == '.' || cmd_chr == 's')
        {
            if (cmd_chr != 'k')
            {
                salt_away = salt_mode;
            }
            ret = SRCH_DONE;
            if (cmd_chr == '+')
            {
                cmd_lst = save_str("+");
                if (!ignore_thru && g_kill_thru_kludge)
                {
                    ignore_thru = 1;
                }
            }
            else if (cmd_chr == '.')
            {
                cmd_lst = save_str(".");
                if (!ignore_thru && g_kill_thru_kludge)
                {
                    ignore_thru = 1;
                }
            }
            else if (cmd_chr == 's')
            {
                cmd_lst = save_str(pat_buf);
            }
            else
            {
                if (cmd_chr == ',')
                {
                    cmd_lst = save_str(",");
                }
                else
                {
                    cmd_lst = save_str("j");
                }
                mark_as_read(article_ptr(g_art));       // this article needs to die
            }
            if (!*h)
            {
                if (g_verbose)
                {
                    std::sprintf(g_msg, "Current article has no %s.", finding_str);
                }
                else
                {
                    std::sprintf(g_msg, "Null %s.", finding_str);
                }
                error_msg(g_msg);
                ret = SRCH_ABORT;
                goto exit;
            }
            if (g_verbose)
            {
                if (cmd_chr != '+' && cmd_chr != '.')
                {
                    std::printf("\nMarking %s \"%s\" as read.\n", finding_str, h);
                }
                else
                {
                    std::printf("\nSelecting %s \"%s\".\n", finding_str, h);
                }
                term_down(2);
            }
        }
        else if (!g_search_ahead)
        {
            g_search_ahead = ArticleNum{-1};
        }

        {                       // compensate for notes files
            for (int i = 24; *h && i--; h++)
            {
                if (*h == '\\')
                {
                    h++;
                }
            }
            *h = '\0';
        }
#ifdef DEBUG
        if (debug)
        {
            std::printf("\npattern = %s\n",pattern);
            term_down(2);
        }
#endif
    }
    {
        const char *s = compile(compex, pattern, true, fold_case);
        if (s != nullptr)
        {
            // compile regular expression
            error_msg(s);
            ret = SRCH_ABORT;
            goto exit;
        }
    }
    if (cmd_lst && std::strchr(cmd_lst,'='))
    {
        ret = SRCH_ERROR;               // listing subjects is an error?
    }
    if (g_general_mode == GM_SELECTOR)
    {
        if (!cmd_lst)
        {
            if (g_sel_mode == SM_ARTICLE)// set the selector's default command
            {
                cmd_lst = save_str("+");
            }
            else
            {
                cmd_lst = save_str("++");
            }
        }
        ret = SRCH_DONE;
    }
    if (salt_away)
    {
        char  salt_buf[LINE_BUF_LEN];
        char *s = salt_buf;
        const char *f = pattern;
        *s++ = '/';
        while (*f)
        {
            if (*f == '/')
            {
                *s++ = '\\';
            }
            *s++ = *f++;
        }
        *s++ = '/';
        if (do_read)
        {
            *s++ = 'r';
        }
        if (!fold_case)
        {
            *s++ = 'c';
        }
        if (ignore_thru)
        {
            *s++ = (ignore_thru == 1 ? 'I' : 'N');
        }
        if (how_much != ARTSCOPE_SUBJECT)
        {
            *s++ = g_scope_str[how_much];
            if (how_much == ARTSCOPE_ONE_HDR)
            {
                safe_copy(s,g_header_type[search_header].name.c_str(),LINE_BUF_LEN-(s-salt_buf));
                s += g_header_type[search_header].length;
                if (s - salt_buf > LINE_BUF_LEN-2)
                {
                    s = salt_buf + LINE_BUF_LEN - 2;
                }
            }
        }
        *s++ = ':';
        if (!cmd_lst)
        {
            cmd_lst = save_str("j");
        }
        safe_copy(s,cmd_lst,LINE_BUF_LEN-(s-salt_buf));
        kill_file_append(salt_buf, salt_away == 2? KF_GLOBAL : KF_LOCAL);
    }
    if (get_cmd)
    {
        if (g_use_threads)
        {
            newline();
        }
        else
        {
            std::fputs("\nSearching...\n",stdout);
            term_down(2);
        }
                                        // give them something to read
    }
    if (ignore_thru == 0 && g_kill_thru_kludge && cmd_lst //
        && (*cmd_lst == '+' || *cmd_lst == '.'))
    {
        ignore_thru = 1;
    }
    search_first = do_read || g_sel_rereading? g_abs_first
                      : (g_mode != MM_PROCESSING_KILL || ignore_thru > 0)? g_first_art : g_kill_first;
    if (top_start || g_art == ArticleNum{})
    {
        g_art.num = g_last_art.num+1;
        top_start = false;
    }
    if (backward)
    {
        if (cmd_lst && g_art <= g_last_art)
        {
            g_art.num++;                // include current article
        }
    }
    else
    {
        if (g_art > g_last_art)
        {
            g_art.num = search_first.num - 1;
        }
        else if (cmd_lst && g_art >= g_abs_first)
        {
            g_art.num--;                // include current article
        }
    }
    if (g_search_ahead > ArticleNum{})
    {
        if (!backward)
        {
            g_art.num = g_search_ahead.num - 1;
        }
        g_search_ahead = ArticleNum{-1};
    }
    TRN_ASSERT(!cmd_lst || *cmd_lst);
    perform_status_init(g_newsgroup_ptr->to_read);
    while (true)
    {
        // check if we're out of articles
        if (backward? ((g_art = article_prev(g_art)) < search_first)
                    : ((g_art = article_next(g_art)) > g_last_art))
        {
            break;
        }
        if (g_int_count)
        {
            g_int_count = 0;
            ret = SRCH_INTR;
            break;
        }
        g_artp = article_ptr(g_art);
        if (do_read || (!(g_artp->flags & AF_UNREAD) ^ (!g_sel_rereading ? AF_SEL : AF_NONE)))
        {
            if (wanted(compex, g_art, how_much))
            {
                                    // does the shoe fit?
                if (!cmd_lst)
                {
                    return SRCH_FOUND;
                }
                if (perform(cmd_lst, output_level && g_page_line == 1) < 0)
                {
                    std::free(cmd_lst);
                    return SRCH_INTR;
                }
            }
            else if (output_level && !cmd_lst && !(g_art.num % 50))
            {
                std::printf("...%ld", g_art.num);
                std::fflush(stdout);
            }
        }
        if (!output_level && g_page_line == 1)
        {
            perform_status(g_newsgroup_ptr->to_read, 60 / (how_much + 1));
        }
    }
exit:
    if (cmd_lst)
    {
        std::free(cmd_lst);
    }
    return ret;
}

// determine if article fits pattern
// returns true if it exists and fits pattern, false otherwise

static bool wanted(CompiledRegex *compex, ArticleNum art_num, ArtScope scope)
{
    Article* ap = article_find(art_num);

    if (!ap || !(ap->flags & AF_EXISTS))
    {
        return false;
    }

    switch (scope)
    {
    case ARTSCOPE_SUBJECT:
        std::strcpy(g_buf,"Subject: ");
        std::strncpy(g_buf+9,fetch_subj(art_num,false),256);
#ifdef DEBUG
        if (debug & DEB_SEARCH_AHEAD)
        {
            std::printf("%s\n",g_buf);
        }
#endif
        break;

    case ARTSCOPE_FROM:
        std::strcpy(g_buf, "From: ");
        std::strncpy(g_buf+6,fetch_from(art_num,false),256);
        break;

    case ARTSCOPE_ONE_HDR:
        g_untrim_cache = true;
        std::sprintf(g_buf, "%s: %s", g_header_type[g_art_srch_hdr].name.c_str(),
                prefetch_lines(art_num,g_art_srch_hdr,false));
        g_untrim_cache = false;
        break;

    default:
    {
        char*s;
        char ch;
        bool success = false;
        bool in_sig = false;
        if (scope != ARTSCOPE_BODY && scope != ARTSCOPE_BODY_NO_SIG)
        {
            if (!parse_header(art_num))
            {
                return false;
            }
            // see if it's in the header
            if (execute(compex,g_head_buf))      // does it match?
            {
                return true;                    // say, "Eureka!"
            }
            if (scope < ARTSCOPE_ARTICLE)
            {
                return false;
            }
        }
        if (g_parsed_art == art_num)
        {
            if (!art_open(art_num,g_header_type[PAST_HEADER].min_pos))
            {
                return false;
            }
        }
        else
        {
            if (!art_open(art_num,(ArticlePosition)0))
            {
                return false;
            }
            if (!parse_header(art_num))
            {
                return false;
            }
        }
        // loop through each line of the article
        seek_art_buf(g_header_type[PAST_HEADER].min_pos);
        while ((s = read_art_buf(false)) != nullptr)
        {
            if (scope == ARTSCOPE_BODY_NO_SIG && *s == '-' && s[1] == '-' //
                && (s[2] == '\n' || (s[2] == ' ' && s[3] == '\n')))
            {
                if (in_sig && success)
                {
                    return true;
                }
                in_sig = true;
            }
            char *nl_ptr = std::strchr(s, '\n');
            if (nl_ptr != nullptr)
            {
                ch = *++nl_ptr;
                *nl_ptr = '\0';
            }
            success = success || execute(compex,s) != nullptr;
            if (nl_ptr)
            {
                *nl_ptr = ch;
            }
            if (success && !in_sig)             // does it match?
            {
                return true;                    // say, "Eureka!"
            }
        }
        return false;                           // out of article, so no match
    }
    }
    return execute(compex,g_buf) != nullptr;
}
