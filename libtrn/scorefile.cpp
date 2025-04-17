/* This file Copyright 1992, 1993 by Clifford A. Adams */
/* scorefile.c
 *
 * A simple "proof of concept" scoring file for headers.
 * (yeah, right. :)
 */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/scorefile.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/cache.h"
#include "trn/head.h"
#include "trn/mempool.h"
#include "trn/ng.h"
#include "trn/rt-util.h"
#include "trn/score.h"  /* shared stuff... */
#include "trn/search.h" /* regex matches */
#include "trn/string-algos.h"
#include "trn/terminal.h" /* finish_command() */
#include "trn/url.h"
#include "trn/util.h"
#include "util/env.h" /* get_val */
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int  g_sf_num_entries{};   /* # of entries */
int  g_sf_score_verbose{}; /* when true, the scoring routine prints lots of info... */
bool g_sf_verbose{true};   /* if true print more stuff while loading */

/* list of score array markers (in g_htype field of score entry) */
/* entry is a file marker.  Score is the file level */
enum
{
    SF_FILE_MARK_START = -1,
    SF_FILE_MARK_END = -2,
    /* other misc. rules */
    SF_KILLTHRESHOLD = -3,
    SF_NEWAUTHOR = -4,
    SF_REPLY = -5
};

static ScoreFileEntry *s_sf_entries{};        /* array of entries */
static ScoreFile  *s_sf_files{};          //
static int       s_sf_num_files{};      //
static char    **s_sf_abbr{};           /* abbreviations */
static bool      s_newauthor_active{};  /* if true, s_newauthor is active */
static int       s_newauthor{};         /* bonus score given to a new (unscored) author */
static bool      s_sf_pattern_status{}; /* should we match by pattern? */
static bool      s_reply_active{};      /* if true, s_reply_score is active */
static int       s_reply_score{};       /* score amount added to an article reply */
static int       s_sf_file_level{};     /* how deep are we? */
static char      s_sf_buf[LBUFLEN]{};
static char    **s_sf_extra_headers{};
static int       s_sf_num_extra_headers{};
static bool      s_sf_has_extra_headers{};
static CompiledRegex   *s_sf_compex{};

static int sf_open_file(const char *name);
static void sf_file_clear();
static char *sf_file_getline(int fnum);

/* Must be called before any other sf_ routine (once for each group) */
void sf_init()
{
    g_sf_num_entries = 0;
    s_sf_extra_headers = nullptr;
    s_sf_num_extra_headers = 0;

    /* initialize abbreviation list */
    s_sf_abbr = (char**)safemalloc(256 * sizeof (char*));
    std::memset((char*)s_sf_abbr,0,256 * sizeof (char*));

    if (g_sf_verbose)
    {
        std::printf("\nReading score files...\n");
    }
    s_sf_file_level = 0;
    /* find # of levels */
    std::strcpy(s_sf_buf,filexp("%C"));
    int level = 0;
    for (char *s = s_sf_buf; *s; s++)
    {
        if (*s == '.')
        {
            level++;            /* count dots in group name */
        }
    }
    level++;

    /* the main read-in loop */
    for (int i = 0; i <= level; i++)
    {
        char *s = sf_get_filename(i);
        if (s != nullptr)
        {
            sf_do_file(s);
        }
    }

    /* do post-processing (set thresholds and detect extra header usage) */
    s_sf_has_extra_headers = false;
    /* set thresholds from the s_sf_entries */
    s_reply_active = false;
    s_newauthor_active = false;
    g_kill_thresh_active = false;
    for (int i = 0; i < g_sf_num_entries; i++)
    {
        if (s_sf_entries[i].head_type >= HEAD_LAST)
        {
            s_sf_has_extra_headers = true;
        }
        switch (s_sf_entries[i].head_type)
        {
        case SF_KILLTHRESHOLD:
            g_kill_thresh_active = true;
            g_kill_thresh = s_sf_entries[i].score;
            if (g_sf_verbose)
            {
                int j;
                /* rethink? */
                for (j = i+1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_KILLTHRESHOLD)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) /* no later thresholds */
                {
                    std::printf("killthreshold %d\n",g_kill_thresh);
                }
            }
            break;

        case SF_NEWAUTHOR:
            s_newauthor_active = true;
            s_newauthor = s_sf_entries[i].score;
            if (g_sf_verbose)
            {
                int j;
                /* rethink? */
                for (j = i+1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_NEWAUTHOR)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) /* no later newauthors */
                {
                    std::printf("New Author score: %d\n",s_newauthor);
                }
            }
            break;

        case SF_REPLY:
            s_reply_active = true;
            s_reply_score = s_sf_entries[i].score;
            if (g_sf_verbose)
            {
                int j;
                /* rethink? */
                for (j = i+1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_REPLY)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) /* no later reply rules */
                {
                    std::printf("Reply score: %d\n",s_reply_score);
                }
            }
            break;
        }
    }
}

void sf_clean()
{
    for (int i = 0; i < g_sf_num_entries; i++)
    {
        if (s_sf_entries[i].compex != nullptr)
        {
            free_compex(s_sf_entries[i].compex);
            std::free(s_sf_entries[i].compex);
        }
    }
    mp_free(MP_SCORE1);         /* free memory pool */
    if (s_sf_abbr)
    {
        for (int i = 0; i < 256; i++)
        {
            if (s_sf_abbr[i])
            {
                std::free(s_sf_abbr[i]);
                s_sf_abbr[i] = nullptr;
            }
        }
        std::free(s_sf_abbr);
    }
    if (s_sf_entries)
    {
        std::free(s_sf_entries);
    }
    s_sf_entries = nullptr;
    for (int i = 0; i < s_sf_num_extra_headers; i++)
    {
        std::free(s_sf_extra_headers[i]);
    }
    s_sf_num_extra_headers = 0;
    s_sf_extra_headers = nullptr;
}

