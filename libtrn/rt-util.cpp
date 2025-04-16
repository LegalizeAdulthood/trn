/* rt-util.c
*  vi: set sw=4 ts=8 ai sm noet :
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#include "config/common.h"
#include "trn/rt-util.h"

#include "trn/artio.h"
#include "trn/cache.h"
#include "trn/charsubst.h"
#include "trn/intrp.h"
#include "trn/ng.h"
#include "trn/ngdata.h"
#include "trn/rt-select.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/utf.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

char g_spin_char{' '};           /* char to put back when we're done spinning */
long g_spin_estimate{};          /* best guess of how much work there is */
long g_spin_todo{};              /* the max word to do (might decrease) */
int  g_spin_count{};             /* counter for when to spin */
bool g_performed_article_loop{}; //
bool g_bkgnd_spinner{};          /* -B */
bool g_unbroken_subjects{};      /* -u */

static int s_spin_marks{25}; /* how many bargraph marks we want */

static char *output_change(char *cp, long num, const char *obj_type, const char *modifier, const char *action);

/* Name-munging routines written by Ross Ridge.
** Enhanced by Wayne Davison.
*/

/* Extract the full-name part of an email address, returning nullptr if not
** found.
*/
char *extract_name(char *name)
{
    name = skip_space(name);
    char *lparen = std::strchr(name, '(');
    char *rparen = std::strrchr(name, ')');
    char *langle = std::strchr(name, '<');
    if (!lparen && !langle)
    {
        return nullptr;
    }

    if (langle && (!lparen || !rparen || lparen > langle || rparen < langle))
    {
        if (langle == name)
        {
            return nullptr;
        }
        *langle = '\0';
    }
    else
    {
        name = lparen;
        *name++ = '\0';
        name = skip_space(name);
        if (name == rparen)
        {
            return nullptr;
        }
        if (rparen != nullptr)
        {
            *rparen = '\0';
        }
    }

    if (*name == '"')
    {
        name++;
        name = skip_space(name);
        char *s = std::strrchr(name, '"');
        if (s != nullptr)
        {
            *s = '\0';
        }
    }

    // strip trailing whitespace
    int len = std::strlen(name);
    while (len > 0 && std::isspace(name[len-1]))
    {
        len--;
    }
    name[len] = '\0';

    return name;
}

