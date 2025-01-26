/* head.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "head.h"

#include "artio.h"
#include "cache.h"
#include "datasrc.h"
#include "final.h"
#include "list.h"
#include "mempool.h"
#include "ng.h"
#include "ngdata.h"
#include "nntp.h"
#include "nntpclient.h"
#include "rt-process.h"
#include "rt-util.h"
#include "string-algos.h"
#include "util.h"
#include "util2.h"

#include <parsedate/parsedate.h>

#define HIDDEN    (HT_HIDE|HT_DEFHIDE)
#define MAGIC_ON  (HT_MAGICOK|HT_MAGIC|HT_DEFMAGIC)
#define MAGIC_OFF (HT_MAGICOK)

#define XREF_CACHED HT_CACHED
#define NGS_CACHED  HT_NONE
#define FILT_CACHED HT_NONE

/* This array must stay in the same order as the enum values header_line_type */
// clang-format off
HEADTYPE g_htype[HEAD_LAST] = {
    /* name             minpos  maxpos  length   flag */
    {"",/*BODY*/        0,      0,      0,      HT_NONE         },
    {"",/*SHOWN*/       0,      0,      0,      HT_NONE         },
    {"",/*HIDDEN*/      0,      0,      0,      HIDDEN          },
    {"",/*CUSTOM*/      0,      0,      0,      HT_NONE         },
    {"unrecognized",    0,      0,      12,     HIDDEN          },
    {"author",          0,      0,      6,      HT_NONE         },
    {"bytes",           0,      0,      5,      HIDDEN|HT_CACHED},
    {"content-name",    0,      0,      12,     HIDDEN          },
    {"content-disposition",
                        0,      0,      19,     HIDDEN          },
    {"content-length",  0,      0,      14,     HIDDEN          },
    {"content-transfer-encoding",
                        0,      0,      25,     HIDDEN          },
    {"content-type",    0,      0,      12,     HIDDEN          },
    {"distribution",    0,      0,      12,     HT_NONE         },
    {"date",            0,      0,      4,      MAGIC_ON        },
    {"expires",         0,      0,      7,      HIDDEN|MAGIC_ON },
    {"followup-to",     0,      0,      11,     HT_NONE         },
    {"from",            0,      0,      4,      MAGIC_OFF|HT_CACHED},
    {"in-reply-to",     0,      0,      11,     HIDDEN          },
    {"keywords",        0,      0,      8,      HT_NONE         },
    {"lines",           0,      0,      5,      HT_CACHED       },
    {"mime-version",    0,      0,      12,     MAGIC_ON|HIDDEN },
    {"message-id",      0,      0,      10,     HIDDEN|HT_CACHED},
    {"newsgroups",      0,      0,      10,     MAGIC_ON|HIDDEN|NGS_CACHED},
    {"path",            0,      0,      4,      HIDDEN          },
    {"relay-version",   0,      0,      13,     HIDDEN          },
    {"reply-to",        0,      0,      8,      HT_NONE         },
    {"references",      0,      0,      10,     HIDDEN|FILT_CACHED},
    {"summary",         0,      0,      7,      HT_NONE         },
    {"subject",         0,      0,      7,      MAGIC_ON|HT_CACHED},
    {"xref",            0,      0,      4,      HIDDEN|XREF_CACHED},
};
// clang-format on

#undef HIDDEN
#undef MAGIC_ON
#undef MAGIC_OFF
#undef NGS_CACHED
#undef XREF_CACHED

USER_HEADTYPE   *g_user_htype{};
short            g_user_htypeix[26];
int              g_user_htype_cnt{};
int              g_user_htype_max{};
ART_NUM          g_parsed_art{}; /* the article number we've parsed */
header_line_type g_in_header{};  /* are we decoding the header? */
char            *g_headbuf;

static ARTICLE         *s_parsed_artp{}; /* the article ptr we've parsed */
static long             s_headbuf_size;
static bool             s_first_one; /* is this the 1st occurrence of this header line? */
static bool             s_reading_nntp_header;
static header_line_type s_htypeix[26]{};