void sf_grow()
{
    g_sf_num_entries++;
    if (g_sf_num_entries == 1)
    {
        s_sf_entries = (ScoreFileEntry*)safemalloc(sizeof (ScoreFileEntry));
    }
    else
    {
        s_sf_entries = (ScoreFileEntry*)saferealloc((char*)s_sf_entries,
                        g_sf_num_entries * sizeof (ScoreFileEntry));
    }
    s_sf_entries[g_sf_num_entries - 1] = ScoreFileEntry{}; /* init */
}

/* Returns -1 if no matching extra header found, otherwise returns offset
 * into the s_sf_extra_headers array.
 */
//char* head;           /* header name, (without ':' character) */
int sf_check_extra_headers(const char *head)
{
    static char lbuf[LBUFLEN];

    /* convert to lower case */
    safecpy(lbuf,head,sizeof lbuf - 1);
    for (char *s = lbuf; *s; s++)
    {
        if (std::isalpha(*s) && std::isupper(*s))
        {
            *s = std::tolower(*s);           /* convert to lower case */
        }
    }
    for (int i = 0; i < s_sf_num_extra_headers; i++)
    {
        if (!std::strcmp(s_sf_extra_headers[i],lbuf))
        {
            return i;
        }
    }
    return -1;
}

/* adds the header to the list of known extra headers if it is not already
 * known.
 */
//char* head;           /* new header name, (without ':' character) */
void sf_add_extra_header(const char *head)
{
    static char lbuf[LBUFLEN]; /* ick. */

    /* check to see if it's already known */
    /* first see if it is a known system header */
    safecpy(lbuf,head,sizeof lbuf - 2);
    int len = std::strlen(lbuf);
    lbuf[len] = ':';
    lbuf[len+1] = '\0';
    char *colonptr = lbuf + len;
    if (set_line_type(lbuf,colonptr) != SOME_LINE)
    {
        return;         /* known types should be interpreted in normal way */
    }
    /* then check to see if it's a known extra header */
    if (sf_check_extra_headers(head) >= 0)
    {
        return;
    }

    s_sf_num_extra_headers++;
    s_sf_extra_headers = (char**)saferealloc((char*)s_sf_extra_headers,
        s_sf_num_extra_headers * sizeof (char*));
    char *s = savestr(head);
    for (char *s2 = s; *s2; s2++)
    {
        if (std::isalpha(*s2) && std::isupper(*s2))
        {
            *s2 = std::tolower(*s2);         /* convert to lower case */
        }
    }
    s_sf_extra_headers[s_sf_num_extra_headers-1] = s;
}

//ART_NUM art;          /* article number to check */
//int hnum;             /* header number: offset into s_sf_extra_headers */
char *sf_get_extra_header(ArticleNum art, int hnum)
{
    static char lbuf[LBUFLEN];

    parse_header(art);   /* fast if already parsed */

    char *head = s_sf_extra_headers[hnum];
    int   len = std::strlen(head);

    for (char *s = g_head_buf; s && *s && *s != '\n'; s++)
    {
        if (string_case_equal(head, s, len))
        {
            s = std::strchr(s,':');
            if (!s)
            {
                return "";
            }
            s++;        /* skip the colon */
            s = skip_hor_space(s);
            if (!*s)
            {
                return "";
            }
            head = s;           /* now point to start of new text */
            s = std::strchr(s,'\n');
            if (!s)
            {
                return "";
            }
            *s = '\0';
            safecpy(lbuf,head,sizeof lbuf - 1);
            *s = '\n';
            return lbuf;
        }
        s = std::strchr(s,'\n');     /* '\n' will be skipped on loop increment */
    }
    return "";
}

/* keep this one outside the functions because it is shared */
static char s_sf_file[LBUFLEN];

/* filenames of type a/b/c/foo.bar.misc for group foo.bar.misc */
char *sf_get_filename(int level)
{
    std::strcpy(s_sf_file,filexp(get_val_const("SCOREDIR",DEFAULT_SCOREDIR)));
    std::strcat(s_sf_file,"/");
    if (!level)
    {
        /* allow environment variable later... */
        std::strcat(s_sf_file,"global");
    }
    else
    {
        std::strcat(s_sf_file,filexp("%C"));
        char *s = std::strrchr(s_sf_file, '/');
        /* maybe redo this logic later... */
        while (level--)
        {
            if (*s == '\0')     /* no more name to match */
            {
                return nullptr;
            }
            s = skip_ne(s, '.');
            if (*s && level)
            {
                s++;
            }
        }
        *s = '\0';      /* cut end of score file */
    }
    return s_sf_file;
}

/* given a string, if no slashes prepends SCOREDIR env. variable */
char *sf_cmd_fname(char *s)
{
    static char lbuf[LBUFLEN];

    char *s1 = std::strchr(s, '/');
    if (s1)
    {
        return s;
    }
    /* no slashes in this filename */
    std::strcpy(lbuf,get_val_const("SCOREDIR",DEFAULT_SCOREDIR));
    std::strcat(lbuf,"/");
    std::strcat(lbuf,s);
    return lbuf;
}