/* If necessary, compress a net user's full name by playing games with
** initials and the middle name(s).  If we start with "Ross Douglas Ridge"
** we try "Ross D Ridge", "Ross Ridge", "R D Ridge" and finally "R Ridge"
** before simply truncating the thing.  We also turn "R. Douglas Ridge"
** into "Douglas Ridge" and "Ross Ridge D.D.S." into "Ross Ridge" as a
** first step of the compaction, if needed.
*/
char *compress_name(char *name, int max)
{
    char *d;
    int   midlen;
    bool  notlast;

try_again:
    /* First remove white space from both ends. */
    name = skip_space(name);
    int len = std::strlen(name);
    if (len == 0)
    {
        return name;
    }
    char *s = name + len - 1;
    while (std::isspace(*s))
    {
        s--;
    }
    s[1] = '\0';

#ifdef USE_UTF_HACK
    int vis_len;
    int vis_namelen;
    int vis_midlen;
#else
#define vis_len len
#define vis_namelen namelen
#define vis_midlen midlen
#endif
#ifdef USE_UTF_HACK
    vis_len = visual_length_between(s, name) + 1;
#else
    vis_len = s - name + 1;
#endif
    if (vis_len <= max)
    {
        return name;
    }

    /* Look for characters that likely mean the end of the name
    ** and the start of some hopefully uninteresting additional info.
    ** Splitting at a comma is somewhat questionable, but since
    ** "Ross Ridge, The Great HTMU" comes up much more often than
    ** "Ridge, Ross" and since "R HTMU" is worse than "Ridge" we do
    ** it anyways.
    */
    for (d = name;;)
    {
#ifdef USE_UTF_HACK
        int w = byte_length_at(d);
        int x = byte_length_at(d + w);
#else
        int w = 1;
        int x = w + 1;
#endif
        char ch = d[w];
        char next = d[x];
        if (!ch)
        {
            break;
        }
        if (ch == ',' || ch == ';' || ch == '(' || ch == '@' || (ch == '-' && (next == '-' || next == ' ')))
        {
            d[w] = '\0';
            s = d;
            break;
        }
        d += w;
    }

    /* Find the last name */
    do
    {
        notlast = false;
        while (std::isspace(*s))
        {
            s--;
        }
        s[1] = '\0';
        len = s - name + 1;
#ifdef USE_UTF_HACK
        vis_len = visual_length_between(s, name) + 1;
#endif
        if (vis_len <= max)
        {
            return name;
        }
        /* If the last name is an abbreviation it's not the one we want. */
        if (*s == '.')
        {
            notlast = true;
        }
        while (!std::isspace(*s))
        {
            if (s == name) /* only one name */
            {
#ifdef USE_UTF_HACK
                terminate_string_at_visual_index(name, max);
#else
                name[max] = '\0';
#endif
                return name;
            }
            if (std::isdigit(*s))    /* probably a phone number */
            {
                notlast = true; /* so chuck it */
            }
            s--;
        }
    } while (notlast);

    char *last = s-- + 1;

    /* Look for a middle name */
    while (std::isspace(*s)) /* get rid of any extra space */
    {
        len--;
        s--;
    }
    char *mid = name;
    while (!std::isspace(*mid))
    {
#ifdef USE_UTF_HACK
        mid += byte_length_at(mid);
#else
        mid++;
#endif
    }
    int namelen = mid - name + 1;
#ifdef USE_UTF_HACK
    vis_namelen = visual_length_between(mid, name) + 1;
#endif
    if (mid == s + 1) /* no middle name */
    {
        mid = nullptr;
        midlen = 0;
    }
    else
    {
        *mid++ = '\0';
        while (std::isspace(*mid))
        {
            len--;
#ifdef USE_UTF_HACK
            mid += byte_length_at(mid);
#else
            mid++;
#endif
        }
        midlen = s - mid + 2;
#ifdef USE_UTF_HACK
        vis_midlen = visual_length_between(s, mid) + 2;
#endif
        /* If first name is an initial and middle isn't and it all fits
        ** without the first initial, drop it. */
        if (vis_len > max && mid != s)
        {
            if (vis_len - vis_namelen <= max &&
                ((mid[1] != '.' && (!name[1] || (name[1] == '.' && !name[2]))) || (*mid == '"' && *s == '"')))
            {
                len -= namelen;
                name = mid;
                namelen = midlen;
#ifdef USE_UTF_HACK
                vis_len = vis_namelen;
                vis_namelen = vis_midlen;
#endif
                mid = nullptr;
            }
            else if (*mid == '"' && *s == '"')
            {
                if (vis_midlen > max)
                {
                    name = mid + 1;
                    *s = '\0';
                    goto try_again;
                }
                len = midlen;
                last = mid;
                namelen = 0;
                mid = nullptr;
#ifdef USE_UTF_HACK
                vis_len = vis_midlen;
                vis_namelen = 0;
#endif
            }
        }
    }
    s[1] = '\0';
    if (mid && vis_len > max)
    {
        /* Turn middle names into initials */
        len -= s - mid + 2;
#ifdef USE_UTF_HACK
        vis_len -= visual_length_between(s, mid) + 2;
#endif
        d = mid;
        s = mid;
        while (*s)
        {
            if (std::isalpha(*s))
            {
                if (d != mid)
                {
#ifdef USE_UTF_HACK
                    int w = byte_length_at(s);
                    std::memset(d, ' ', w);
                    d += w;
#else
                    *d++ = ' ';
#endif
                }
#ifdef USE_UTF_HACK
                int w = byte_length_at(s);
                std::memcpy(d, s, w);
                d += w;
                s += w;
#else
                *d++ = *s++;
#endif
            }
            while (*s && !std::isspace(*s))
            {
#ifdef USE_UTF_HACK
                s += byte_length_at(s);
#else
                s++;
#endif
            }
            s = skip_space(s);
        }
        if (d != mid)
        {
            *d = '\0';
            midlen = d - mid + 1;
            len += midlen;
#ifdef USE_UTF_HACK
            vis_midlen = visual_length_between(d, mid) + 1;
            vis_len += vis_midlen;
#endif
        }
        else
        {
            mid = nullptr;
        }
    }
    if (vis_len > max)
    {
        /* If the first name fits without the middle initials, drop them */
        if (mid && vis_len - vis_midlen <= max)
        {
            len -= midlen;
#ifdef USE_UTF_HACK
            vis_len -= vis_midlen;
#endif
            mid = nullptr;
        }
        else if (namelen > 0)
        {
            /* Turn the first name into an initial */
#ifdef USE_UTF_HACK
            int w = byte_length_at(name);
            len -= namelen - (w + 1);
            name[w] = '\0';
            namelen = w + 1;
            vis_namelen = visual_width_at(name) + 1;
#else
            len -= namelen - 2;
            name[1] = '\0';
            namelen = 2;
#endif
            if (vis_len > max)
            {
                /* Dump the middle initials (if present) */
                if (mid)
                {
                    len -= midlen;
#ifdef USE_UTF_HACK
                    vis_len -= vis_midlen;
#endif
                    mid = nullptr;
                }
                if (vis_len > max)
                {
                    /* Finally just truncate the last name */
#ifdef USE_UTF_HACK
                    terminate_string_at_visual_index(last, max - 2);
#else
                    last[max - 2] = '\0';
#endif
                }
            }
        }
        else
        {
            namelen = 0;
#ifdef USE_UTF_HACK
            vis_namelen = 0;
#endif
        }
    }

    /* Paste the names back together */
    d = name + namelen;
    if (namelen)
    {
        d[-1] = ' ';
    }
    if (mid)
    {
        if (d != mid)
        {
            std::strcpy(d, mid);
        }
        d += midlen;
        d[-1] = ' ';
    }
#ifdef USE_UTF_HACK
    /* FIXME - need to move into some function */
    do
    {
        int i;
        int j;
        for (i = j = 0; last[i] && j < max;)
        {
            int w = byte_length_at(last + i);
            int v = visual_width_at(last + i);
            if (w == 0 || j + v > max)
            {
                break;
            }
            std::memcpy(d + i, last + i, w);
            i += w;
            j += v;
        }
        d[i] = '\0';
    } while (false);
#else
    safecpy(d, last, max); /* "max - (d-name)" would be overkill */
#endif
    return name;
}
#undef vis_len
#undef vis_namelen
#undef vis_midlen