void head_init()
{
    for (int i = HEAD_FIRST + 1; i < HEAD_LAST; i++)
        s_htypeix[*g_htype[i].name - 'a'] = static_cast<header_line_type>(i);

    g_user_htype_max = 10;
    g_user_htype = (USER_HEADTYPE*)safemalloc(g_user_htype_max
                                            * sizeof (USER_HEADTYPE));
    g_user_htype[g_user_htype_cnt++].name = "*";

    s_headbuf_size = LBUFLEN * 8;
    g_headbuf = safemalloc(s_headbuf_size);
}

void head_final()
{
    safefree0(g_headbuf);
    safefree0(g_user_htype);
    g_user_htype_cnt = 0;
}

#ifdef DEBUG
void dumpheader(char *where)
{
    printf("header: %ld %s", (long)g_parsed_art, where);

    for (int i = HEAD_FIRST-1; i < HEAD_LAST; i++) {
        printf("%15s %4ld %4ld %03o\n",g_htype[i].name,
               (long)g_htype[i].minpos, (long)g_htype[i].maxpos,
               g_htype[i].flags);
    }
}
#endif

header_line_type set_line_type(char *bufptr, const char *colon)
{
    char* t;
    char* f;

    if (colon-bufptr > sizeof g_msg)
        return SOME_LINE;

    for (t = g_msg, f = bufptr; f < colon; f++, t++) {
        /* guard against space before : */
        if (isspace(*f))
            return SOME_LINE;
        *t = isupper(*f) ? static_cast<char>(tolower(*f)) : *f;
    }
    *t = '\0';
    f = g_msg;                          /* get g_msg into a register */
    int len = t - f;

    /* now scan the HEADTYPE table, backwards so we don't have to supply an
     * extra terminating value, using first letter as index, and length as
     * optimization to avoid calling subroutine strEQ unnecessarily.  Hauls.
     */
    if (*f >= 'a' && *f <= 'z') {
        for (int i = s_htypeix[*f - 'a']; *g_htype[i].name == *f; i--)
        {
            if (len == g_htype[i].length && !strcmp(f, g_htype[i].name))
                return static_cast<header_line_type>(i);
        }
        if (len == g_htype[CUSTOM_LINE].length
         && !strcmp(f,g_htype[(((0+1)+1)+1)].name))
            return CUSTOM_LINE;
        for (int i = g_user_htypeix[*f - 'a']; *g_user_htype[i].name == *f; i--) {
            if (len >= g_user_htype[i].length
             && !strncmp(f, g_user_htype[i].name, g_user_htype[i].length)) {
                if (g_user_htype[i].flags & HT_HIDE)
                    return HIDDEN_LINE;
                return SHOWN_LINE;
            }
        }
    }
    return SOME_LINE;
}

header_line_type get_header_num(char *s)
{
    char* end = s + strlen(s);

    header_line_type i = set_line_type(s, end);     /* Sets g_msg to lower-cased header name */

    if (i <= SOME_LINE && i != CUSTOM_LINE) {
        if (!empty(g_htype[CUSTOM_LINE].name))
            free(g_htype[CUSTOM_LINE].name);
        g_htype[CUSTOM_LINE].name = savestr(g_msg);
        g_htype[CUSTOM_LINE].length = end - s;
        g_htype[CUSTOM_LINE].flags = g_htype[i].flags;
        g_htype[CUSTOM_LINE].minpos = -1;
        g_htype[CUSTOM_LINE].maxpos = 0;
        for (char *bp = g_headbuf; *bp; bp = end) {
            if (!(end = strchr(bp,'\n')) || end == bp)
                break;
            char ch = *++end;
            *end = '\0';
            s = strchr(bp,':');
            *end = ch;
            if (!s || (i = set_line_type(bp,s)) != CUSTOM_LINE)
                continue;
            g_htype[CUSTOM_LINE].minpos = bp - g_headbuf;
            while (is_hor_space(*end)) {
                if (!(end = strchr(end, '\n'))) {
                    end = bp + strlen(bp);
                    break;
                }
                end++;
            }
            g_htype[CUSTOM_LINE].maxpos = end - g_headbuf;
            break;
        }
        i = CUSTOM_LINE;
    }
    return i;
}

void start_header(ART_NUM artnum)
{
#ifdef DEBUG
    if (debug & DEB_HEADER)
        dumpheader("start_header\n");
#endif
    for (int i = 0; i < HEAD_LAST; i++) {
        g_htype[i].minpos = -1;
        g_htype[i].maxpos = 0;
    }
    g_in_header = SOME_LINE;
    s_first_one = false;
    g_parsed_art = artnum;
    s_parsed_artp = article_ptr(artnum);
}