/* returns true if good command, false otherwise */
//char* cmd;            /* text of command */
//bool check;           /* if true, just check, don't execute */
bool sf_do_command(char *cmd, bool check)
{
    char* s;
    int i;

    if (!std::strncmp(cmd, "killthreshold", 13))
    {
        /* skip whitespace and = sign */
        for (s = cmd+13; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }

        /* make **sure** that there is a number here */
        i = std::atoi(s);
        if (i == 0)             /* it might not be a number */
        {
            if (!is_text_zero(s))
            {
                std::printf("\nBad killthreshold: %s",cmd);
                return false;   /* continue looping */
            }
        }
        if (check)
        {
            return true;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_KILLTHRESHOLD);
        s_sf_entries[g_sf_num_entries-1].score = i;
        return true;
    }
    if (!std::strncmp(cmd, "savescores", 10))
    {
        /* skip whitespace and = sign */
        for (s = cmd+10; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }
        if (!std::strncmp(s, "off", 3))
        {
            if (!check)
            {
                g_sc_savescores = false;
            }
            return true;
        }
        if (*s)         /* there is some argument */
        {
            if (check)
            {
                return true;
            }
            g_sc_savescores = true;
            return true;
        }
        std::printf("Bad savescores command: |%s|\n",cmd);
        return false;
    }
    if (!std::strncmp(cmd, "newauthor", 9))
    {
        /* skip whitespace and = sign */
        for (s = cmd+9; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }

        /* make **sure** that there is a number here */
        i = std::atoi(s);
        if (i == 0)             /* it might not be a number */
        {
            if (!is_text_zero(s))
            {
                std::printf("\nBad newauthor: %s",cmd);
                return false;   /* continue looping */
            }
        }
        if (check)
        {
            return true;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_NEWAUTHOR);
        s_sf_entries[g_sf_num_entries-1].score = i;
        return true;
    }
    if (!std::strncmp(cmd, "include", 7))
    {
        if (check)
        {
            return true;
        }
        s = skip_hor_space(cmd + 7); /* skip whitespace */
        if (!*s)
        {
            std::printf("Bad include command (missing filename)\n");
            return false;
        }
        sf_do_file(filexp(sf_cmd_fname(s)));
        return true;
    }
    if (!std::strncmp(cmd, "exclude", 7))
    {
        if (check)
        {
            return true;
        }
        s = skip_hor_space(cmd + 7); /* skip whitespace */
        if (!*s)
        {
            std::printf("Bad exclude command (missing filename)\n");
            return false;
        }
        sf_exclude_file(filexp(sf_cmd_fname(s)));
        return true;
    }
    if (!std::strncmp(cmd, "header", 6))
    {
        s = skip_hor_space(cmd + 7); /* skip whitespace */
        char *s2 = skip_ne(s, ':');
        if (!s2)
        {
            std::printf("\nBad header command (missing :)\n%s\n",cmd);
            return false;
        }
        if (check)
        {
            return true;
        }
        *s2 = '\0';
        sf_add_extra_header(s);
        *s2 = ':';
        return true;
    }
    if (!std::strncmp(cmd, "begin", 5))
    {
        s = skip_hor_space(cmd + 6); /* skip whitespace */
        if (!std::strncmp(s, "score", 5))
        {
            /* do something useful later */
            return true;
        }
        return true;
    }
    if (!std::strncmp(cmd, "reply", 5))
    {
        /* skip whitespace and = sign */
        for (s = cmd+5; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }

        /* make **sure** that there is a number here */
        i = std::atoi(s);
        if (i == 0)             /* it might not be a number */
        {
            if (!is_text_zero(s))
            {
                std::printf("\nBad reply command: %s\n",cmd);
                return false;   /* continue looping */
            }
        }
        if (check)
        {
            return true;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_REPLY);
        s_sf_entries[g_sf_num_entries-1].score = i;
        return true;
    }
    if (!std::strncmp(cmd, "file", 4))
    {
        if (check)
        {
            return true;
        }
        s = skip_hor_space(cmd + 4); /* skip whitespace */
        if (!*s)
        {
            std::printf("Bad file command (missing parameters)\n");
            return false;
        }
        char ch = *s++;
        s = skip_hor_space(s); /* skip whitespace */
        if (!*s)
        {
            std::printf("Bad file command (missing parameters)\n");
            return false;
        }
        if (s_sf_abbr[(int)ch])
        {
            std::free(s_sf_abbr[(int)ch]);
        }
        s_sf_abbr[(int)ch] = savestr(sf_cmd_fname(s));
        return true;
    }
    if (!std::strncmp(cmd, "end", 3))
    {
        s = skip_hor_space(cmd + 4); /* skip whitespace */
        if (!std::strncmp(s, "score", 5))
        {
            /* do something useful later */
            return true;
        }
        return true;
    }
    if (!std::strncmp(cmd, "newsclip", 8))
    {
        std::printf("Newsclip is no longer supported.\n");
        return false;
    }
    /* no command matched */
    std::printf("Unknown command: |%s|\n",cmd);
    return false;
}

