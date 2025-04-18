/* uudecode.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/uudecode.h"

#include "trn/artio.h"
#include "trn/mime.h"
#include "trn/terminal.h"
#include "trn/string-algos.h"
#include "util/util2.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void uudecode_line(char *line, std::FILE *ofp);

int uue_prescan(char *bp, char **filenamep, int *partp, int *totalp)
{
    if (!std::strncmp(bp, "begin ", 6)                //
        && std::isdigit(bp[6]) && std::isdigit(bp[7]) //
        && std::isdigit(bp[8]) &&                     //
        (bp[9] == ' ' ||                              //
         (bp[6] == '0' && std::isdigit(bp[9]) && bp[10] == ' ')))
    {
        if (*partp == -1)
        {
            *filenamep = nullptr;
            *partp = 1;
            *totalp = 0;
        }
        return 1;
    }
    if (!std::strncmp(bp, "section ", 8) && std::isdigit(bp[8]))
    {
        char *s = bp + 8;
        int   tmppart = std::atoi(s);
        if (tmppart == 0)
        {
            return 0;
        }
        s = skip_digits(s);
        if (!std::strncmp(s, " of ", 4))
        {
            // "section N of ... of file F ..."
            for (s += 4; *s && std::strncmp(s," of file ",9) != 0; s++)
            {
            }
            if (!*s)
            {
                return 0;
            }
            s += 9;
            char *tmpfilename = s;
            s = std::strchr(s, ' ');
            if (!s)
            {
                return 0;
            }
            *s = '\0';
            *filenamep = tmpfilename;
            *partp = tmppart;
            *totalp = 0;
            return 1;
        }
        if (*s == '/' && std::isdigit(s[1]))
        {
            int tmptotal = std::atoi(s);
            s = skip_digits(s);
            s = skip_space(s);
            if (tmppart > tmptotal || std::strncmp(s,"file ",5) != 0)
            {
                return 0;
            }
            char *tmpfilename = s + 5;
            s = std::strchr(tmpfilename, ' ');
            if (!s)
            {
                return 0;
            }
            *s = '\0';
            *filenamep = tmpfilename;
            *partp = tmppart;
            *totalp = tmptotal;
            return 1;
        }
    }
    if (!std::strncmp(bp, "POST V", 6))
    {
        char *s = std::strchr(bp + 6, ' ');
        if (!s)
        {
            return 0;
        }
        char *tmpfilename = s + 1;
        s = std::strchr(tmpfilename, ' ');
        if (!s || std::strncmp(s, " (Part ", 7) != 0)
        {
            return 0;
        }
        *s = '\0';
        s += 7;
        int tmppart = std::atoi(s);
        s = skip_digits(s);
        if (tmppart == 0 || *s++ != '/')
        {
            return 0;
        }
        int tmptotal = std::atoi(s);
        s = skip_digits(s);
        if (tmppart > tmptotal || *s != ')')
        {
            return 0;
        }
        *filenamep = tmpfilename;
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (!std::strncmp(bp, "File: ", 6))
    {
        char *tmpfilename = bp + 6;
        char *s = std::strchr(tmpfilename, ' ');
        if (!s || std::strncmp(s, " -- part ", 9) != 0)
        {
            return 0;
        }
        *s = '\0';
        s += 9;
        int tmppart = std::atoi(s);
        s = skip_digits(s);
        if (tmppart == 0 || std::strncmp(s, " of ", 4) != 0)
        {
            return 0;
        }
        s += 4;
        int tmptotal = std::atoi(s);
        s = skip_digits(s);
        if (tmppart > tmptotal || std::strncmp(s, " -- ", 4) != 0)
        {
            return 0;
        }
        *filenamep = tmpfilename;
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (!std::strncmp(bp, "[Section: ", 10))
    {
        char *s = bp + 10;
        int   tmppart = std::atoi(s);
        if (tmppart == 0)
        {
            return 0;
        }
        s = skip_digits(s);
        int tmptotal = std::atoi(++s);
        s = skip_digits(s);
        s = skip_space(s);
        if (tmppart > tmptotal || std::strncmp(s, "File: ", 6) != 0)
        {
            return 0;
        }
        char *tmpfilename = s + 6;
        s = std::strchr(tmpfilename, ' ');
        if (!s)
        {
            return 0;
        }
        *s = '\0';
        *filenamep = tmpfilename;
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (*filenamep && *partp > 0 && *totalp > 0 && *partp <= *totalp                //
        && (!std::strncmp(bp, "BEGIN", 5) || !std::strncmp(bp, "--- BEGIN ---", 12) //
            || (bp[0] == 'M' && std::strlen(bp) == UU_LENGTH)))
    {
        // Found the start of a section of uuencoded data
        // and have the part N of M information.
        //
        return 1;
    }
    if (string_case_equal(bp, "x-file-name: ", 13))
    {
        char *s = skip_non_space(bp + 13);
        *s = '\0';
        safe_copy(g_msg, bp+13, sizeof g_msg);
        *filenamep = g_msg;
        return 0;
    }
    if (string_case_equal(bp, "x-part: ", 8))
    {
        int tmppart = std::atoi(bp + 8);
        if (tmppart > 0)
        {
            *partp = tmppart;
        }
        return 0;
    }
    if (string_case_equal(bp, "x-part-total: ", 14))
    {
        int tmptotal = std::atoi(bp + 14);
        if (tmptotal > 0)
        {
            *totalp = tmptotal;
        }
        return 0;
    }
    return 0;
}

DecodeState uudecode(std::FILE *ifp, DecodeState state)
{
    static std::FILE *ofp = nullptr;
    static int   line_length;

    if (state == DECODE_DONE)
    {
all_done:
        if (ofp)
        {
            std::fclose(ofp);
            ofp = nullptr;
        }
        return state;
    }

    while (ifp ? std::fgets(g_buf, sizeof g_buf, ifp) : read_art(g_buf, sizeof g_buf))
    {
        char lastline[UU_LENGTH+1];
        if (!ifp && mime_end_of_section(g_buf))
        {
            break;
        }
        char *p = std::strchr(g_buf, '\r');
        if (p)
        {
            p[0] = '\n';
            p[1] = '\0';
        }
        switch (state)
        {
        case DECODE_START:    // Looking for start of uuencoded file
        case DECODE_MAYBE_DONE:
        {
            if (std::strncmp(g_buf, "begin ", 6) != 0)
            {
                break;
            }
            // skip mode
            p = skip_non_space(g_buf + 6);
            p = skip_space(p);
            char *filename = p;
            while (*p && (!std::isspace(*p) || *p == ' '))
            {
                p++;
            }
            *p = '\0';
            if (!*filename)
            {
                return DECODE_ERROR;
            }
            filename = decode_fix_filename(filename);

            // Create output file and start decoding
            ofp = std::fopen(filename, "wb");
            if (!ofp)
            {
                return DECODE_ERROR;
            }
            std::printf("Decoding %s\n", filename);
            term_down(1);
            state = DECODE_SET_LEN;
            break;
        }

        case DECODE_INACTIVE: // Looking for uuencoded data to resume
            if (*g_buf != 'M' || std::strlen(g_buf) != line_length)
            {
                if (*g_buf == 'B' && !std::strncmp(g_buf, "BEGIN", 5))
                {
                    state = DECODE_ACTIVE;
                }
                break;
            }
            state = DECODE_ACTIVE;
            // FALL THROUGH

        case DECODE_SET_LEN:
            line_length = std::strlen(g_buf);
            state = DECODE_ACTIVE;
            // FALL THROUGH

        case DECODE_ACTIVE:   // Decoding data
            if (*g_buf == 'M' && std::strlen(g_buf) == line_length)
            {
                uudecode_line(g_buf, ofp);
                break;
            }
            if ((int)std::strlen(g_buf) > line_length)
            {
                state = DECODE_INACTIVE;
                break;
            }
            // May be nearing end of file, so save this line
            std::strcpy(lastline, g_buf);
            // some encoders put the end line right after the last M line
            if (!std::strncmp(g_buf, "end", 3))
            {
                goto end;
            }
            else if (*g_buf == ' ' || *g_buf == '`')
            {
                state = DECODE_LAST;
            }
            else
            {
                state = DECODE_NEXT_TO_LAST;
            }
            break;

        case DECODE_NEXT_TO_LAST:// May be nearing end of file
            if (!std::strncmp(g_buf, "end", 3))
            {
                goto end;
            }
            else if (*g_buf == ' ' || *g_buf == '`')
            {
                state = DECODE_LAST;
            }
            else
            {
                state = DECODE_INACTIVE;
            }
            break;

        case DECODE_LAST:     // Should be at end of file
            if (!std::strncmp(g_buf, "end", 3) && std::isspace(g_buf[3]))
            {
                // Handle that last line we saved
                uudecode_line(lastline, ofp);
end:            if (ofp)
                {
                    std::fclose(ofp);
                    ofp = nullptr;
                }
                state = DECODE_MAYBE_DONE;
            }
            else
            {
                state = DECODE_INACTIVE;
            }
            break;

        case DECODE_DONE:
            break;
        }
    }

    if (state == DECODE_DONE || state == DECODE_MAYBE_DONE)
    {
        goto all_done;
    }

    return DECODE_INACTIVE;
}

#define DEC(c)  (((c) - ' ') & 077)

// Decode a uuencoded line to 'ofp'
static void uudecode_line(char *line, std::FILE *ofp)
{
    // Calculate expected length and pad if necessary
    int len = ((DEC(line[0]) + 2) / 3) * 4;
    len = std::min(len, static_cast<int>(UU_LENGTH));
    for (int c = std::strlen(line) - 1; c <= len; c++)
    {
        line[c] = ' ';
    }

    len = DEC(*line++);
    while (len)
    {
        int c = DEC(*line) << 2 | DEC(line[1]) >> 4;
        std::putc(c, ofp);
        if (--len)
        {
            c = DEC(line[1]) << 4 | DEC(line[2]) >> 2;
            std::putc(c, ofp);
            if (--len)
            {
                c = DEC(line[2]) << 6 | DEC(line[3]);
                std::putc(c, ofp);
                len--;
            }
        }
        line += 4;
    }
}