void end_header_line()
{
    if (s_first_one) {          /* did we just pass 1st occurance? */
        s_first_one = false;
        /* remember where line left off */
        g_htype[g_in_header].maxpos = g_artpos;
        if (g_htype[g_in_header].flags & HT_CACHED) {
            if (!get_cached_line(s_parsed_artp, g_in_header, true)) {
                int start = g_htype[g_in_header].minpos
                          + g_htype[g_in_header].length + 1;
                while (is_hor_space(g_headbuf[start]))
                    start++;
                MEM_SIZE size = g_artpos - start + 1 - 1;   /* pre-strip newline */
                if (g_in_header == SUBJ_LINE)
                    set_subj_line(s_parsed_artp,g_headbuf+start,size-1);
                else {
                    char* s = safemalloc(size);
                    safecpy(s,g_headbuf+start,size);
                    set_cached_line(s_parsed_artp,g_in_header,s);
                }
            }
        }
    }
}

bool parseline(char *art_buf, int newhide, int oldhide)
{
    if (is_hor_space(*art_buf)) /* continuation line? */
        return oldhide;

    end_header_line();
    char *s = strchr(art_buf, ':');
    if (s == nullptr) { /* is it the end of the header? */
            /* Did NNTP ship us a mal-formed header line? */
            if (s_reading_nntp_header && *art_buf && *art_buf != '\n') {
                g_in_header = SOME_LINE;
                return newhide;
            }
            g_in_header = PAST_HEADER;
    }
    else {              /* it is a new header line */
            g_in_header = set_line_type(art_buf,s);
            s_first_one = (g_htype[g_in_header].minpos < 0);
            if (s_first_one) {
                g_htype[g_in_header].minpos = g_artpos;
                if (g_in_header == DATE_LINE) {
                    if (!s_parsed_artp->date)
                        s_parsed_artp->date = parsedate(art_buf+6);
                }
            }
#ifdef DEBUG
            if (debug & DEB_HEADER)
                dumpheader(art_buf);
#endif
            if (g_htype[g_in_header].flags & HT_HIDE)
                return newhide;
    }
    return false;                       /* don't hide this line */
}

void end_header()
{
    ARTICLE* ap = s_parsed_artp;

    end_header_line();
    g_in_header = PAST_HEADER;  /* just to be sure */

    if (!ap->subj)
        set_subj_line(ap,"<NONE>",6);

    if (s_reading_nntp_header) {
        s_reading_nntp_header = false;
        g_htype[PAST_HEADER].minpos = g_artpos + 1;     /* nntp_body will fix this */
    }
    else
        g_htype[PAST_HEADER].minpos = tellart();

    /* If there's no References: line, then the In-Reply-To: line may give us
    ** more information.
    */
    if (g_threaded_group
     && (!(ap->flags & AF_THREADED) || g_htype[INREPLY_LINE].minpos >= 0)) {
        if (valid_article(ap)) {
            ARTICLE* artp_hold = g_artp;
            char* references = fetchlines(g_parsed_art, REFS_LINE);
            char* inreply = fetchlines(g_parsed_art, INREPLY_LINE);
            int reflen = strlen(references) + 1;
            growstr(&references, &reflen, reflen + strlen(inreply) + 1);
            safecat(references, inreply, reflen);
            thread_article(ap, references);
            free(inreply);
            free(references);
            g_artp = artp_hold;
            check_poster(ap);
        }
    } else if (!(ap->flags & AF_CACHED)) {
        cache_article(ap);
        check_poster(ap);
    }
}

/* read the header into memory and parse it if we haven't already */