//char* start1;         /* points to first character of keyword */
//char* end1;           /* points to last  character of keyword */
char *sf_freeform(char *start1, char *end1)
{
    char*s;

    bool error = false; /* be optimistic :-) */
    /* cases are # of letters in keyword */
    switch (end1 - start1 + 1)
    {
    case 7:
        if (!std::strncmp(start1,"pattern",7))
        {
            s_sf_pattern_status = true;
            break;
        }
        error = true;
        break;

    case 4:
#ifdef UNDEF
        /* here is an example of a hypothetical freeform key with an argument */
        if (!std::strncmp(start1,"date",4))
        {
            char* s1;
            int datenum;
            /* skip whitespace and = sign */
            s = skip_hor_space(end1 + 1);
            if (!*s)    /* ran out of line */
            {
                std::printf("freeform: date keyword: ran out of input\n");
                return s;
            }
            datenum = atoi(s);
            std::printf("Date: %d\n",datenum);
            s = skip_digits(s); /* skip datenum */
            end1 = s;           /* end of key data */
            break;
        }
#endif
        error = true;
        break;

    default:
        error = true;
        break;
    }
    if (error)
    {
        s = end1+1;
        char ch = *s;
        *s = '\0';
        std::printf("Scorefile freeform: unknown key: |%s|\n",start1);
        *s = ch;
        return nullptr; /* error indicated */
    }
    /* no error, so skip whitespace at end of key */
    return skip_hor_space(end1 + 1);
}

//bool check;           /* if true, just check the line, don't act. */
bool sf_do_line(char *line, bool check)
{
    if (!line || !*line)
    {
        return true;            /* very empty line */
    }
    char *s = line + std::strlen(line) - 1;
    if (*s == '\n')
    {
        *s = '\0';              /* kill the newline */
    }

    char ch = line[0];
    if (ch == '#')              /* comment */
    {
        return true;
    }

    /* reset any per-line bitflags */
    s_sf_pattern_status = false;

    if (std::isalpha(ch))            /* command line */
    {
        return sf_do_command(line,check);
    }

    /* skip whitespace */
    s = skip_hor_space(line);
    if (!*s || *s == '#')
    {
        return true;    /* line was whitespace or comment after whitespace */
    }
    /* convert line to lowercase (make optional later?) */
    for (char *s2 = s; *s2 != '\0'; s2++)
    {
        if (std::isupper(*s2))
        {
            *s2 = std::tolower(*s2);         /* convert to lower case */
        }
    }
    int i = std::atoi(s);
    if (i == 0)         /* it might not be a number */
    {
        if (!is_text_zero(s))
        {
            std::printf("\nBad scorefile line:\n|%s|\n",s);
            return false;
        }
    }
    /* add the line as a scoring entry */
    while (std::isdigit(*s) || *s == '+' || *s == '-' || is_hor_space(*s))
    {
        s++;    /* skip score */
    }
    char *s2;
    while (true)
    {
        for (s2 = s; *s2 && !is_hor_space(*s2); s2++)
        {
        }
        s2--;
        if (*s2 == ':') /* did header */
        {
            break;      /* go to set header routine */
        }
        s = sf_freeform(s,s2);
        if (!s || !*s)          /* used up all the line's text, or error */
        {
            std::printf("Scorefile entry error error (freeform parse).  ");
            std::printf("Line was:\n|%s|\n",line);
            return false;       /* error */
        }
    } /* while */
    /* s is start of header name, s2 points to the ':' character */
    int j = set_line_type(s, s2);
    if (j == SOME_LINE)
    {
        *s2 = '\0';
        j = sf_check_extra_headers(s);
        *s2 = ':';
        if (j >= 0)
        {
            j += HEAD_LAST;
        }
        else
        {
            std::printf("Unknown score header type.  Line follows:\n|%s|\n",line);
            return false;
        }
    }
    /* skip whitespace */
    s = skip_hor_space(++s2);
    if (!*s)    /* no pattern */
    {
        std::printf("Empty score pattern.  Line follows:\n|%s|\n",line);
        return false;
    }
    if (check)
    {
        return true;            /* limits of check */
    }
    sf_grow();          /* acutally make an entry */
    s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(j);
    s_sf_entries[g_sf_num_entries-1].score = i;
    if (s_sf_pattern_status)    /* in pattern matching mode */
    {
        s_sf_entries[g_sf_num_entries-1].flags |= 1;
        s_sf_entries[g_sf_num_entries-1].str1 = mp_save_str(s,MP_SCORE1);
        s_sf_compex = (CompiledRegex*)safemalloc(sizeof (CompiledRegex));
        init_compex(s_sf_compex);
        /* compile arguments: */
        /* 1st is COMPEX to store compiled regex in */
        /* 2nd is search string */
        /* 3rd should be true if the search string is a regex */
        /* 4th is true for case-insensitivity */
        s2 = compile(s_sf_compex,s,true,true);
        if (s2 != nullptr)
        {
            std::printf("Bad pattern : |%s|\n",s);
            std::printf("Compex returns: |%s|\n",s2);
            free_compex(s_sf_compex);
            std::free(s_sf_compex);
            s_sf_entries[g_sf_num_entries-1].compex = nullptr;
            return false;
        }
        s_sf_entries[g_sf_num_entries-1].compex = s_sf_compex;
    }
    else
    {
        s_sf_entries[g_sf_num_entries-1].flags &= 0xfe;
        s_sf_entries[g_sf_num_entries-1].str2 = nullptr;
        /* Note: consider allowing * wildcard on other header filenames */
        if (j == FROM_LINE)     /* may have * wildcard */
        {
            s2 = std::strchr(s, '*');
            if (s2 != nullptr)
            {
                s_sf_entries[g_sf_num_entries - 1].str2 = mp_save_str(s2 + 1, MP_SCORE1);
                *s2 = '\0';
            }
        }
        s_sf_entries[g_sf_num_entries-1].str1 = mp_save_str(s,MP_SCORE1);
    }
    return true;
}

