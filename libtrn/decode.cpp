/* decode.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/decode.h"

#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artstate.h"
#include "trn/change_dir.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/intrp.h"
#include "trn/mime.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/util.h"
#include "util/util2.h"
#include "trn/uudecode.h"

#include <algorithm>
#include <cstring>

#ifdef MSDOS
#define GOODCHARS                \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefghijklmnopqrstuvwxyz" \
    "0123456789-_^#%"
#else
#define BADCHARS "!$&*()|\'\";<>[]{}?/`\\ \t"
#endif

char *g_decode_filename{};

static bool bad_filename(const char *filename);

void decode_init()
{
}

char *decode_fix_fname(const char *s)
{
    char* t;
#ifdef MSDOS
    int dotcount = 0;
#endif

    if (!s)
    {
        s = "unknown";
    }

    safefree(g_decode_filename);
    g_decode_filename = safemalloc(std::strlen(s) + 2);

    /*TODO: we need to eliminate any "../"s from the string */
    while (*s == '/' || *s == '~')
    {
        s++;
    }
    for (t = g_decode_filename; *s; s++)
    {
#ifdef MSDOS
        /* TODO: we should also handle backslashes here */
        if (*s == '.' && (t == g_decode_filename || dotcount++))
        {
            continue;
        }
#endif
        if (std::isprint(*s)
#ifdef GOODCHARS
         && std::strchr(GOODCHARS, *s)
#else
         && !std::strchr(BADCHARS, *s)
#endif
        )
        {
            *t++ = *s;
        }
    }
    *t = '\0';
    if (t == g_decode_filename || bad_filename(g_decode_filename))
    {
        *t++ = 'x';
        *t = '\0';
    }
    return g_decode_filename;
}

/* Returns nonzero if "filename" is a bad choice */
static bool bad_filename(const char *filename)
{
    int len = std::strlen(filename);
#ifdef MSDOS
    if (len == 3)
    {
        if (string_case_equal(filename, "aux") || string_case_equal(filename, "con") //
            || string_case_equal(filename, "nul") || string_case_equal(filename, "prn"))
        {
            return true;
        }
    }
    else if (len == 4)
    {
        if (string_case_equal(filename, "com1") || string_case_equal(filename, "com2")    //
            || string_case_equal(filename, "com3") || string_case_equal(filename, "com4") //
            || string_case_equal(filename, "lpt1") || string_case_equal(filename, "lpt2") //
            || string_case_equal(filename, "lpt3"))
        {
            return true;
        }
    }
#else
    if (len <= 2)
    {
        if (*filename == '.' && (*filename == '\0' || *filename == '.'))
        {
            return true;
        }
    }
#endif
    return false;
}

