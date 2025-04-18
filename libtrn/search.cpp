/* search.c
 */

/* string search routines */

/*              Copyright (c) 1981,1980 James Gosling           */

/* Modified Aug. 12, 1981 by Tom London to include regular expressions
   as in ed.  RE stuff hacked over by jag to correct a few major problems,
   mainly dealing with searching within the buffer rather than copying
   each line to a separate array.  Newlines can now appear in RE's */

/* Ripped to shreds and glued back together to make a search package,
 * July 6, 1984, by Larry Wall. (If it doesn't work, it's probably my fault.)
 * Changes include:
 *      Buffer, window, and mlisp stuff gone.
 *      Translation tables reduced to 1 table.
 *      Expression buffer is now dynamically allocated.
 *      Character classes now implemented with a bitmap.
 * Modified by David Canzi, Apr 1997:
 *      Check bounds on alternatives array.
 *      Correct spurious matching, eg. /: re: .*\bfoo/ matched ": re: bar".
 */

#include "config/common.h"
#include "trn/search.h"

#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

enum
{
    BMAPSIZ = (127 / BITS_PER_BYTE + 1)
};

enum
{
    META_NULL = 64 /* Bit is set in a meta-character defn to
                          indicate that the metacharacter can match
                          a null string.  advance() uses this. */
};

/* meta characters in the "compiled" form of a regular expression */
#define CBRA (2 | META_NULL)     /* \( -- begin bracket */
#define CCHR 4                   /* a vanilla character */
#define CDOT 6                   /* . -- match anything except a newline */
#define CCL 8                    /* [...] -- character class */
#define NCCL 10                  /* [^...] -- negated character class */
#define CDOL (12 | META_NULL)    /* $ -- matches the end of a line */
#define CEND (14 | META_NULL)    /* The end of the pattern */
#define CKET (16 | META_NULL)    /* \) -- close bracket */
#define CBACK (18 | META_NULL)   /* \N -- backreference to the Nth bracketed string */
#define CIRC (20 | META_NULL)    /* ^ matches the beginning of a line */
#define WORD 32                  /* matches word character \w */
#define NWORD 34                 /* matches non-word characer \W */
#define WBOUND (36 | META_NULL)  /* matches word boundary \b */
#define NWBOUND (38 | META_NULL) /* matches non-(word boundary) \B */

enum
{
    STAR = 01, /* * -- Kleene star, repeats the previous
                          REas many times as possible; the value
                          ORs with the other operator types */

    ASCSIZ = 256
};
using TranslationTable = Uchar[ASCSIZ];

static TranslationTable s_trans{};
static bool             s_folding{};
static int              s_err{};
static const char      *s_first_character{};

void search_init()
{
    for (int i = 0; i < ASCSIZ; i++)
    {
        s_trans[i] = i;
    }
}

void init_compex(CompiledRegex *compex)
{
    /* the following must start off zeroed */

    compex->eblen = 0;
    compex->brastr = nullptr;
}

void free_compex(CompiledRegex *compex)
{
    if (compex->eblen)
    {
        std::free(compex->expbuf);
        compex->eblen = 0;
    }
    if (compex->brastr)
    {
        std::free(compex->brastr);
        compex->brastr = nullptr;
    }
}

static char *s_gbr_str{};
static int   s_gbr_siz{};

const char *getbracket(CompiledRegex *compex, int n)
{
    int length = compex->braelist[n] - compex->braslist[n];

    if (!compex->nbra)
    {
        return nullptr;
    }
    if (n > compex->nbra || !compex->braelist[n] || length < 0)
    {
        return "";
    }
    grow_str(&s_gbr_str, &s_gbr_siz, length+1);
    safe_copy(s_gbr_str, compex->braslist[n], length+1);
    return s_gbr_str;
}

void case_fold(bool which)
{
    if (which != s_folding)
    {
        if (which)
        {
            for (int i = 'A'; i <= 'Z'; i++)
            {
                s_trans[i] = std::tolower(i);
            }
        }
        else
        {
            for (int i = 'A'; i <= 'Z'; i++)
            {
                s_trans[i] = i;
            }
        }
        s_folding = which;
    }
}