void sf_do_file(const char *fname)
{
    char*s;

    int sf_fp = sf_open_file(fname);
    if (sf_fp < 0)
    {
        return;
    }
    s_sf_file_level++;
    if (g_sf_verbose)
    {
        for (int i = 1; i < s_sf_file_level; i++)
        {
            std::printf(".");                /* maybe later putchar... */
        }
        std::printf("Score file: %s\n",fname);
    }
    char *safefilename = savestr(fname);
    /* add end marker to scoring array */
    sf_grow();
    s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_FILE_MARK_START);
    /* file_level is 1 to n */
    s_sf_entries[g_sf_num_entries-1].score = s_sf_file_level;
    s_sf_entries[g_sf_num_entries-1].str2 = nullptr;
    s_sf_entries[g_sf_num_entries-1].str1 = savestr(safefilename);

    while ((s = sf_file_getline(sf_fp)) != nullptr)
    {
        std::strcpy(s_sf_buf,s);
        s = s_sf_buf;
        (void)sf_do_line(s,false);
    }
    /* add end marker to scoring array */
    sf_grow();
    s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_FILE_MARK_END);
    /* file_level is 1 to n */
    s_sf_entries[g_sf_num_entries-1].score = s_sf_file_level;
    s_sf_entries[g_sf_num_entries-1].str2 = nullptr;
    s_sf_entries[g_sf_num_entries-1].str1 = savestr(safefilename);
    std::free(safefilename);
    s_sf_file_level--;
}

//char* str;            /* string to match on */
//int ind;              /* index into s_sf_entries */
int score_match(char *str, int ind)
{
    const char *s1 = s_sf_entries[ind].str1;
    const char *s2 = s_sf_entries[ind].str2;

    if (s_sf_entries[ind].flags & 1)    /* pattern style match */
    {
        if (s_sf_entries[ind].compex != nullptr)
        {
            /* we have a good pattern */
            s2 = execute(s_sf_entries[ind].compex,str);
            if (s2 != nullptr)
            {
                return true;
            }
        }
        return false;
    }
    /* default case */
    const char *s3 = std::strstr(str, s1);
    return s3 != nullptr && (!s2 || std::strstr(s3 + std::strlen(s1), s2));
}

int sf_score(ArticleNum a)
{
    if (is_unavailable(a))
    {
        return LOWSCORE;        /* unavailable arts get low negative score. */
    }

    /* if there are no score entries, then the answer is real easy and quick */
    if (g_sf_num_entries == 0)
    {
        return 0;
    }
    bool old_untrim = g_untrim_cache;
    g_untrim_cache = true;
    g_sc_scoring = true;                /* loop prevention */
    int sum = 0;

    /* parse the header now if there are extra headers */
    /* (This could save disk accesses.) */
    if (s_sf_has_extra_headers)
    {
        parse_header(a);
    }

    char *s;            /* misc */

    for (int i = 0; i < g_sf_num_entries; i++)
    {
        HeaderLineType h = s_sf_entries[i].head_type;
        if (h <= 0)     /* don't use command headers for scoring */
        {
            continue;   /* the outer for loop */
        }
        /* if this head_type has been done before, this entry
           has already been done */
        if (s_sf_entries[i].flags & 2)          /* rule has been applied */
        {
            s_sf_entries[i].flags &= 0xfd; /* turn off flag */
            continue;                   /* ...with the next rule */
        }

        /* sf_get_line will return ptr to buffer (already lowercased string) */
        s = sf_get_line(a,h);
        if (!s || !*s)  /* no such line for the article */
        {
            continue;   /* with the s_sf_entries. */
        }

        /* do the matches for this header */
        for (int j = i; j < g_sf_num_entries; j++)
        {
            /* see if there is a match */
            if (h == s_sf_entries[j].head_type)
            {
                if (j != i)             /* set flag only for future rules */
                {
                    s_sf_entries[j].flags |= 2; /* rule has been applied. */
                }
                if (score_match(s, j))
                {
                    sum = sum + s_sf_entries[j].score;
                    if (h == FROM_LINE)
                    {
                        article_ptr(a)->score_flags |= SFLAG_AUTHOR;
                    }
                    if (g_sf_score_verbose)
                    {
                        sf_print_match(j);
                    }
                }
            }
        }
    }
    if (s_newauthor_active && !(article_ptr(a)->score_flags & SFLAG_AUTHOR))
    {
        sum = sum+s_newauthor;  /* add new author bonus */
        if (g_sf_score_verbose)
        {
            std::printf("New Author: %d\n",s_newauthor);
            /* consider: print which file the bonus came from */
        }
    }
    if (s_reply_active)
    {
        /* should be in cache if a rule above used the subject */
        s = fetch_cache(a, SUBJ_LINE, true);
        /* later: consider other possible reply forms (threading?) */
        if (s && subject_has_re(s, nullptr))
        {
            sum = sum+s_reply_score;
            if (g_sf_score_verbose)
            {
                std::printf("Reply: %d\n",s_reply_score);
                /* consider: print which file the bonus came from */
            }
        }
    }
    g_untrim_cache = old_untrim;
    g_sc_scoring = false;
    return sum;
}