/* Parse the subject looking for filename and part number information. */
char *decode_subject(ART_NUM artnum, int *partp, int *totalp)
{
    static char* subject = nullptr;
    char* filename;
    char* t;
    int part = -1;
    int total = 0;
    int hasdot = 0;

    *partp = part;
    *totalp = total;
    safefree(subject);
    subject = fetchsubj(artnum,true);
    if (!*subject)
    {
        return nullptr;
    }

    /* Skip leading whitespace and other garbage */
    char *s = subject;
    while (is_hor_space(*s) || *s == '-')
    {
        s++;
    }
    if (string_case_equal(s, "repost", 6))
    {
        for (s += 6; is_hor_space(*s) || *s == ':' || *s == '-'; s++)
        {
        }
    }

    while (string_case_equal(s, "re:", 3))
    {
        s = skip_space(s + 3);
    }

    /* Get filename */

    /* Grab the first filename-like string.  Explicitly ignore strings with
     * prefix "v<digit>" ending in ":", since that is a popular volume/issue
     * representation syntax
     */
    char *end = s + std::strlen(s);
    do
    {
        while (*s && !std::isalnum(*s) && *s != '_')
        {
            s++;
        }
        filename = s;
        t = s;
        while (std::isalnum(*s) || *s == '-' || *s == '+' || *s == '&' //
               || *s == '_' || *s == '.')
        {
            if (*s++ == '.')
            {
                hasdot = 1;
            }
        }
        if (!*s || *s == '\n')
        {
            return nullptr;
        }
    } while (t == s || (t[0] == 'v' && std::isdigit(t[1]) && *s == ':'));
    *s++ = '\0';

    /* Try looking for a filename with a "." in it later in the subject line.
     * Exclude <digit>.<digit>, since that is usually a version number.
     */
    if (!hasdot)
    {
        while (*(t = s) != '\0' && *s != '\n')
        {
            t = skip_space(t);
            for (s = t; std::isalnum(*s) || *s == '-' || *s == '+' || *s == '&' || *s == '_' || *s == '.'; s++)
            {
                if (*s == '.' && (!std::isdigit(s[-1]) || !std::isdigit(s[1])))
                {
                    hasdot = 1;
                }
            }
            if (hasdot && s > t)
            {
                filename = t;
                *s++ = '\0';
                break;
            }
            while (*s && *s != '\n' && !std::isalnum(*s))
            {
                s++;
            }
        }
        s = filename + std::strlen(filename) + 1;
    }

    if (s >= end)
    {
        return nullptr;
    }

    /* Get part number */
    while (*s && *s != '\n')
    {
        /* skip over versioning */
        if (*s == 'v' && std::isdigit(s[1]))
        {
            s++;
            s = skip_digits(s);
        }
        /* look for "1/6" or "1 / 6" or "1 of 6" or "1-of-6" or "1o6" */
        if (std::isdigit(*s)                                   //
            && (s[1] == '/'                                    //
                || (s[1] == ' ' && s[2] == '/')                //
                || (s[1] == ' ' && s[2] == 'o' && s[3] == 'f') //
                || (s[1] == '-' && s[2] == 'o' && s[3] == 'f') //
                || (s[1] == 'o' && std::isdigit(s[2]))))
        {
            for (t = s; std::isdigit(t[-1]); t--)
            {
            }
            part = std::atoi(t);
            while (*++s != '\0' && *s != '\n' && !std::isdigit(*s))
            {
            }
            total = std::isdigit(*s)? std::atoi(s) : 0;
            s = skip_digits(s);
            /* We don't break here because we want the last item on the line */
        }

        /* look for "6 parts" or "part 1" */
        if (string_case_equal("part", s, 4))
        {
            if (s[4] == 's')
            {
                for (t = s; t >= subject && !std::isdigit(*t); t--)
                {
                }
                if (t > subject)
                {
                    while (t > subject && std::isdigit(t[-1]))
                    {
                        t--;
                    }
                    total = std::atoi(t);
                }
            }
            else
            {
                while (*s && *s != '\n' && !std::isdigit(*s))
                {
                    s++;
                }
                if (std::isdigit(*s))
                {
                    part = std::atoi(s);
                }
                s--;
            }
        }
        if (*s)
        {
            s++;
        }
    }

    if (total == 0 || part == -1 || part > total)
    {
        return nullptr;
    }
    *partp = part;
    *totalp = total;
    return filename;
}

/*
 * Handle a piece of a split file.
 */