/* Compile the given regular expression into a [secret] internal format */

char *compile(CompiledRegex *compex, const char *strp, bool RE, bool fold)
{
    char  bracket[NBRA];
    char**alt = compex->alternatives;
    char* retmes = "Badly formed search string";

    case_fold(compex->do_folding = fold);
    if (!compex->eblen)
    {
        compex->expbuf = safe_malloc(84);
        compex->eblen = 80;
    }
    char *ep = compex->expbuf; /* point at expression buffer */
    *alt++ = ep;               /* first alternative starts here */
    char *bracketp = bracket;  /* first bracket goes here */
    if (*strp == 0)            /* nothing to compile? */
    {
        if (*ep == 0)          /* nothing there yet? */
        {
            return "Null search string";
        }
        return nullptr;                 /* just keep old expression */
    }
    compex->nbra = 0;                   /* no brackets yet */
    char *lastep = nullptr;
    while (true)
    {
        if (ep + 4 - compex->expbuf >= compex->eblen)
        {
            ep = grow_eb(compex, ep, alt);
        }
        int c = *strp++;               /* fetch next char of pattern */
        if (c == 0)                    /* end of pattern? */
        {
            if (bracketp != bracket)   /* balanced brackets? */
            {
                retmes = "Unbalanced parens";
                goto cerror;
            }
            *ep++ = CEND;               /* terminate expression */
            *alt++ = nullptr;           /* terminal alternative list */
            return nullptr;             /* return success */
        }
        if (c != '*')
        {
            lastep = ep;
        }
        if (!RE)                        /* just a normal search string? */
        {
            *ep++ = CCHR;               /* everything is a normal char */
            *ep++ = c;
        }
        else                            /* it is a regular expression */
        {
            switch (c)
            {
            case '\\':              /* meta something */
                switch (c = *strp++)
                {
                case '(':
                    if (compex->nbra >= NBRA)
                    {
                        retmes = "Too many parens";
                        goto cerror;
                    }
                    *bracketp++ = ++compex->nbra;
                    *ep++ = CBRA;
                    *ep++ = compex->nbra;
                    break;

                case '|':
                    if (bracketp > bracket)
                    {
                        retmes = "No \\| in parens";        /* Alas! */
                        goto cerror;
                    }
                    *ep++ = CEND;
                    *alt++ = ep;
                    if (alt > compex->alternatives + NALTS)
                    {
                            retmes = "Too many alternatives in reg ex";
                            goto cerror;
                    }
                    break;

                case ')':
                    if (bracketp <= bracket)
                    {
                        retmes = "Unmatched right paren";
                        goto cerror;
                    }
                    *ep++ = CKET;
                    *ep++ = *--bracketp;
                    break;

                case 'w':
                    *ep++ = WORD;
                    break;

                case 'W':
                    *ep++ = NWORD;
                    break;

                case 'b':
                    *ep++ = WBOUND;
                    break;

                case 'B':
                    *ep++ = NWBOUND;
                    break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    *ep++ = CBACK;
                    *ep++ = c - '0';
                    break;

                default:
                    *ep++ = CCHR;
                    if (c == '\0')
                    {
                        goto cerror;
                    }
                    *ep++ = c;
                    break;
                }
                break;

            case '.':
                *ep++ = CDOT;
                continue;

            case '*':
                if (lastep == nullptr || *lastep == CBRA || *lastep == CKET //
                    || *lastep == CIRC                                      //
                    || (*lastep & STAR) || *lastep > NWORD)
                {
                    goto defchar;
                }
                *lastep |= STAR;
                continue;

            case '^':
                if (ep != compex->expbuf && ep[-1] != CEND)
                {
                    goto defchar;
                }
                *ep++ = CIRC;
                continue;

            case '$':
                if (*strp != 0 && (*strp != '\\' || strp[1] != '|'))
                {
                    goto defchar;
                }
                *ep++ = CDOL;
                continue;

            case '[':               /* character class */
            {
                if (ep - compex->expbuf >= compex->eblen - BMAPSIZ)
                {
                    ep = grow_eb(compex, ep, alt); /* reserve bitmap */
                }

                for (int i = BMAPSIZ; i; --i)
                {
                    ep[i] = 0;
                }

                c = *strp++;
                if (c == '^')
                {
                    c = *strp++;
                    *ep++ = NCCL;   /* negated */
                }
                else
                {
                    *ep++ = CCL;    /* normal */
                }

                int i = 0;          /* remember oldchar */
                do
                {
                    if (c == '\0')
                    {
                        retmes = "Missing ]";
                        goto cerror;
                    }
                    if (*strp == '-' && *(++strp) != ']' && *strp)
                    {
                        i = *strp++;
                    }
                    else
                    {
                        i = c;
                    }
                    while (c <= i)
                    {
                        ep[c / BITS_PER_BYTE] |= 1 << (c % BITS_PER_BYTE);
                        if (fold && std::isalpha(c))
                        {
                            ep[(c ^ 32) / BITS_PER_BYTE] |=
                                1 << ((c ^ 32) % BITS_PER_BYTE);
                                    /* set the other bit too */
                        }
                        c++;
                    }
                } while ((c = *strp++) != ']');
                ep += BMAPSIZ;
                continue;
            }

defchar:
            default:
                *ep++ = CCHR;
                *ep++ = c;
            }
        }
    }
cerror:
    compex->expbuf[0] = 0;
    compex->nbra = 0;
    return retmes;
}