/* returns changed score line or nullptr if no changes */
char *sf_missing_score(const char *line)
{
    static char lbuf[LBUFLEN];

    /* save line since it is probably pointing at (the TRN-global) g_buf */
    char *s = savestr(line);
    std::printf("Possibly missing score.\n"
           "Type a score now or delete the colon to abort this entry:\n");
    g_buf[0] = ':';
    g_buf[1] = FINISHCMD;
    int i = finish_command(true); /* print the CR */
    if (!i)                       /* there was no score */
    {
        std::free(s);
        return nullptr;
    }
    std::strcpy(lbuf,g_buf+1);
    i = std::strlen(lbuf);
    lbuf[i] = ' ';
    lbuf[i+1] = '\0';
    std::strcat(lbuf,s);
    std::free(s);
    return lbuf;
}

/* Interprets the '\"' command for creating new score entries online */
/* consider using some external buffer rather than the 2 internal ones */
void sf_append(char *line)
{
    const char* scoretext; /* text after the score# */
    char*       filename;  /* expanded filename */
    static char filebuf[LBUFLEN];
    char*       s;

    if (!line)
    {
        return;         /* do nothing with empty string */
    }

    char filechar = *line; /* ch is file abbreviation */

    if (filechar == '?')        /* list known file abbreviations */
    {
        std::printf("List of abbreviation/file pairs\n") ;
        for (int i = 0; i < 256; i++)
        {
            if (s_sf_abbr[i])
            {
                std::printf("%c %s\n",(char)i,s_sf_abbr[i]);
            }
        }
        std::printf("\" [The current newsgroup's score file]\n");
        std::printf("* [The global score file]\n");
        return;
    }

    /* skip whitespace after filechar */
    char *scoreline = skip_hor_space(line + 1);

    char ch = *scoreline; /* first non-whitespace after filechar */
    /* If the scorefile line does not begin with a number,
       and is not a valid command, request a score */
    if (!std::isdigit(ch) && ch != '+' && ch != '-' && ch != ':' && ch != '!' && ch != '#')
    {
        if (!sf_do_line(scoreline,true))    /* just checking */
        {
            scoreline = sf_missing_score(scoreline);
            if (!scoreline)     /* no score typed */
            {
                std::printf("Score entry aborted.\n");
                return;
            }
        }
    }

    /* scoretext = first non-whitespace after score# */
    for (scoretext = scoreline;
         std::isdigit(*scoretext) || *scoretext == '+' || *scoretext == '-' || is_hor_space(*scoretext);
         scoretext++)
    {
    }
    /* special one-character shortcuts */
    if (*scoretext && scoretext[1] == '\0')
    {
        static char lbuf[LBUFLEN];
        switch (*scoretext)
        {
        case 'F':     /* domain-shortened FROM line */
            std::strcpy(lbuf,scoreline);
            lbuf[std::strlen(lbuf)-1] = '\0';
            std::strcat(lbuf,filexp("from: %y"));
            scoreline = lbuf;
            break;

        case 'S':     /* current subject */
            std::strcpy(lbuf,scoreline);
            s = fetch_cache(g_art, SUBJ_LINE,true);
            if (!s || !*s)
            {
                std::printf("No subject: score entry aborted.\n");
                return;
            }
            if (s[0] == 'R' && s[1] == 'e' && s[2] == ':' && s[3] == ' ')
            {
                s += 4;
            }
            /* change this next line if LBUFLEN changes */
            std::sprintf(lbuf+(std::strlen(lbuf)-1),"subject: %.900s",s);
            scoreline = lbuf;
            break;

        default:
            std::printf("\nBad scorefile line: |%s| (not added)\n", line);
            return;
        }
        std::printf("%s\n",scoreline);
    }

    /* test the scoring line unless filechar is '!' (meaning do it now) */
    if (!sf_do_line(scoreline, filechar != '!'))
    {
        std::printf("Bad score line (ignored)\n");
        return;
    }
    if (filechar == '!')
    {
        return;         /* don't actually append to file */
    }
    if (filechar == '"')        /* do local group */
    {
        /* Note: should probably be changed to use sf_ file functions */
        std::strcpy(filebuf,get_val_const("SCOREDIR",DEFAULT_SCOREDIR));
        std::strcat(filebuf,"/%C");
        filename = filebuf;
    }
    else if (filechar == '*')   /* do global scorefile */
    {
        /* Note: should probably be changed to use sf_ file functions */
        std::strcpy(filebuf,get_val_const("SCOREDIR",DEFAULT_SCOREDIR));
        std::strcat(filebuf,"/global");
        filename = filebuf;
    }
    else if (!(filename = s_sf_abbr[(int) filechar]))
    {
        std::printf("\nBad file abbreviation: %c\n",filechar);
        return;
    }
    filename = filexp(sf_cmd_fname(filename));  /* allow shortcuts */
    /* make sure directory exists... */
    makedir(filename, MD_FILE);
    sf_file_clear();
    std::FILE *fp = std::fopen(filename, "a");
    if (fp != nullptr)
    {
        std::fprintf(fp, "%s\n", scoreline); /* open (or create) for append */
        std::fclose(fp);
    }
    else /* unsuccessful in opening file */
    {
        std::printf("\nCould not open (for append) file %s\n",filename);
    }
}