/* Compress an email address, trying to keep as much of the local part of
** the addresses as possible.  The order of precence is @ ! %, but
** @ % ! may be better...
*/
char *compress_address(char *name, int max)
{
    char*start;

    /* Remove white space from both ends. */
    name = skip_space(name);
    int len = std::strlen(name);
    if (len == 0)
    {
        return name;
    }
    {
        char *s = name + len - 1;
        while (std::isspace(*s))
        {
            s--;
        }
        s[1] = '\0';
        if (*name == '<')
        {
            name++;
            if (*s == '>')
            {
                *s-- = '\0';
            }
        }
        len = s - name + 1;
        if (len <= max)
        {
            return name;
        }
    }

    char *at = nullptr;
    char *bang = nullptr;
    char *hack = nullptr;
    for (char *s = name + 1; *s; s++)
    {
        /* If there's whitespace in the middle then it's probably not
        ** really an email address. */
        if (std::isspace(*s))
        {
            name[max] = '\0';
            return name;
        }
        switch (*s)
        {
          case '@':
            if (at == nullptr)
            {
                at = s;
            }
            break;
          case '!':
            if (at == nullptr)
            {
                bang = s;
                hack = nullptr;
            }
            break;
          case '%':
            if (at == nullptr && hack == nullptr)
            {
                hack = s;
            }
            break;
        }
    }
    if (at == nullptr)
    {
        at = name + len;
    }

    if (hack != nullptr)
    {
        if (bang != nullptr)
        {
            if (at - bang - 1 >= max)
            {
                start = bang + 1;
            }
            else if (at - name >= max)
            {
                start = at - max;
            }
            else
            {
                start = name;
            }
        }
        else
        {
            start = name;
        }
    }
    else if (bang != nullptr)
    {
        if (at - name >= max)
        {
            start = at - max;
        }
        else
        {
            start = name;
        }
    }
    else
    {
        start = name;
    }
    if (len - (start - name) > max)
    {
        start[max] = '\0';
    }
    return start;
}

/* Fit the author name in <max> chars.  Uses the comment portion if present
** and pads with spaces.
*/
char *compress_from(const char *from, int size)
{
    static char lbuf[LBUFLEN];

    strcharsubst(lbuf, from ? from : "", sizeof lbuf, *g_charsubst);
    char *s = extract_name(lbuf);
    if (s != nullptr)
    {
        s = compress_name(s, size);
    }
    else
    {
        s = compress_address(lbuf, size);
    }

    int len = std::strlen(s);
    int vis_len;
#ifdef USE_UTF_HACK
    vis_len = visual_length_of(s);
#else
    vis_len = len;
#endif
    if (!len)
    {
        std::strcpy(s,"NO NAME");
        len = 7;
    }
    while (vis_len < size && len < sizeof lbuf - 1)
    {
        s[len++] = ' ';
        vis_len++;
    }
    s[len] = '\0';
    return s;
}