bool parseheader(ART_NUM artnum)
{
    int len;
    bool had_nl = true;
    int found_nl;

    if (g_parsed_art == artnum)
        return true;
    if (artnum > g_lastart)
        return false;
    spin(20);
    if (g_datasrc->flags & DF_REMOTE) {
        char *s = nntp_artname(artnum, false);
        if (s) {
            if (!artopen(artnum,(ART_POS)0))
                return false;
        }
        else if (nntp_header(artnum) <= 0) {
            uncache_article(article_ptr(artnum),false);
            return false;
        }
        else
            s_reading_nntp_header = true;
    }
    else if (!artopen(artnum,(ART_POS)0))
        return false;

    start_header(artnum);
    g_artpos = 0;
    char *bp = g_headbuf;
    while (g_in_header) {
        if (s_headbuf_size < g_artpos + LBUFLEN) {
            len = bp - g_headbuf;
            s_headbuf_size += LBUFLEN * 4;
            g_headbuf = saferealloc(g_headbuf,s_headbuf_size);
            bp = g_headbuf + len;
        }
        if (s_reading_nntp_header) {
            found_nl = nntp_gets(bp,LBUFLEN) == NGSR_FULL_LINE;
            if (found_nl < 0)
                strcpy(bp,"."); /*$$*/
            if (had_nl && *bp == '.') {
                if (!bp[1]) {
                    *bp++ = '\n';       /* tag the end with an empty line */
                    break;
                }
                strcpy(bp,bp+1);
            }
            len = strlen(bp);
            if (found_nl)
                bp[len++] = '\n';
            bp[len] = '\0';
        }
        else
        {
            if (readart(bp,LBUFLEN) == nullptr)
                break;
            len = strlen(bp);
            found_nl = (bp[len-1] == '\n');
        }
        if (had_nl)
            parseline(bp,false,false);
        had_nl = found_nl;
        g_artpos += len;
        bp += len;
    }
    *bp = '\0';
    end_header();
    return true;
}

/* get a header line from an article */