char *grow_eb(CompiledRegex *compex, char *epp, char **alt)
{
    char* oldbuf = compex->expbuf;
    char** altlist = compex->alternatives;

    compex->eblen += 80;
    compex->expbuf = safe_realloc(compex->expbuf, (MemorySize)compex->eblen + 4);
    if (compex->expbuf != oldbuf)       /* realloc can change expbuf! */
    {
        epp += compex->expbuf - oldbuf;
        while (altlist != alt)
        {
            *altlist++ += compex->expbuf - oldbuf;
        }
    }
    return epp;
}

const char *execute(CompiledRegex *compex, const char *addr)
{
    const char* p1 = addr;
    Uchar* trt = s_trans;

    if (addr == nullptr || compex->expbuf == nullptr)
    {
        return nullptr;
    }
    if (compex->nbra)                   /* any brackets? */
    {
        for (int i = 0; i <= compex->nbra; i++)
        {
            compex->braslist[i] = nullptr;
            compex->braelist[i] = nullptr;
        }
        if (compex->brastr)
        {
            std::free(compex->brastr);
        }
        compex->brastr = save_str(p1);   /* in case p1 is not static */
        p1 = compex->brastr;            /* ! */
    }
    case_fold(compex->do_folding);      /* make sure table is correct */
    s_first_character = p1;             /* for ^ tests */
    if (compex->expbuf[0] == CCHR && !compex->alternatives[1])
    {
        int c = trt[*(Uchar*)(compex->expbuf + 1)]; /* fast check for first char */
        do
        {
            if (trt[*(Uchar*)p1] == c && advance(compex, p1, compex->expbuf))
            {
                return p1;
            }
            p1++;
        } while (*p1 && !s_err);
        if (s_err)
        {
            s_err = 0;
        }
        return nullptr;
    }
    do                                  /* regular algorithm */
    {
        char** alt = compex->alternatives;
        while (*alt)
        {
            if (advance(compex, p1, *alt++))
            {
                return p1;
            }
        }
        p1++;
    } while (*p1 && !s_err);
    if (s_err)
    {
        s_err = 0;
    }
    return nullptr;
}

/* advance the match of the regular expression starting at ep along the
   string lp, simulates an NDFSA */