/* Fit the date in <max> chars. */
char *compress_date(const ARTICLE *ap, int size)
{
    char* t;

    std::strncpy(t = g_cmd_buf, std::ctime(&ap->date), size);
    char *s = std::strchr(t, '\n');
    if (s != nullptr)
    {
        *s = '\0';
    }
    t[size] = '\0';
    return t;
}

inline bool eq_ignore_case(char unknown, char lower)
{
    return std::tolower(unknown) == lower;
}

bool strip_one_Re(char *str, char **strp)
{
    bool has_Re = false;
    while (*str && at_grey_space(str))
    {
        str++;
    }
    if (eq_ignore_case(str[0], 'r') && eq_ignore_case(str[1], 'e')) /* check for Re: */
    {
        char *cp = str + 2;
        if (*cp == '^') /* allow Re^2: */
        {
            do
            {
                ++cp;
            } while (*cp <= '9' && *cp >= '0');
        }
        if (*cp == ':')
        {
            do
            {
                ++cp;
            } while (*cp && at_grey_space(cp));
            str = cp;
            has_Re = true;
        }
    }
    if (strp)
    {
        *strp = str;
    }

    return has_Re;
}

/* Parse the subject to look for any "Re[:^]"s at the start.
** Returns true if a Re was found.  If strp is non-nullptr, it
** will be set to the start of the interesting characters.
*/
bool subject_has_Re(char *str, char **strp)
{
    bool has_Re = false;
    while (strip_one_Re(str, &str))
    {
        has_Re = true;
    }
    if (strp)
    {
        *strp = str;
    }
    return has_Re;
}

/* Output a subject in <max> chars.  Does intelligent trimming that tries to
** save the last two words on the line, excluding "(was: blah)" if needed.
*/
const char *compress_subj(const ARTICLE *ap, int max)
{
    if (!ap)
    {
        return "<MISSING>";
    }

    /* Put a preceeding '>' on subjects that are replies to other articles */
    char *   cp = g_buf;
    ARTICLE *first = (g_threaded_group ? ap->subj->thread : ap->subj->articles);
    if (ap != first || (ap->flags & AF_HAS_RE)
     || (!(ap->flags&AF_UNREAD) ^ g_sel_rereading))
    {
        *cp++ = '>';
    }
    strcharsubst(cp, ap->subj->str + 4, (sizeof g_buf) - (cp-g_buf), *g_charsubst);

    /* Remove "(was: oldsubject)", because we already know the old subjects.
    ** Also match "(Re: oldsubject)".  Allow possible spaces after the ('s.
    */
    for (cp = g_buf; (cp = std::strchr(cp + 1, '(')) != nullptr;)
    {
        cp = skip_eq(++cp, ' ');
        if (eq_ignore_case(cp[0], 'w') && eq_ignore_case(cp[1], 'a') && eq_ignore_case(cp[2], 's') //
            && (cp[3] == ':' || cp[3] == ' '))
        {
            *--cp = '\0';
            break;
        }
        if (eq_ignore_case(cp[0], 'r') && eq_ignore_case(cp[1], 'e') //
            && ((cp[2] == ':' && cp[3] == ' ') || (cp[2] == '^' && cp[4] == ':')))
        {
            *--cp = '\0';
            break;
        }
    }
    int len = std::strlen(g_buf);
    if (!g_unbroken_subjects && len > max)
    {
        /* Try to include the last two words on the line while trimming */
        char *last_word = std::strrchr(g_buf, ' ');
        if (last_word != nullptr)
        {
            *last_word = '\0';
            char *next_to_last = std::strrchr(g_buf, ' ');
            if (next_to_last != nullptr)
            {
                if (next_to_last-g_buf >= len - max + 3 + 10-1)
                {
                    cp = next_to_last;
                }
                else
                {
                    cp = last_word;
                }
            }
            else
            {
                cp = last_word;
            }
            *last_word = ' ';
            if (cp - g_buf >= len - max + 3 + 10 - 1)
            {
                char* s = g_buf + max - (len-(cp-g_buf)+3);
                *s++ = '.';
                *s++ = '.';
                *s++ = '.';
                safecpy(s, cp + 1, max);
                len = max;
            }
        }
    }
    if (len > max)
    {
        g_buf[max] = '\0';
    }
    return g_buf;
}