/* returns a lowercased copy of the header line type h in private buffer */
char *sf_get_line(ArticleNum a, HeaderLineType h)
{
    static char sf_getline[LBUFLEN];
    char* s;

    if (h <= SOME_LINE)
    {
        std::printf("sf_get_line(%d,%d): bad header type\n",(int)a,h);
        std::printf("(Internal error: header number too low)\n");
        *sf_getline = '\0';
        return sf_getline;
    }
    if (h >= HEAD_LAST)
    {
        if (h-HEAD_LAST < s_sf_num_extra_headers)
        {
            s = sf_get_extra_header(a,h-HEAD_LAST);
        }
        else
        {
            std::printf("sf_get_line(%d,%d): bad header type\n",(int)a,h);
            std::printf("(Internal error: header number too high)\n");
            *sf_getline = '\0';
            return sf_getline;
        }
    }
    else if (h == SUBJ_LINE)
    {
        s = fetch_cache(a,h,true);       /* get compressed copy */
    }
    else
    {
        s = prefetch_lines(a,h,false);   /* don't make a copy */
    }
    if (!s)
    {
        *sf_getline = '\0';
    }
    else
    {
        safecpy(sf_getline,s,sizeof sf_getline - 1);
    }

    for (char *t = sf_getline; *t; t++)
    {
        if (std::isupper(*t))
        {
            *t = std::tolower(*t);
        }
    }
    return sf_getline;
}

/* given an index into s_sf_entries, print information about that index */
void sf_print_match(int indx)
{
    int  i;
    int  level; /* level is initialized iff used */
    char*head_name;
    char*pattern;

    for (i = indx; i >= 0; i--)
    {
        int j = s_sf_entries[i].head_type;
        if (j == SF_FILE_MARK_START)  /* found immediate inclusion. */
        {
            break;
        }
        if (j == SF_FILE_MARK_END)      /* found included file, skip */
        {
            int tmplevel = s_sf_entries[i].score;
            int k;
            for (k = i; k >= 0; k--)
            {
                if (s_sf_entries[k].head_type == static_cast<HeaderLineType>(SF_FILE_MARK_START) //
                    && s_sf_entries[k].score == tmplevel)
                {
                    break;      /* inner for loop */
                }
            }
            i = k;      /* will be decremented again */
        }
    }
    if (i >= 0)
    {
        level = s_sf_entries[i].score;
    }
    /* print the file markers. */
    for (; i >= 0; i--)
    {
        if (s_sf_entries[i].head_type == static_cast<HeaderLineType>(SF_FILE_MARK_START) //
            && s_sf_entries[i].score <= level)
        {
            level--;    /* go out... */
            for (int k = 0; k < level; k++)
            {
                std::printf(".");            /* make putchar later? */
            }
            std::printf("From file: %s\n",s_sf_entries[i].str1);
            if (level == 0)             /* top level */
            {
                break;          /* out of the big for loop */
            }
        }
    }
    if (s_sf_entries[indx].flags & 1)   /* regex type */
    {
        pattern = "pattern ";
    }
    else
    {
        pattern = "";
    }

    if (s_sf_entries[indx].head_type >= HEAD_LAST)
    {
        head_name = s_sf_extra_headers[s_sf_entries[indx].head_type-HEAD_LAST];
    }
    else
    {
        head_name = g_htype[s_sf_entries[indx].head_type].name;
    }
    std::printf("%d %s%s: %s", s_sf_entries[indx].score,pattern,head_name,
           s_sf_entries[indx].str1);
    if (s_sf_entries[indx].str2)
    {
        std::printf("*%s",s_sf_entries[indx].str2);
    }
    std::printf("\n");
}

void sf_exclude_file(const char *fname)
{
    int       start;
    int       end;
    ScoreFileEntry *tmp_entries;

    for (start = 0; start < g_sf_num_entries; start++)
    {
        if (s_sf_entries[start].head_type == static_cast<HeaderLineType>(SF_FILE_MARK_START)
         && !std::strcmp(s_sf_entries[start].str1,fname))
        {
            break;
        }
    }
    if (start == g_sf_num_entries)
    {
        std::printf("Exclude: file |%s| was not included\n",fname);
        return;
    }
    for (end = start+1; end < g_sf_num_entries; end++)
    {
        if (s_sf_entries[end].head_type==SF_FILE_MARK_END
         && !std::strcmp(s_sf_entries[end].str1,fname))
        {
            break;
        }
    }
    if (end == g_sf_num_entries)
    {
        std::printf("Exclude: file |%s| is incomplete at exclusion command\n",
                fname);
        /* insert more explanation later? */
        return;
    }

    int newnum = g_sf_num_entries - (end - start) - 1;
#ifdef UNDEF
    /* Deal with exclusion of all scorefile entries.
     * This cannot happen since the exclusion command has to be within a
     * file.  Code kept in case online exclusions allowed later.
     */
    if (newnum==0)
    {
        g_sf_num_entries = 0;
        std::free(s_sf_entries);
        s_sf_entries = nullptr;
        return;
    }
#endif
    tmp_entries = (ScoreFileEntry*)safemalloc(newnum*sizeof(ScoreFileEntry));
    /* copy the parts into tmp_entries */
    if (start > 0)
    {
        std::memcpy((char*)tmp_entries,(char*)s_sf_entries,start * sizeof (ScoreFileEntry));
    }
    if (end < g_sf_num_entries-1)
    {
        std::memcpy((char*)(tmp_entries+start),(char*)(s_sf_entries+end+1),(g_sf_num_entries-end-1) * sizeof (ScoreFileEntry));
    }
    std::free(s_sf_entries);
    s_sf_entries = tmp_entries;
    g_sf_num_entries = newnum;
    if (g_sf_verbose)
    {
        std::printf("Excluded file: %s\n",fname);
    }
}