bool advance(CompiledRegex *compex, const char *lp, const char *ep)
{
    const char* curlp;
    Uchar* trt = s_trans;
    int i;

    while (*lp || (*ep & (STAR | META_NULL)))
    {
        switch (*ep++)
        {
        case CCHR:
            if (trt[*(Uchar*)ep++] != trt[*(Uchar*)lp])
            {
                return false;
            }
            lp++;
            continue;

        case CDOT:
            if (*lp == '\n')
            {
                return false;
            }
            lp++;
            continue;

        case CDOL:
            if (!*lp || *lp == '\n')
            {
                continue;
            }
            return false;

        case CIRC:
            if (lp == s_first_character || lp[-1]=='\n')
            {
                continue;
            }
            return false;

        case WORD:
            if (std::isalnum(*lp))
            {
                lp++;
                continue;
            }
            return false;

        case NWORD:
            if (!std::isalnum(*lp))
            {
                lp++;
                continue;
            }
            return false;

        case WBOUND:
            if ((lp == s_first_character || !std::isalnum(lp[-1])) !=
                    (!*lp || !std::isalnum(*lp)) )
            {
                continue;
            }
            return false;

        case NWBOUND:
            if ((lp == s_first_character || !std::isalnum(lp[-1])) ==
                    (!*lp || !std::isalnum(*lp)))
            {
                continue;
            }
            return false;

        case CEND:
            return true;

        case CCL:
            if (cclass(ep, *lp, 1))
            {
                ep += BMAPSIZ;
                lp++;
                continue;
            }
            return false;

        case NCCL:
            if (cclass(ep, *lp, 0))
            {
                ep += BMAPSIZ;
                lp++;
                continue;
            }
            return false;

        case CBRA:
            compex->braslist[(unsigned char)*ep++] = lp;
            continue;

        case CKET:
            i = *ep++;
            compex->braelist[i] = lp;
            compex->braelist[0] = lp;
            compex->braslist[0] = compex->braslist[i];
            continue;

        case CBACK:
            i = *ep++;
            if (compex->braelist[i] == nullptr)
            {
                std::fputs("bad braces\n",stdout);
                s_err = true;
                return false;
            }
            if (backref(compex, i, lp))
            {
                lp += compex->braelist[i] - compex->braslist[i];
                continue;
            }
            return false;

        case CBACK | STAR:
            i = *ep++;
            if (compex->braelist[i] == nullptr)
            {
                std::fputs("bad braces\n",stdout);
                s_err = true;
                return false;
            }
            curlp = lp;
            while (backref(compex, i, lp))
            {
                lp += compex->braelist[i] - compex->braslist[i];
            }
            while (lp >= curlp)
            {
                if (advance(compex, lp, ep))
                {
                    return true;
                }
                lp -= compex->braelist[i] - compex->braslist[i];
            }
            continue;

        case CDOT | STAR:
            curlp = lp;
            while (*lp++ && lp[-1] != '\n')
            {
            }
            goto star;

        case WORD | STAR:
            curlp = lp;
            while (*lp++ && std::isalnum(lp[-1]))
            {
            }
            goto star;

        case NWORD | STAR:
            curlp = lp;
            while (*lp++ && !std::isalnum(lp[-1]))
            {
            }
            goto star;

        case CCHR | STAR:
            curlp = lp;
            while (*lp++ && trt[*(Uchar*)(lp-1)] == trt[*(Uchar*)ep])
            {
            }
            ep++;
            goto star;

        case CCL | STAR:
        case NCCL | STAR:
            curlp = lp;
            while (*lp++ && cclass(ep, lp[-1], ep[-1] == (CCL | STAR)))
            {
            }
            ep += BMAPSIZ;
            goto star;

star:
            do
            {
                lp--;
                if (advance(compex, lp, ep))
                {
                    return true;
                }
            } while (lp > curlp);
            return false;

        default:
            std::fputs("Badly compiled pattern\n",stdout);
            s_err = true;
            return false;
        }
    }
    return false;
}

bool backref(CompiledRegex *compex, int i, const char *lp)
{
    const char *bp = compex->braslist[i];
    while (*lp && *bp == *lp)
    {
        bp++;
        lp++;
        if (bp >= compex->braelist[i])
        {
            return true;
        }
    }
    return false;
}

bool cclass(const char *set, int c, int af)
{
    c &= 0177;
    if (set[c >> 3] & 1 << (c & 7))
    {
        return af;
    }
    return !af;
}