/* Modified version of a spinner originally found in Clifford Adams' strn. */

static char     *s_spinchars{};
static int       s_spin_level{}; /* used to allow non-interfering nested spins */
static spin_mode s_spin_mode{};
static int       s_spin_place{}; /* represents place in s_spinchars array */
static int       s_spin_pos{};   /* the last spinbar position we drew */
static ART_NUM   s_spin_art{};
static ART_POS   s_spin_tell{};

void setspin(spin_mode mode)
{
    switch (mode)
    {
      case SPIN_FOREGROUND:
      case SPIN_BACKGROUND:
      case SPIN_BARGRAPH:
        if (!s_spin_level++)
        {
            s_spin_art = g_openart;
            if (s_spin_art != 0 && g_artfp)
            {
                s_spin_tell = tellart();
            }
            g_spin_count = 0;
            s_spin_place = 0;
        }
        if (s_spin_mode == SPIN_BARGRAPH)
        {
            mode = SPIN_BARGRAPH;
        }
        if (mode == SPIN_BARGRAPH)
        {
            if (s_spin_mode != SPIN_BARGRAPH)
            {
                s_spin_marks = (g_verbose? 25 : 10);
                std::printf(" [%*s]", s_spin_marks, "");
                for (int i = s_spin_marks + 1; i--; )
                {
                    backspace();
                }
                std::fflush(stdout);
            }
            s_spin_pos = 0;
        }
        s_spinchars = "|/-\\";
        s_spin_mode = mode;
        break;
      case SPIN_POP:
      case SPIN_OFF:
        if (s_spin_mode == SPIN_BARGRAPH)
        {
            s_spin_level = 1;
            spin(10000);
            if (g_spin_count >= g_spin_todo)
            {
                g_spin_char = ']';
            }
            g_spin_count--;
            s_spin_mode = SPIN_FOREGROUND;
        }
        if (mode == SPIN_POP && --s_spin_level > 0)
        {
            break;
        }
        s_spin_level = 0;
        if (s_spin_place)       /* we have spun at least once */
        {
            std::putchar(g_spin_char); /* get rid of spin character */
            backspace();
            std::fflush(stdout);
            s_spin_place = 0;
        }
        if (s_spin_art)
        {
            artopen(s_spin_art,s_spin_tell);   /* do not screw up the pager */
            s_spin_art = 0;
        }
        s_spin_mode = SPIN_OFF;
        g_spin_char = ' ';
        break;
    }
}

/* modulus for the spin... */
void spin(int count)
{
    if (!s_spin_level)
    {
        return;
    }
    switch (s_spin_mode)
    {
      case SPIN_BACKGROUND:
        if (!g_bkgnd_spinner)
        {
            return;
        }
        if (!(++g_spin_count % count))
        {
            std::putchar(s_spinchars[++s_spin_place % 4]);
            backspace();
            std::fflush(stdout);
        }
        break;
      case SPIN_FOREGROUND:
        if (!(++g_spin_count % count))
        {
            std::putchar('.');
            std::fflush(stdout);
        }
        break;
      case SPIN_BARGRAPH:
      {
          if (g_spin_todo == 0)
          {
            break;              /* bail out rather than crash */
          }
        int new_pos = (int)((long)s_spin_marks * ++g_spin_count / g_spin_todo);
        if (s_spin_pos < new_pos && g_spin_count <= g_spin_todo+1)
        {
            do
            {
                std::putchar('*');
            } while (++s_spin_pos < new_pos);
            s_spin_place = 0;
            std::fflush(stdout);
        }
        else if (!(g_spin_count % count))
        {
            std::putchar(s_spinchars[++s_spin_place % 4]);
            backspace();
            std::fflush(stdout);
        }
        break;
      }
    }
}

bool inbackground()
{
    return s_spin_mode == SPIN_BACKGROUND;
}

static int    s_prior_perform_cnt{};
static std::time_t s_prior_now{};
static long   s_ps_sel{};
static long   s_ps_cnt{};
static long   s_ps_missing{};