//char* filespec;               /* file abbrev. or name */
void sf_edit_file(const char *filespec)
{
    char filebuf[LBUFLEN];      /* clean up buffers */

    if (!filespec || !*filespec)
    {
        return;         /* empty, do nothing (error later?) */
    }
    char filechar = *filespec;
    /* if more than one character use as filename */
    if (filespec[1])
    {
        std::strcpy(filebuf,filespec);
    }
    else if (filechar == '"')   /* edit local group */
    {
        /* Note: should probably be changed to use sf_ file functions */
        std::strcpy(filebuf,get_val_const("SCOREDIR",DEFAULT_SCOREDIR));
        std::strcat(filebuf,"/%C");
    }
    else if (filechar == '*')   /* edit global scorefile */
    {
        /* Note: should probably be changed to use sf_ file functions */
        std::strcpy(filebuf,get_val_const("SCOREDIR",DEFAULT_SCOREDIR));
        std::strcat(filebuf,"/global");
    }
    else        /* abbreviation */
    {
        if (!s_sf_abbr[(int) filechar])
        {
            std::printf("\nBad file abbreviation: %c\n",filechar);
            return;
        }
        std::strcpy(filebuf,s_sf_abbr[(int)filechar]);
    }
    char *fname_noexpand = sf_cmd_fname(filebuf);
    std::strcpy(filebuf,filexp(fname_noexpand));
    /* make sure directory exists... */
    if (!makedir(filebuf, MD_FILE))
    {
        (void)edit_file(fname_noexpand);
        sf_file_clear();
    }
    else
    {
        std::printf("Can't make %s\n",filebuf);
    }
}

/* returns file number */
/* if file number is negative, the file does not exist or cannot be opened */
static int sf_open_file(const char *name)
{
    char* s;
    int i;

    if (!name || !*name)
    {
        return 0;       /* unable to open */
    }
    for (i = 0; i < s_sf_num_files; i++)
    {
        if (!std::strcmp(s_sf_files[i].fname, name))
        {
            if (s_sf_files[i].num_lines < 0)    /* nonexistent */
            {
                return -1;      /* no such file */
            }
            s_sf_files[i].line_on = 0;
            return i;
        }
    }
    s_sf_num_files++;
    s_sf_files = (ScoreFile*)saferealloc((char*)s_sf_files,
        s_sf_num_files * sizeof (ScoreFile));
    s_sf_files[i].fname = savestr(name);
    s_sf_files[i].num_lines = 0;
    s_sf_files[i].num_alloc = 0;
    s_sf_files[i].line_on = 0;
    s_sf_files[i].lines = nullptr;

    char *temp_name = nullptr;
    if (string_case_equal(name, "URL:", 4))
    {
        char lbuf[1024];
        safecpy(lbuf,name,sizeof lbuf - 4);
        name = lbuf;
        temp_name = temp_filename();
        if (!url_get(name+4,temp_name))
        {
            name = nullptr;
        }
        else
        {
            name = temp_name;
        }
    }
    if (!name)
    {
        s_sf_files[i].num_lines = -1;
        return -1;
    }
    std::FILE *fp = std::fopen(name, "r");
    if (!fp)
    {
        s_sf_files[i].num_lines = -1;
        return -1;
    }
    while ((s = std::fgets(s_sf_buf, LBUFLEN - 4, fp)) != nullptr)
    {
        if (s_sf_files[i].num_lines >= s_sf_files[i].num_alloc)
        {
            s_sf_files[i].num_alloc += 100;
            s_sf_files[i].lines = (char**)saferealloc((char*)s_sf_files[i].lines,
                s_sf_files[i].num_alloc*sizeof(char**));
        }
        /* I kind of like the next line in a twisted sort of way. */
        s_sf_files[i].lines[s_sf_files[i].num_lines++] = mp_save_str(s,MP_SCORE2);
    }
    std::fclose(fp);
    if (temp_name)
    {
        remove(temp_name);
    }
    return i;
}

static void sf_file_clear()
{
    for (int i = 0; i < s_sf_num_files; i++)
    {
        if (s_sf_files[i].fname)
        {
            std::free(s_sf_files[i].fname);
        }
        if (s_sf_files[i].num_lines > 0)
        {
            /* memory pool takes care of freeing line contents */
            std::free(s_sf_files[i].lines);
        }
    }
    mp_free(MP_SCORE2);
    if (s_sf_files)
    {
        std::free(s_sf_files);
    }
    s_sf_files = (ScoreFile*)nullptr;
    s_sf_num_files = 0;
}

static char *sf_file_getline(int fnum)
{
    if (fnum < 0 || fnum >= s_sf_num_files)
    {
        return nullptr;
    }
    if (s_sf_files[fnum].line_on >= s_sf_files[fnum].num_lines)
    {
        return nullptr;         /* past end of file, or empty file */
    }
    /* below: one of the more twisted lines of my career  (:-) */
    return s_sf_files[fnum].lines[s_sf_files[fnum].line_on++];
}