/* article to get line from */
/* type of line desired */
char *fetchlines(ART_NUM artnum, header_line_type which_line)
{
    char* s;

    /* Only return a cached line if it isn't the current article */
    if (g_parsed_art != artnum) {
        /* If the line is not in the cache, this will parse the header */
        s = fetchcache(artnum,which_line, FILL_CACHE);
        if (s)
            return savestr(s);
    }
    ART_POS firstpos = g_htype[which_line].minpos;
    if (firstpos < 0)
        return savestr("");

    firstpos += g_htype[which_line].length + 1;
    ART_POS lastpos = g_htype[which_line].maxpos;
    int size = lastpos - firstpos;
    char *t = g_headbuf + firstpos;
    while (is_hor_space(*t))
    {
        t++;
        size--;
    }
#ifdef DEBUG
    if (debug && (size < 1 || size > 1000)) {
        printf("Firstpos = %ld, lastpos = %ld\n",(long)firstpos,(long)lastpos);
        fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    s = safemalloc((MEM_SIZE)size);
    safecpy(s,t,size);
    return s;
}

/* (strn) like fetchlines, but for memory pools */
/* ART_NUM artnum   article to get line from */
/* int which_line   type of line desired */
/* int pool         which memory pool to use */
char *mp_fetchlines(ART_NUM artnum, header_line_type which_line, memory_pool pool)
{
    char* s;

    /* Only return a cached line if it isn't the current article */
    if (g_parsed_art != artnum) {
        /* If the line is not in the cache, this will parse the header */
        s = fetchcache(artnum,which_line, FILL_CACHE);
        if (s)
            return mp_savestr(s,pool);
    }
    ART_POS firstpos = g_htype[which_line].minpos;
    if (firstpos < 0)
        return mp_savestr("",pool);

    firstpos += g_htype[which_line].length + 1;
    ART_POS lastpos = g_htype[which_line].maxpos;
    int size = lastpos - firstpos;
    char *t = g_headbuf + firstpos;
    while (is_hor_space(*t))
    {
        t++;
        size--;
    }
#ifdef DEBUG
    if (debug && (size < 1 || size > 1000)) {
        printf("Firstpos = %ld, lastpos = %ld\n",(long)firstpos,(long)lastpos);
        fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    s = mp_malloc(size,pool);
    safecpy(s,t,size);
    return s;
}

static int nntp_xhdr(header_line_type which_line, ART_NUM artnum)
{
    sprintf(g_ser_line,"XHDR %s %ld",g_htype[which_line].name,artnum);
    return nntp_command(g_ser_line);
}

static int nntp_xhdr(header_line_type which_line, ART_NUM artnum, ART_NUM lastnum)
{
    sprintf(g_ser_line, "XHDR %s %ld-%ld", g_htype[which_line].name, artnum, lastnum);
    return nntp_command(g_ser_line);
}

/* prefetch a header line from one or more articles */

// ART_NUM artnum   article to get line from */
// int which_line   type of line desired */
// bool copy    do you want it savestr()ed? */
char *prefetchlines(ART_NUM artnum, header_line_type which_line, bool copy)
{
    char* s;
    char* t;
    ART_POS firstpos;

    if ((g_datasrc->flags & DF_REMOTE) && g_parsed_art != artnum) {
        ARTICLE*ap;
        int     size;
        ART_NUM num, lastnum;
        bool    hasxhdr = true;

        s = fetchcache(artnum,which_line, DONT_FILL_CACHE);
        if (s) {
            if (copy)
                s = savestr(s);
            return s;
        }

        spin(20);
        if (copy)
        {
            size = LBUFLEN;
            s = safemalloc((MEM_SIZE) size);
        }
        else {
            s = g_cmd_buf;
            size = sizeof g_cmd_buf;
        }
        *s = '\0';
        ART_NUM priornum = artnum - 1;
        bool    cached = (g_htype[which_line].flags & HT_CACHED);
        int     status;
        if (cached != 0)
        {
            lastnum = artnum + PREFETCH_SIZE - 1;
            if (lastnum > g_lastart)
                lastnum = g_lastart;
            status = nntp_xhdr(which_line, artnum, lastnum);
        } else {
            lastnum = artnum;
            status = nntp_xhdr(which_line, artnum);
        }
        if (status <= 0)
            finalize(1); /*$$*/
        if (nntp_check() > 0) {
            char* last_buf = g_ser_line;
            MEM_SIZE last_buflen = sizeof g_ser_line;
            for (;;) {
                char *line = nntp_get_a_line(last_buf, last_buflen, last_buf!=g_ser_line);
# ifdef DEBUG
                if (debug & DEB_NNTP)
                    printf("<%s", line? line : "<EOF>");
# endif
                if (nntp_at_list_end(line))
                    break;
                last_buf = line;
                last_buflen = g_buflen_last_line_got;
                t = strchr(line, '\r');
                if (t != nullptr)
                    *t = '\0';
                if (!(t = strchr(line, ' ')))
                    continue;
                t++;
                num = atol(line);
                if (num < artnum || num > lastnum)
                    continue;
                if (!(g_datasrc->flags & DF_XHDR_BROKEN)) {
                    while ((priornum = article_next(priornum)) < num)
                        uncache_article(article_ptr(priornum),false);
                }
                ap = article_find(num);
                if (which_line == SUBJ_LINE)
                    set_subj_line(ap, t, strlen(t));
                else if (cached)
                    set_cached_line(ap, which_line, savestr(t));
                if (num == artnum)
                    safecat(s,t,size);
            }
            if (last_buf != g_ser_line)
                free(last_buf);
        } else {
            hasxhdr = false;
            lastnum = artnum;
            if (!parseheader(artnum)) {
                fprintf(stderr,"\nBad NNTP response.\n");
                finalize(1);
            }
            s = fetchlines(artnum,which_line);
        }
        if (hasxhdr && !(g_datasrc->flags & DF_XHDR_BROKEN)) {
            for (priornum = article_first(priornum); priornum < lastnum; priornum = article_next(priornum))
                uncache_article(article_ptr(priornum),false);
        }
        if (copy)
            s = saferealloc(s, (MEM_SIZE)strlen(s)+1);
        return s;
    }

    /* Only return a cached line if it isn't the current article */
    s = nullptr;
    if (g_parsed_art != artnum)
        s = fetchcache(artnum,which_line, FILL_CACHE);
    if (g_parsed_art == artnum && (firstpos = g_htype[which_line].minpos) < 0)
        s = "";
    if (s) {
        if (copy)
            s = savestr(s);
        return s;
    }

    firstpos += g_htype[which_line].length + 1;
    ART_POS lastpos = g_htype[which_line].maxpos;
    int size = lastpos - firstpos;
    t = g_headbuf + firstpos;
    while (is_hor_space(*t))
    {
        t++;
        size--;
    }
    if (copy)
        s = safemalloc((MEM_SIZE)size);
    else {                              /* hope this is okay--we're */
        s = g_cmd_buf;                  /* really scraping for space here */
        if (size > sizeof g_cmd_buf)
            size = sizeof g_cmd_buf;
    }
    safecpy(s,t,size);
    return s;
}