void perform_status_init(long cnt)
{
    g_perform_cnt = 0;
    g_error_occurred = false;
    g_subjline = nullptr;
    g_page_line = 1;
    g_performed_article_loop = true;

    s_prior_perform_cnt = 0;
    s_prior_now = 0;
    s_ps_sel = g_selected_count;
    s_ps_cnt = cnt;
    s_ps_missing = g_missing_count;

    g_spin_count = 0;
    s_spin_place = 0;
    s_spinchars = "v>^<";
}

void perform_status(long cnt, int spin)
{
    if (!(++g_spin_count % spin))
    {
        std::putchar(s_spinchars[++s_spin_place % 4]);
        backspace();
        std::fflush(stdout);
    }

    if (g_perform_cnt == s_prior_perform_cnt)
    {
        return;
    }

    std::time_t now = std::time(nullptr);
    if (now - s_prior_now < 2)
    {
        return;
    }

    s_prior_now = now;
    s_prior_perform_cnt = g_perform_cnt;

    long missing = g_missing_count - s_ps_missing;
    long kills = s_ps_cnt - cnt - missing;
    long sels = g_selected_count - s_ps_sel;

    if (!(kills | sels))
    {
        return;
    }

    carriage_return();
    if (g_perform_cnt != sels  && g_perform_cnt != -sels
     && g_perform_cnt != kills && g_perform_cnt != -kills)
    {
        std::printf("M:%d ", g_perform_cnt);
    }
    if (kills)
    {
        std::printf("K:%ld ", kills);
    }
    if (sels)
    {
        std::printf("S:%ld ", sels);
    }
#if 0
    if (missing > 0)
    {
        std::printf("(M: %ld) ", missing);
    }
#endif
    erase_eol();
    std::fflush(stdout);
}

static char *output_change(char *cp, long num, const char *obj_type, const char *modifier, const char *action)
{
    bool neg;

    if (num < 0)
    {
        num *= -1;
        neg = true;
    }
    else
    {
        neg = false;
    }

    if (cp != g_msg)
    {
        *cp++ = ',';
        *cp++ = ' ';
    }
    std::sprintf(cp, "%ld ", num);
    if (obj_type)
    {
        std::sprintf(cp += std::strlen(cp), "%s%s ", obj_type, plural(num));
    }
    cp += std::strlen(cp);
    const char *s = modifier;
    if (s != nullptr)
    {
        *cp++ = ' ';
        if (num != 1)
        {
            s = skip_ne(s, '|');
        }
        while (*s && *s != '|')
        {
            *cp++ = *s++;
        }
        *cp++ = ' ';
    }
    s = action;
    if (!neg)
    {
        s = skip_ne(s, '|');
    }
    while (*s && *s != '|')
    {
        *cp++ = *s++;
    }
    s++;
    if (neg)
    {
        s = skip_ne(s, '|');
    }
    while (*s)
    {
        *cp++ = *s++;
    }

    *cp = '\0';
    return cp;
}

int perform_status_end(long cnt, const char *obj_type)
{
    char*cp = g_msg;
    bool article_status = (*obj_type == 'a');

    if (g_perform_cnt == 0)
    {
        std::sprintf(g_msg, "No %ss affected.", obj_type);
        return 0;
    }

    long missing = g_missing_count - s_ps_missing;
    long kills = s_ps_cnt - cnt - missing;
    long sels = g_selected_count - s_ps_sel;

    if (!g_performed_article_loop)
    {
        cp = output_change(cp, (long)g_perform_cnt,
                           g_sel_mode == SM_THREAD? "thread" : "subject",
                           nullptr, "ERR|match|ed");
    }
    else if (g_perform_cnt != sels && g_perform_cnt != -sels //
             && g_perform_cnt != kills && g_perform_cnt != -kills)
    {
        cp = output_change(cp, (long)g_perform_cnt, obj_type, nullptr,
                           "ERR|match|ed");
        obj_type = nullptr;
    }
    if (kills)
    {
        cp = output_change(cp, kills, obj_type, nullptr,
                           article_status? "un||killed" : "more|less|");
        obj_type = nullptr;
    }
    if (sels)
    {
        cp = output_change(cp, sels, obj_type, nullptr, "de||selected");
        obj_type = nullptr;
    }
    if (article_status && missing > 0)
    {
        *cp++ = '(';
        cp = output_change(cp, missing, obj_type, "was|were", "ERR|missing|");
        *cp++ = ')';
    }

    std::strcpy(cp, ".");

    /* If we only selected/deselected things, return 1, else 2 */
    return (kills | missing) == 0? 1 : 2;
}
