/* uudecode.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "uudecode.h"

#include "artio.h"
#include "mime.h"
#include "terminal.h"
#include "string-algos.h"
#include "util2.h"

#include <algorithm>

static void uudecodeline(char *line, FILE *ofp);

int uue_prescan(char *bp, char **filenamep, int *partp, int *totalp)
{
    if (!strncmp(bp, "begin ", 6)
        && isdigit(bp[6]) && isdigit(bp[7])
        && isdigit(bp[8]) && (bp[9] == ' ' ||
                              (bp[6] == '0' && isdigit(bp[9]) && bp[10] == ' '))) {
        if (*partp == -1) {
            *filenamep = nullptr;
            *partp = 1;
            *totalp = 0;
        }
        return 1;
    }
    if (!strncmp(bp,"section ",8) && isdigit(bp[8])) {
        char *s = bp + 8;
        int   tmppart = atoi(s);
        if (tmppart == 0)
        {
            return 0;
        }
        s = skip_digits(s);
        if (!strncmp(s, " of ", 4)) {
            /* "section N of ... of file F ..." */
            for (s += 4; *s && strncmp(s," of file ",9) != 0; s++)
            {
            }
            if (!*s)
            {
                return 0;
            }
            s += 9;
            char *tmpfilename = s;
            s = strchr(s, ' ');
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
        if (*s == '/' && isdigit(s[1])) {
            int tmptotal = atoi(s);
            s = skip_digits(s);
            s = skip_space(s);
            if (tmppart > tmptotal || strncmp(s,"file ",5) != 0)
            {
                return 0;
            }
            char *tmpfilename = s + 5;
            s = strchr(tmpfilename, ' ');
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
    if (!strncmp(bp, "POST V", 6)) {
        char *s = strchr(bp + 6, ' ');
        if (!s)
        {
            return 0;
        }
        char *tmpfilename = s + 1;
        s = strchr(tmpfilename, ' ');
        if (!s || strncmp(s, " (Part ", 7) != 0)
        {
            return 0;
        }
        *s = '\0';
        s += 7;
        int tmppart = atoi(s);
        s = skip_digits(s);
        if (tmppart == 0 || *s++ != '/')
        {
            return 0;
        }
        int tmptotal = atoi(s);
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
    if (!strncmp(bp, "File: ", 6)) {
        char *tmpfilename = bp + 6;
        char *s = strchr(tmpfilename, ' ');
        if (!s || strncmp(s, " -- part ", 9) != 0)
        {
            return 0;
        }
        *s = '\0';
        s += 9;
        int tmppart = atoi(s);
        s = skip_digits(s);
        if (tmppart == 0 || strncmp(s, " of ", 4) != 0)
        {
            return 0;
        }
        s += 4;
        int tmptotal = atoi(s);
        s = skip_digits(s);
        if (tmppart > tmptotal || strncmp(s, " -- ", 4) != 0)
        {
            return 0;
        }
        *filenamep = tmpfilename;
        *partp = tmppart;
        *totalp = tmptotal;
        return 1;
    }
    if (!strncmp(bp, "[Section: ", 10)) {
        char *s = bp + 10;
        int   tmppart = atoi(s);
        if (tmppart == 0)
        {
            return 0;
        }
        s = skip_digits(s);
        int tmptotal = atoi(++s);
        s = skip_digits(s);
        s = skip_space(s);
        if (tmppart > tmptotal || strncmp(s, "File: ", 6) != 0)
        {
            return 0;
        }
        char *tmpfilename = s + 6;
        s = strchr(tmpfilename, ' ');
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
    if (*filenamep && *partp > 0 && *totalp > 0 && *partp <= *totalp
     && (!strncmp(bp,"BEGIN",5) || !strncmp(bp,"--- BEGIN ---",12)
      || (bp[0] == 'M' && strlen(bp) == UULENGTH))) {
        /* Found the start of a section of uuencoded data
         * and have the part N of M information.
         */
        return 1;
    }
    if (string_case_equal(bp, "x-file-name: ", 13)) {
        char *s = skip_non_space(bp + 13);
        *s = '\0';
        safecpy(g_msg, bp+13, sizeof g_msg);
        *filenamep = g_msg;
        return 0;
    }
    if (string_case_equal(bp, "x-part: ", 8)) {
        int tmppart = atoi(bp + 8);
        if (tmppart > 0)
        {
            *partp = tmppart;
        }
        return 0;
    }
    if (string_case_equal(bp, "x-part-total: ", 14)) {
        int tmptotal = atoi(bp + 14);
        if (tmptotal > 0)
        {
            *totalp = tmptotal;
        }
        return 0;
    }
    return 0;
}

decode_state uudecode(FILE *ifp, decode_state state)
{
    static FILE *ofp = nullptr;
    static int   line_length;

    if (state == DECODE_DONE) {
      all_done:
        if (ofp) {
            fclose(ofp);
            ofp = nullptr;
        }
        return state;
    }

    while (ifp? fgets(g_buf, sizeof g_buf, ifp) : readart(g_buf, sizeof g_buf)) {
        char lastline[UULENGTH+1];
        if (!ifp && mime_EndOfSection(g_buf))
        {
            break;
        }
        char *p = strchr(g_buf, '\r');
        if (p) {
            p[0] = '\n';
            p[1] = '\0';
        }
        switch (state) {
          case DECODE_START:    /* Looking for start of uuencoded file */
          case DECODE_MAYBEDONE:
          {
              if (strncmp(g_buf, "begin ", 6) != 0)
              {
                  break;
              }
              /* skip mode */
              p = skip_non_space(g_buf + 6);
              p = skip_space(p);
              char *filename = p;
              while (*p && (!isspace(*p) || *p == ' '))
              {
                  p++;
              }
              *p = '\0';
              if (!*filename)
              {
                  return DECODE_ERROR;
              }
              filename = decode_fix_fname(filename);

              /* Create output file and start decoding */
              ofp = fopen(filename, "wb");
              if (!ofp)
              {
                  return DECODE_ERROR;
              }
              printf("Decoding %s\n", filename);
              termdown(1);
              state = DECODE_SETLEN;
              break;
          }
          case DECODE_INACTIVE: /* Looking for uuencoded data to resume */
            if (*g_buf != 'M' || strlen(g_buf) != line_length) {
                if (*g_buf == 'B' && !strncmp(g_buf, "BEGIN", 5))
                {
                    state = DECODE_ACTIVE;
                }
                break;
            }
            state = DECODE_ACTIVE;
            /* FALL THROUGH */
          case DECODE_SETLEN:
            line_length = strlen(g_buf);
            state = DECODE_ACTIVE;
            /* FALL THROUGH */
          case DECODE_ACTIVE:   /* Decoding data */
            if (*g_buf == 'M' && strlen(g_buf) == line_length) {
                uudecodeline(g_buf, ofp);
                break;
            }
            if ((int)strlen(g_buf) > line_length) {
                state = DECODE_INACTIVE;
                break;
            }
            /* May be nearing end of file, so save this line */
            strcpy(lastline, g_buf);
            /* some encoders put the end line right after the last M line */
            if (!strncmp(g_buf, "end", 3))
            {
                goto end;
            }
            else if (*g_buf == ' ' || *g_buf == '`')
            {
                state = DECODE_LAST;
            }
            else
            {
                state = DECODE_NEXT2LAST;
            }
            break;
          case DECODE_NEXT2LAST:/* May be nearing end of file */
            if (!strncmp(g_buf, "end", 3))
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
          case DECODE_LAST:     /* Should be at end of file */
            if (!strncmp(g_buf, "end", 3) && isspace(g_buf[3])) {
                /* Handle that last line we saved */
                uudecodeline(lastline, ofp);
end:            if (ofp) {
                    fclose(ofp);
                    ofp = nullptr;
                }
                state = DECODE_MAYBEDONE;
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

    if (state == DECODE_DONE || state == DECODE_MAYBEDONE)
    {
        goto all_done;
    }

    return DECODE_INACTIVE;
}

#define DEC(c)  (((c) - ' ') & 077)

/* Decode a uuencoded line to 'ofp' */
static void uudecodeline(char *line, FILE *ofp)
{
    /* Calculate expected length and pad if necessary */
    int len = ((DEC(line[0]) + 2) / 3) * 4;
    len = std::min(len, static_cast<int>(UULENGTH));
    for (int c = strlen(line) - 1; c <= len; c++)
    {
        line[c] = ' ';
    }

    len = DEC(*line++);
    while (len) {
        int c = DEC(*line) << 2 | DEC(line[1]) >> 4;
        putc(c, ofp);
        if (--len) {
            c = DEC(line[1]) << 4 | DEC(line[2]) >> 2;
            putc(c, ofp);
            if (--len) {
                c = DEC(line[2]) << 6 | DEC(line[3]);
                putc(c, ofp);
                len--;
            }
        }
        line += 4;
    }
}