bool decode_piece(MIMECAP_ENTRY *mcp, char *first_line)
{
    *g_msg = '\0';

    int part = g_mime_section->part;
    int total = g_mime_section->total;
    if (!total && g_is_mime)
    {
        total = 1;
        part = 1;
    }

    char* dir;
    char *filename = decode_fix_fname(g_mime_section->filename);
    if (mcp || total != 1 || part != 1)
    {
        /* Create directory to store parts and copy this part there. */
        dir = decode_mkdir(filename);
        if (!dir)
        {
            std::strcpy(g_msg, "Failed.");
            return false;
        }
    }
    else
    {
        dir = nullptr;
    }

    if (mcp)
    {
        if (change_dir(dir))
        {
            std::printf(g_nocd,dir);
            sig_catcher(0);
        }
    }

    std::FILE* fp;
    if (total != 1 || part != 1)
    {
        std::sprintf(g_buf, "Saving part %d ", part);
        if (total)
        {
            std::sprintf(g_buf + std::strlen(g_buf), "of %d ", total);
        }
        std::strcat(g_buf, filename);
        std::fputs(g_buf,stdout);
        if (g_nowait_fork)
        {
            std::fflush(stdout);
        }
        else
        {
            newline();
        }

        std::sprintf(g_buf, "%s%d", dir, part);
        fp = std::fopen(g_buf, "w");
        if (!fp)
        {
            std::strcpy(g_msg,"Failed.");
            return false;
        }
        while (readart(g_art_line, sizeof g_art_line))
        {
            if (mime_EndOfSection(g_art_line))
            {
                break;
            }
            std::fputs(g_art_line,fp);
            if (total == 0 && *g_art_line == 'e' && g_art_line[1] == 'n' //
                && g_art_line[2] == 'd' && std::isspace(g_art_line[3]))
            {
                /* This is the last part. Remember the fact */
                total = part;
                std::sprintf(g_buf, "%sCT", dir);
                if (std::FILE *total_fp = std::fopen(g_buf, "w"))
                {
                    std::fprintf(total_fp, "%d\n", total);
                    std::fclose(total_fp);
                }
            }
        }
        std::fclose(fp);

        /* Retrieve any previously saved number of the last part */
        if (total == 0)
        {
            std::sprintf(g_buf, "%sCT", dir);
            fp = std::fopen(g_buf, "r");
            if (fp != nullptr)
            {
                if (std::fgets(g_buf, sizeof g_buf, fp))
                {
                    total = std::atoi(g_buf);
                    total = std::max(total, 0);
                }
                std::fclose(fp);
            }
            if (total == 0)
            {
                return true;
            }
        }

        /* Check to see if we have all parts.  Start from the highest numbers
         * as we are more likely not to have them.
         */
        for (part = total; part; part--)
        {
            std::sprintf(g_buf, "%s%d", dir, part);
            fp = std::fopen(g_buf, "r");
            if (!fp)
            {
                return true;
            }
            if (part != 1)
            {
                std::fclose(fp);
            }
        }
    }
    else
    {
        fp = nullptr;
        total = 1;
    }

    if (g_mime_section->type == MESSAGE_MIME)
    {
        mime_PushSection();
        mime_ParseSubheader(fp, first_line);
        first_line = nullptr;
    }
    g_mime_getc_line = first_line;
    DECODE_FUNC decoder = decode_function(g_mime_section->encoding);
    if (!decoder)
    {
        std::strcpy(g_msg,"Unhandled encoding type -- aborting.");
        if (fp)
        {
            std::fclose(fp);
        }
        if (dir)
        {
            decode_rmdir(dir);
        }
        return false;
    }

    /* Handle each part in order */
    DecodeState state;
    for (state = DECODE_START, part = 1; part <= total; part++)
    {
        if (part != 1)
        {
            std::sprintf(g_buf, "%s%d", dir, part);
            fp = std::fopen(g_buf, "r");
            if (!fp)
            {
                return true;
            }
        }

        state = decoder(fp, state);
        if (fp)
        {
            std::fclose(fp);
        }
        if (state == DECODE_ERROR)
        {
            std::strcpy(g_msg,"Failed.");
            return false;
        }
    }

    if (state != DECODE_DONE)
    {
        (void) decoder((FILE*)nullptr, DECODE_DONE);
        if (state != DECODE_MAYBEDONE)
        {
            std::strcpy(g_msg,"Premature EOF.");
            return false;
        }
    }

    if (fp)
    {
        /* Cleanup all the pieces */
        for (part = 0; part <= total; part++)
        {
            std::sprintf(g_buf, "%s%d", dir, part);
            remove(g_buf);
        }
        std::sprintf(g_buf, "%sCT", dir);
        remove(g_buf);
    }

    if (mcp)
    {
        mime_Exec(mcp->command);
        remove(g_decode_filename);
        change_dir("..");
    }

    if (dir)
    {
        decode_rmdir(dir);
    }

    return true;
}

DECODE_FUNC decode_function(MimeEncoding encoding)
{
    switch (encoding)
    {
    case MENCODE_QPRINT:
        return qp_decode;

    case MENCODE_BASE64:
        return b64_decode;

    case MENCODE_UUE:
        return uudecode;

    case MENCODE_NONE:
        return cat_decode;

    default:
        return nullptr;
    }
}

/* return a directory to use for unpacking the pieces of a given filename */
char *decode_mkdir(const char *filename)
{
    static char dir[LBUFLEN];

#ifdef MSDOS
    interp(dir, sizeof dir, "%Y/parts/");
#else
    interp(dir, sizeof dir, "%Y/m-prts-%L/");
#endif
    std::strcat(dir, filename);
    char *s = dir + std::strlen(dir);
    if (s[-1] == '/')
    {
        return nullptr;
    }
    *s++ = '/';
    *s = '\0';
    if (makedir(dir, MD_FILE))
    {
        return nullptr;
    }
    return dir;
}

void decode_rmdir(char *dir)
{
    /* Remove trailing slash */
    char *s = dir + std::strlen(dir) - 1;
    *s = '\0';

    /* TODO: conditionalize this */
    std::filesystem::remove(dir);
}
