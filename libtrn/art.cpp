/* art.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "config/common.h"
#include "trn/art.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/artio.h"
#include "trn/artstate.h"
#include "trn/backpage.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/charsubst.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/help.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/mime.h"
#include "trn/ng.h"
#include "trn/ngstuff.h"
#include "trn/nntp.h"
#include "trn/rcstuff.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rt-wumpus.h"
#include "trn/rthread.h"
#include "trn/search.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/utf.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

ArticleLine    g_highlight{-1};         /* next line to be highlighted */
ArticleLine    g_first_view{};          //
ArticlePosition     g_raw_art_size{};         /* size in bytes of raw article */
ArticlePosition     g_art_size{};             /* size in bytes of article */
char        g_art_line[LBUFLEN];     /* place for article lines */
int         g_g_line{};               //
ArticlePosition     g_inner_search{};         /* g_artpos of end of line we want to visit */
ArticleLine    g_inner_light{};          /* highlight position for g_innersearch or 0 */
char        g_hide_everything{};     /* if set, do not write page now, ...but execute char when done with page */
bool        g_reread{};              /* consider current art temporarily unread? */
bool        g_do_fseek{};            /* should we back up in article file? */
bool        g_old_subject{};          /* not 1st art in subject thread */
ArticleLine    g_top_line{-1};           /* top line of current screen */
bool        g_do_hiding{true};       /* hide header lines with -h? */
bool        g_is_mime{};             /* process mime in an article? */
bool        g_multimedia_mime{};     /* images/audio to see/hear? */
bool        g_rotate{};              /* has rotation been requested? */
std::string g_prompt;                /* current prompt */
char       *g_first_line{};           /* s_special first line? */
const char *g_hide_line{};            /* custom line hiding? */
const char *g_page_stop{};            /* custom page terminator? */
CompiledRegex      g_hide_compex{};         //
CompiledRegex      g_page_compex{};         //
bool        g_dont_filter_control{}; /* -j */

inline char *line_ptr(ArticlePosition pos)
{
    return g_art_buf + pos - g_htype[PAST_HEADER].min_pos;
}
inline ArticlePosition line_offset(char *ptr)
{
    return ptr - g_art_buf + g_htype[PAST_HEADER].min_pos;
}

/* page_switch() return values */
enum PageSwitchResult
{
    PS_NORM = 0,
    PS_ASK = 1,
    PS_RAISE = 2,
    PS_TOEND = 3
};

static bool     s_special{};         /* is next page special length? */
static int      s_slines{};          /* how long to make page when special */
static ArticlePosition  s_restart{};         /* if nonzero, the place where last line left off on line split */
static ArticlePosition  s_alinebeg{};        /* where in file current line began */
static int      s_more_prompt_col{}; /* non-zero when the more prompt is indented */
static ArticleLine s_isrchline{};       /* last line to display */
static CompiledRegex   s_gcompex{};         /* in article search pattern */
static bool     s_firstpage{};       /* is this the 1st page of article? */
static bool     s_continuation{};    /* this line/header is being continued */

static PageSwitchResult page_switch();

void art_init()
{
    init_compex(&s_gcompex);
}

DoArticleResult do_article()
{
    char* s;
    bool hide_this_line = false; /* hidden header line? */
    bool under_lining = false;   /* are we underlining a word? */
    char* bufptr = g_art_line;   /* pointer to input buffer */
    int outpos;                  /* column position of output */
    static char prompt_buf[64];  /* place to hold prompt */
    bool notesfiles = false;     /* might there be notesfiles junk? */
    MinorMode oldmode = g_mode;
    bool outputok = true;

    if (g_data_source->flags & DF_REMOTE)
    {
        g_raw_art_size = nntp_artsize();
        g_art_size = nntp_artsize();
    }
    else
    {
        stat_t art_stat{};
        if (fstat(fileno(g_art_fp),&art_stat))   /* get article file stats */
        {
            return DA_CLEAN;
        }
        if (!S_ISREG(art_stat.st_mode))
        {
            return DA_NORM;
        }
        g_raw_art_size = art_stat.st_size;
        g_art_size = art_stat.st_size;
    }
    std::sprintf(prompt_buf, g_mousebar_cnt>3? "%%sEnd of art %ld (of %ld) %%s[%%s]"
        : "%%sEnd of article %ld (of %ld) %%s-- what next? [%%s]",
        (long)g_art,(long)g_lastart);   /* format prompt string */
    g_prompt = prompt_buf;
    g_int_count = 0;            /* interrupt count is 0 */
    s_firstpage = (g_top_line < 0);
    if (s_firstpage != 0)
    {
        parse_header(g_art);
        mime_set_article();
        clear_art_buf();
        seek_art(g_art_buf_seek = g_htype[PAST_HEADER].min_pos);
    }
    g_term_scrolled = 0;

    while (true) /* for each page */
    {
        if (g_threaded_group && g_max_tree_lines)
        {
            init_tree(); /* init tree display */
        }
        TRN_ASSERT(g_art == g_open_art);
        if (g_do_fseek)
        {
            parse_header(g_art);         /* make sure header is ours */
            if (!*g_art_buf)
            {
                mime_set_article();
                g_art_buf_seek = g_htype[PAST_HEADER].min_pos;
            }
            g_art_pos = virtual_read(g_art_line_num);
            if (g_art_pos < 0)
            {
                g_art_pos = -g_art_pos; /* labs(), anyone? */
            }
            if (s_firstpage)
            {
                g_art_pos = (ArticlePosition) 0;
            }
            if (g_art_pos < g_htype[PAST_HEADER].min_pos)
            {
                g_in_header = SOME_LINE;
                seek_art(g_htype[PAST_HEADER].min_pos);
                seek_art_buf(g_htype[PAST_HEADER].min_pos);
            }
            else
            {
                seek_art(g_art_buf_seek);
                seek_art_buf(g_art_pos);
            }
            g_do_fseek = false;
            s_restart = 0;
        }
        ArticleLine linenum = 1;
#if 0 /* This causes a bug (headers displayed twice sometimes when you press v then ^R) */
        if (!g_do_hiding)
        {
            g_is_mime = false;
        }
#endif
        if (s_firstpage)
        {
            if (g_first_line)
            {
                interp(g_art_line,sizeof g_art_line,g_first_line);
                linenum += tree_puts(g_art_line,linenum+g_top_line,0);
            }
            else
            {
                ArticleNum i;

                int selected = (g_curr_artp->flags & AF_SEL);
                int unseen = article_unread(g_art) ? 1 : 0;
                std::sprintf(g_art_line,"%s%s #%ld",g_ngname.c_str(),g_moderated.c_str(),(long)g_art);
                if (g_selected_only)
                {
                    i = g_selected_count - (unseen && selected);
                    std::sprintf(g_art_line+std::strlen(g_art_line)," (%ld + %ld more)",
                            (long)i,(long)g_ngptr->toread - g_selected_count
                                        - (!selected && unseen));
                }
                else if ((i = (ArticleNum) (g_ngptr->toread - unseen)) != 0 //
                         || (!g_threaded_group && g_dm_count))
                {
                    std::sprintf(g_art_line+std::strlen(g_art_line),
                            " (%ld more)",(long)i);
                }
                if (!g_threaded_group && g_dm_count)
                {
                    std::sprintf(g_art_line + std::strlen(g_art_line) - 1, " + %ld Marked to return)", (long) g_dm_count);
                }
                linenum += tree_puts(g_art_line,linenum+g_top_line,0);
            }
            start_header(g_art);
            g_force_last = false;        /* we will have our day in court */
            s_restart = 0;
            g_art_line_num = 0;              /* start counting lines */
            g_art_pos = 0;
            virtual_write(g_art_line_num,g_art_pos); /* remember pos in file */
        }
        for (bool restart_color = true; /* linenum already set */
          g_inner_search? (g_in_header || inner_more())
           : s_special? (linenum < s_slines)
           : (s_firstpage && !g_in_header)? (linenum < g_initlines)
           : (linenum < g_tc_LINES);
             linenum++)
        {                               /* for each line on page */
            if (g_int_count)            /* exit via interrupt? */
            {
                newline();              /* get to left margin */
                g_int_count = 0;        /* reset interrupt count */
                set_mode(g_general_mode,oldmode);
                s_special = false;
                return DA_NORM;         /* skip out of loops */
            }
            if (s_restart)              /* did not finish last line? */
            {
                bufptr = line_ptr(s_restart);/* then start again here */
                s_restart = 0;          /* and reset the flag */
                s_continuation = true;
                if (restart_color && g_do_hiding && !g_in_header)
                {
                    maybe_set_color(bufptr, true);
                }
            }
            else if (g_in_header && *(bufptr = g_head_buf + g_art_pos))
            {
                s_continuation = is_hor_space(*bufptr);
            }
            else
            {
                bufptr = read_art_buf(g_auto_view_inline);
                if (bufptr == nullptr)
                {
                    s_special = false;
                    if (g_inner_search)
                    {
                        (void) inner_more();
                    }
                    break;
                }
                if (g_do_hiding && !g_in_header)
                {
                    s_continuation = maybe_set_color(bufptr, restart_color);
                }
                else
                {
                    s_continuation = false;
                }
            }
            s_alinebeg = g_art_pos;      /* remember where we began */
            restart_color = false;
            if (g_in_header)
            {
                hide_this_line = parse_line(bufptr,g_do_hiding,hide_this_line);
                if (!g_in_header)
                {
                    linenum += finish_tree(linenum+g_top_line);
                    end_header();
                    seek_art(g_art_buf_seek);
                }
            }
            else if (notesfiles && g_do_hiding && !s_continuation //
                     && *bufptr == '#' && std::isupper(bufptr[1]) //
                     && bufptr[2] == ':')
            {
                bufptr = read_art_buf(g_auto_view_inline);
                if (bufptr == nullptr)
                {
                    break;
                }
                for (s = bufptr; *s && *s != '\n' && *s != '!'; s++)
                {
                }
                if (*s != '!')
                {
                    read_art_buf(g_auto_view_inline);
                }
                mime_set_article();
                clear_art_buf();         /* exclude notesfiles droppings */
                g_htype[PAST_HEADER].min_pos = tell_art();
                g_art_buf_seek = tell_art();
                hide_this_line = true;  /* and do not print either */
                notesfiles = false;
            }
            if (g_hide_line && !s_continuation && execute(&g_hide_compex,bufptr))
            {
                hide_this_line = true;
            }
            if (g_in_header && g_do_hiding && (g_htype[g_in_header].flags & HT_MAGIC))
            {
                switch (g_in_header)
                {
                case NEWSGROUPS_LINE:
                    s = std::strchr(bufptr, '\n');
                    if (s != nullptr)
                    {
                        *s = '\0';
                    }
                    hide_this_line = (std::strchr(bufptr,',') == nullptr)
                        && !strcmp(bufptr+12,g_ngname.c_str());
                    if (s != nullptr)
                    {
                        *s = '\n';
                    }
                    break;

                case EXPIR_LINE:
                    if (!(g_htype[EXPIR_LINE].flags & HT_HIDE))
                    {
                        s = bufptr + g_htype[EXPIR_LINE].length + 1;
                        hide_this_line = *s != ' ' || s[1] == '\n';
                    }
                    break;

                case FROM_LINE:
                    if ((s = std::strchr(bufptr,'\n')) != nullptr
                     && s-bufptr < sizeof g_art_line)
                    {
                        safecpy(g_art_line,bufptr,s-bufptr+1);
                    }
                    else
                    {
                        safecpy(g_art_line,bufptr,sizeof g_art_line);
                    }
                    s = extract_name(g_art_line + 6);
                    if (s != nullptr)
                    {
                        std::strcpy(g_art_line+6,s);
                        bufptr = g_art_line;
                    }
                    break;

                case DATE_LINE:
                    if (g_curr_artp->date != -1)
                    {
                        std::strncpy(g_art_line,bufptr,6);
                        std::strftime(g_art_line+6, (sizeof g_art_line)-6,
                                 get_val_const("LOCALTIMEFMT", LOCALTIMEFMT),
                                 std::localtime(&g_curr_artp->date));
                        bufptr = g_art_line;
                    }
                    break;
                }
            }
            if (g_in_header == SUBJ_LINE && g_do_hiding   //
                && (g_htype[SUBJ_LINE].flags & HT_MAGIC)) /* handle the subject */
            {
                s = get_cached_line(g_artp, SUBJ_LINE, false);
                if (s && s_continuation)
                {
                    /* continuation lines were already output */
                    linenum--;
                }
                else
                {
                    int length = std::strlen(bufptr+1);
                    notesfiles = in_string(&bufptr[length-10]," - (nf", true)!=nullptr;
                    g_art_line_num++;
                    if (!s)
                    {
                        bufptr += (s_continuation ? 0 : 9);
                    }
                    else
                    {
                        bufptr = s;
                    }
                    /* tree_puts(, ,1) underlines subject */
                    linenum += tree_puts(bufptr,linenum+g_top_line,1)-1;
                }
            }
            else if (hide_this_line && g_do_hiding)     /* do not print line? */
            {
                linenum--;                        /* compensate for linenum++ */
                if (!g_in_header)
                {
                    hide_this_line = false;
                }
            }
            else if (g_in_header)
            {
                g_art_line_num++;
                linenum += tree_puts(bufptr,linenum+g_top_line,0)-1;
            }
            else                          /* just a normal line */
            {
                if (outputok && g_erase_each_line)
                {
                    erase_line(false);
                }
                if (g_highlight == g_art_line_num)   /* this line to be highlit? */
                {
                    if (g_marking == STANDOUT)
                    {
#ifdef NOFIREWORKS
                        if (g_erase_screen)
                        {
                            no_sofire();
                        }
#endif
                        standout();
                    }
                    else
                    {
#ifdef NOFIREWORKS
                        if (g_erase_screen)
                        {
                            no_ulfire();
                        }
#endif
                        underline();
                        carriage_return();
                    }
                    if (*bufptr == '\n')
                    {
                        std::putchar(' ');
                    }
                }
                outputok = !g_hide_everything; /* registerize it, hopefully */
                if (g_page_stop && !s_continuation && execute(&g_page_compex,bufptr))
                {
                    linenum = 32700;
                }
                for (outpos = 0; outpos < g_tc_COLS; )   /* while line has room */
                {
                    if (at_norm_char(bufptr))       /* normal char? */
                    {
                        if (*bufptr == '_')
                        {
                            if (bufptr[1] == '\b')
                            {
                                if (outputok && !under_lining && g_highlight != g_art_line_num)
                                {
                                    under_lining = true;
                                    if (g_tc_UG)
                                    {
                                        if (bufptr != g_buf && bufptr[-1] == ' ')
                                        {
                                            outpos--;
                                            backspace();
                                        }
                                    }
                                    underline();
                                }
                                bufptr += 2;
                            }
                        }
                        else
                        {
                            if (under_lining)
                            {
                                under_lining = false;
                                un_underline();
                                if (g_tc_UG)
                                {
                                    outpos++;
                                    if (*bufptr == ' ')
                                    {
                                        goto skip_put;
                                    }
                                }
                            }
                        }
                        /* handle rot-13 if wanted */
                        if (g_rotate && !g_in_header && std::isalpha(*bufptr))
                        {
                            if (outputok)
                            {
                                if ((*bufptr & 31) <= 13)
                                {
                                    std::putchar(*bufptr + 13);
                                }
                                else
                                {
                                    std::putchar(*bufptr - 13);
                                }
                            }
                            outpos++;
                        }
                        else
                        {
                            int i;
#ifdef USE_UTF_HACK
                            if (outpos + visual_width_at(bufptr) > g_tc_COLS)   /* will line overflow? */
                            {
                                newline();
                                outpos = 0;
                                linenum++;
                            }
                            i = put_char_adv(&bufptr, outputok);
                            bufptr--;
#else /* !USE_UTF_HACK */
                            i = putsubstchar(*bufptr, g_tc_COLS - outpos, outputok);
#endif /* USE_UTF_HACK */
                            if (i < 0)
                            {
                                outpos += -i - 1;
                                break;
                            }
                            outpos += i;
                        }
                        if (*g_tc_UC && ((g_highlight == g_art_line_num && g_marking == STANDOUT) || under_lining))
                        {
                            backspace();
                            underchar();
                        }
                    skip_put:
                        bufptr++;
                    }
                    else if (at_nl(*bufptr) || !*bufptr)      /* newline? */
                    {
                        if (under_lining)
                        {
                            under_lining = false;
                            un_underline();
                        }
#ifdef DEBUG
                        if (debug & DEB_INNERSRCH && outpos < g_tc_COLS - 6)
                        {
                            standout();
                            std::printf("%4d",g_artline);
                            un_standout();
                        }
#endif
                        if (outputok)
                        {
                            newline();
                        }
                        s_restart = 0;
                        outpos = 1000;  /* signal normal \n */
                    }
                    else if (*bufptr == '\t')   /* tab? */
                    {
                        int incpos =  8 - outpos % 8;
                        if (outputok)
                        {
                            if (g_tc_GT)
                            {
                                std::putchar(*bufptr);
                            }
                            else
                            {
                                while (incpos--)
                                {
                                    std::putchar(' ');
                                }
                            }
                        }
                        bufptr++;
                        outpos += 8 - outpos % 8;
                    }
                    else if (*bufptr == '\f')   /* form feed? */
                    {
                        if (outpos+2 > g_tc_COLS)
                        {
                            break;
                        }
                        if (outputok)
                        {
                            std::fputs("^L", stdout);
                        }
                        if (bufptr == line_ptr(s_alinebeg) && g_highlight != g_art_line_num)
                        {
                            linenum = 32700;
                            /* how is that for a magic number? */
                        }
                        bufptr++;
                        outpos += 2;
                    }
                    else                  /* other control char */
                    {
                        if (g_dont_filter_control)
                        {
                            if (outputok)
                            {
                                std::putchar(*bufptr);
                            }
                            outpos++;
                        }
                        else if (*bufptr != '\r' || bufptr[1] != '\n')
                        {
                            if (outpos+2 > g_tc_COLS)
                            {
                                break;
                            }
                            if (outputok)
                            {
                                std::putchar('^');
                                if (g_highlight == g_art_line_num && *g_tc_UC && g_marking == STANDOUT)
                                {
                                    backspace();
                                    underchar();
                                    std::putchar((*bufptr & 0x7F) ^ 0x40);
                                    backspace();
                                    underchar();
                                }
                                else
                                {
                                    std::putchar((*bufptr & 0x7F) ^ 0x40);
                                }
                            }
                            outpos += 2;
                        }
                        bufptr++;
                    }
                } /* end of column loop */

                if (outpos < 1000)      /* did line overflow? */
                {
                    s_restart = line_offset(bufptr);/* restart here next time */
                    if (outputok)
                    {
                        if (!g_tc_AM || g_tc_XN || outpos < g_tc_COLS)
                        {
                            newline();
                        }
                        else
                        {
                            g_term_line++;
                        }
                    }
                    if (at_nl(*bufptr))         /* skip the newline */
                    {
                        s_restart = 0;
                    }
                }

                /* handle normal end of output line formalities */

                if (g_highlight == g_art_line_num)
                {
                    if (g_marking == STANDOUT)  /* were we highlighting line? */
                    {
                        un_standout();
                    }
                    else
                    {
                        un_underline();
                    }
                    carriage_return();
                    g_highlight = -1;   /* no more we are */
                    /* in case terminal highlighted rest of line earlier */
                    /* when we did an eol with highlight turned on: */
                    if (g_erase_each_line)
                    {
                        erase_eol();
                    }
                }
                g_art_line_num++;    /* count the line just printed */
                            /* did we just scroll top line off? */
                            /* then recompute top line # */
                g_top_line = std::max(g_art_line_num - g_tc_LINES + 1, g_top_line);
            }

            /* determine actual position in file */

            if (s_restart)      /* stranded somewhere in the buffer? */
            {
                g_art_pos += s_restart - s_alinebeg;
            }
            else if (g_in_header)
            {
                g_art_pos = std::strchr(g_head_buf + g_art_pos, '\n') - g_head_buf + 1;
            }
            else
            {
                g_art_pos = g_art_buf_pos + g_htype[PAST_HEADER].min_pos;
            }
            virtual_write(g_art_line_num,g_art_pos); /* remember pos in file */
        } /* end of line loop */

        g_inner_search = 0;
        if (g_hide_everything)
        {
            *g_buf = g_hide_everything;
            g_hide_everything = 0;
            goto fake_command;
        }
        if (linenum >= 32700)   /* did last line have formfeed? */
        {
            virtual_write(g_art_line_num - 1, -virtual_read(g_art_line_num - 1));
                                /* remember by negating pos in file */
        }

        s_special = false;      /* end of page, so reset page length */
        s_firstpage = false;    /* and say it is not 1st time thru */
        g_highlight = -1;

        /* extra loop bombout */

        if (g_art_size < 0 && (g_raw_art_size = nntp_artsize()) >= 0)
        {
            g_art_size = g_raw_art_size-g_art_buf_seek+g_art_buf_len+g_htype[PAST_HEADER].min_pos;
        }
recheck_pager:
        if (g_do_hiding && g_art_buf_pos == g_art_buf_len)
        {
            /* If we're filtering we need to figure out if any
             * remaining text is going to vanish or not. */
            long seekpos = g_art_buf_pos + g_htype[PAST_HEADER].min_pos;
            read_art_buf(false);
            seek_art_buf(seekpos);
        }
        if (g_art_pos == g_art_size)  /* did we just now reach EOF? */
        {
            color_default();
            set_mode(g_general_mode,oldmode);
            return DA_NORM;     /* avoid --MORE--(100%) */
        }

/* not done with this article, so pretend we are a pager */

reask_pager:
        if (g_term_line >= g_tc_LINES)
        {
            g_term_scrolled += g_term_line - g_tc_LINES + 1;
            g_term_line = g_tc_LINES-1;
        }
        s_more_prompt_col = g_term_col;

        unflush_output();       /* disable any ^O in effect */
         maybe_eol();
        color_default();
        if (g_art_size < 0)
        {
            std::strcpy(g_cmd_buf, "?");
        }
        else
        {
            std::sprintf(g_cmd_buf, "%ld", (long) (g_art_pos * 100 / g_art_size));
        }
        std::sprintf(g_buf,"%s--MORE--(%s%%)",current_char_subst(),g_cmd_buf);
        outpos = g_term_col + std::strlen(g_buf);
        draw_mousebar(g_tc_COLS - (g_term_line == g_tc_LINES-1? outpos+5 : 0), true);
        color_string(COLOR_MORE,g_buf);
        std::fflush(stdout);
        g_term_col = outpos;
        eat_typeahead();
#ifdef DEBUG
        if (debug & DEB_CHECKPOINTING)
        {
            std::printf("(%d %d %d)",g_checkcount,linenum,g_artline);
            std::fflush(stdout);
        }
#endif
        if (g_check_count >= g_do_check_when && linenum == g_tc_LINES &&
            (g_art_line_num > 40 || g_check_count >= g_do_check_when + 10))
        {
                            /* while he is reading a whole page */
                            /* in an article he is interested in */
            g_check_count = 0;
            checkpoint_newsrcs();       /* update all newsrcs */
            update_thread_kill_file();
        }
        cache_until_key();
        if (g_art_size < 0 && (g_raw_art_size = nntp_artsize()) >= 0)
        {
            g_art_size = g_raw_art_size-g_art_buf_seek+g_art_buf_len+g_htype[PAST_HEADER].min_pos;
            goto_xy(s_more_prompt_col,g_term_line);
            goto recheck_pager;
        }
        set_mode(g_general_mode,MM_PAGER);
        getcmd(g_buf);
        if (errno)
        {
            if (g_tc_LINES < 100 && !g_int_count)
            {
                *g_buf = '\f'; /* on CONT fake up refresh */
            }
            else
            {
                *g_buf = 'q';   /* on INTR or paper just quit */
            }
        }
        erase_line(g_erase_screen && g_erase_each_line);

    fake_command:               /* used by g_innersearch */
        color_default();
        g_output_chase_phrase = true;

        /* parse and process pager command */

        if (g_mousebar_cnt)
        {
            clear_rest();
        }
        switch (page_switch())
        {
        case PS_ASK:  /* reprompt "--MORE--..." */
            goto reask_pager;

        case PS_RAISE:        /* reparse on article level */
            set_mode(g_general_mode,oldmode);
            return DA_RAISE;

        case PS_TOEND:        /* fast pager loop exit */
            set_mode(g_general_mode,oldmode);
            return DA_TO_END;

        case PS_NORM:         /* display more article */
            break;
        }
    } /* end of page loop */
}

bool maybe_set_color(const char *cp, bool back_search)
{
    const char ch = (cp == g_art_buf || cp == g_art_line? 0 : cp[-1]);
    if (ch == '\001')
    {
        color_object(COLOR_MIME_DESC, false);
    }
    else if (ch == '\002')
    {
        color_object(COLOR_MIME_SEP, false);
    }
    else if (ch == WRAPPED_NL)
    {
        if (back_search)
        {
            while (cp > g_art_buf && cp[-1] != '\n')
            {
                cp--;
            }
            maybe_set_color(cp, false);
        }
        return true;
    }
    else
    {
        cp = skip_hor_space(cp);
        if (std::strchr(">}]#!:|", *cp))
        {
            color_object(COLOR_CITE_DTEXT, false);
        }
        else
        {
            color_object(COLOR_BODY_TEXT, false);
        }
    }
    return false;
}

/* process pager commands */

PageSwitchResult page_switch()
{
    char* s;

    switch (*g_buf)
    {
    case '!':                 /* shell escape */
        escapade();
        return PS_ASK;

    case Ctl('i'):
    {
        ArticleLine i = g_art_line_num;
        g_g_line = 3;
        s = line_ptr(s_alinebeg);
        while (at_nl(*s) && i >= g_top_line)
        {
            ArticlePosition pos = virtual_read(--i);
            if (pos < 0)
            {
                pos = -pos;
            }
            if (pos < g_htype[PAST_HEADER].min_pos)
            {
                break;
            }
            seek_art_buf(pos);
            s = read_art_buf(false);
            if (s == nullptr)
            {
                s = line_ptr(s_alinebeg);
                break;
            }
        }
        std::sprintf(g_cmd_buf,"^[^%c\n]",*s);
        compile(&s_gcompex,g_cmd_buf,true,true);
        goto caseG;
    }

    case Ctl('g'):
        g_g_line = 3;
        compile(&s_gcompex,"^Subject:",true,true);
        goto caseG;

    case 'g':         /* in-article search */
        if (!finish_command(false))/* get rest of command */
        {
            return PS_ASK;
        }
        s = g_buf+1;
        if (std::isspace(*s))
        {
            s++;
        }
        s = compile(&s_gcompex, s, true, true);
        if (s != nullptr)
        {
                            /* compile regular expression */
            std::printf("\n%s\n", s);
            termdown(2);
            return PS_ASK;
        }
        erase_line(false);      /* erase the prompt */
        /* FALL THROUGH */
      caseG:

    case 'G':
    {
        ArticlePosition start_where;
        bool success;
        char* nlptr;
        char ch;

        if (g_g_line < 0 || g_g_line > g_tc_LINES-2)
        {
            g_g_line = g_tc_LINES - 2;
        }
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("Start here? %d  >=? %d\n",g_topline + g_gline + 1,g_artline);
            termdown(1);
        }
#endif
        if (*g_buf == Ctl('i') || g_top_line+g_g_line+1 >= g_art_line_num)
        {
            start_where = g_art_pos;
                        /* in case we had a line wrap */
        }
        else
        {
            start_where = virtual_read(g_top_line+g_g_line+1);
            if (start_where < 0)
            {
                start_where = -start_where;
            }
        }
        start_where = std::max(start_where, g_htype[PAST_HEADER].min_pos);
        seek_art_buf(start_where);
        g_inner_light = 0;
        g_inner_search = 0; /* assume not found */
        while ((s = read_art_buf(false)) != nullptr)
        {
            nlptr = std::strchr(s, '\n');
            if (nlptr != nullptr)
            {
                ch = *++nlptr;
                *nlptr = '\0';
            }
#ifdef DEBUG
            if (debug & DEB_INNERSRCH)
            {
                std::printf("Test %s\n",s);
            }
#endif
            success = execute(&s_gcompex,s) != nullptr;
            if (nlptr)
            {
                *nlptr = ch;
            }
            if (success)
            {
                g_inner_search = g_art_buf_pos + g_htype[PAST_HEADER].min_pos;
                break;
            }
        }
        if (!g_inner_search)
        {
            seek_art_buf(g_art_pos);
            std::fputs("(Not found)", stdout);
            g_term_col = 11;
            return PS_ASK;
        }
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("On page? %ld <=? %ld\n",(long)g_innersearch,(long)g_artpos);
            termdown(1);
        }
#endif
        if (g_inner_search <= g_art_pos)          /* already on page? */
        {
            if (g_inner_search < g_art_pos)
            {
                g_art_line_num = g_top_line+1;
                while (virtual_read(g_art_line_num) < g_inner_search)
                {
                    g_art_line_num++;
                }
            }
            g_highlight = g_art_line_num - 1;
#ifdef DEBUG
            if (debug & DEB_INNERSRCH)
            {
                std::printf("@ %d\n",g_highlight);
                termdown(1);
            }
#endif
            g_top_line = g_highlight - g_g_line;
            g_top_line = std::max(g_top_line, -1);
            *g_buf = '\f';              /* fake up a refresh */
            g_inner_search = 0;
            return page_switch();
        }
        g_do_fseek = true;              /* who knows how many lines it is? */
        g_hide_everything = '\f';
        return PS_NORM;
    }

    case '\n':                        /* one line down */
    case '\r':
        s_special = true;
        s_slines = 2;
        return PS_NORM;

    case 'X':
        g_rotate = !g_rotate;
        /* FALL THROUGH */

    case 'l':
    case '\f':                /* refresh screen */
      refresh_screen:
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("Topline = %d",g_topline);
            std::fgets(g_buf, sizeof g_buf, stdin);
        }
#endif
        clear();
        g_do_fseek = true;
        g_art_line_num = g_top_line;
        g_art_line_num = std::max(g_art_line_num, 0);
        s_firstpage = (g_top_line < 0);
        return PS_NORM;

    case Ctl('e'):
        if (g_art_size < 0)
        {
            nntp_finishbody(FB_OUTPUT);
            g_raw_art_size = nntp_artsize();
            g_art_size = g_raw_art_size-g_art_buf_seek+g_art_buf_len+g_htype[PAST_HEADER].min_pos;
        }
        if (g_do_hiding)
        {
            seek_art_buf(g_art_size);
            seek_art_buf(g_art_pos);
        }
        g_top_line = g_art_line_num;
        g_inner_light = g_art_line_num - 1;
        g_inner_search = g_art_size;
        g_g_line = 0;
        g_hide_everything = 'b';
        return PS_NORM;

    case 'B':         /* one line up */
        if (g_top_line < 0)
        {
            break;
        }
        if (*g_tc_IL && *g_tc_HO)
        {
            home_cursor();
            insert_line();
            carriage_return();
            ArticlePosition pos = virtual_read(g_top_line - 1);
            if (pos < 0)
            {
                pos = -pos;
            }
            if (pos >= g_htype[PAST_HEADER].min_pos)
            {
                seek_art_buf(pos);
                s = read_art_buf(false);
                if (s != nullptr)
                {
                    g_art_pos = virtual_read(g_top_line);
                    if (g_art_pos < 0)
                    {
                        g_art_pos = -g_art_pos;
                    }
                    maybe_set_color(s, true);
                    for (pos = g_art_pos - pos; pos-- && !at_nl(*s); s++)
                    {
                        std::putchar(*s);
                    }
                    color_default();
                    std::putchar('\n');
                    g_top_line--;
                    g_art_pos = virtual_read(--g_art_line_num);
                    if (g_art_pos < 0)
                    {
                        g_art_pos = -g_art_pos;
                    }
                    seek_art_buf(g_art_pos);
                    s_alinebeg = virtual_read(g_art_line_num-1);
                    if (s_alinebeg < 0)
                    {
                        s_alinebeg = -s_alinebeg;
                    }
                    goto_xy(0,g_art_line_num-g_top_line);
                    erase_line(false);
                    return PS_ASK;
                }
            }
        }
        /* FALL THROUGH */

    case 'b':
    case Ctl('b'):    /* back up a page */
    {
        ArticleLine target;

        if (g_erase_each_line)
        {
            home_cursor();
        }
        else
        {
            clear();
        }

        g_do_fseek = true;      /* reposition article file */
        if (*g_buf == 'B')
        {
            target = g_top_line - 1;
        }
        else
        {
            target = g_top_line - (g_tc_LINES - 2);
            if (g_marking && (g_marking_areas & BACKPAGE_MARKING))
            {
                g_highlight = g_top_line;
            }
        }
        g_art_line_num = g_top_line;
        if (g_art_line_num >= 0)
        {
            do
            {
                g_art_line_num--;
            } while (g_art_line_num >= 0 && g_art_line_num > target && virtual_read(g_art_line_num - 1) >= 0);
        }
        g_top_line = g_art_line_num;  /* remember top line of screen */
                                /*  (line # within article file) */
        g_art_line_num = std::max(g_art_line_num, 0);
        s_firstpage = (g_top_line < 0);
        return PS_NORM;
      }

    case 'H':         /* help */
        help_page();
        return PS_ASK;

    case 't':         /* output thread data */
        g_page_line = 1;
        entire_tree(g_curr_artp);
        return PS_ASK;

    case '_':
        if (!finish_dblchar())
        {
            return PS_ASK;
        }
        switch (g_buf[1] & 0177)
        {
        case 'C':
            if (!*(++g_char_subst))
            {
                g_char_subst = g_charsets.c_str();
            }
            goto refresh_screen;

        default:
            break;
        }
        goto leave_pager;

    case '\0':                /* treat break as 'n' */
        *g_buf = 'n';
        /* FALL THROUGH */

    case 'a': case 'A':
    case 'e':
    case 'k': case 'K': case 'J':
    case 'n': case 'N': case Ctl('n'):
              case 'F':
              case 'R':
    case 's': case 'S':
              case 'T':
    case 'u':
    case 'w': case 'W':
    case '|':
        mark_as_read(g_artp);   /* mark article as read */
        /* FALL THROUGH */

    case 'U': case ',':
    case '<': case '>':
    case '[': case ']':
    case '{': case '}':
    case '(': case ')':
    case ':':
    case '+':
    case Ctl('v'):            /* verify crypto signature */
    case ';':                 /* enter article scan mode */
    case '"':                 /* append to local scorefile */
    case '\'':                /* score command */
    case '#':
    case '$':
    case '&':
    case '-':
    case '.':
    case '/':
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
    case '=':
    case '?':
    case 'c': case 'C':
#ifdef DEBUG
              case 'D':
#endif
    case 'f':           case Ctl('f'):
    case 'h':
    case 'j':
                        case Ctl('k'):
    case 'm': case 'M':
    case 'p': case 'P': case Ctl('p'):
    case '`': case 'Q':
    case 'r':           case Ctl('r'):
    case 'v':
    case 'x':           case Ctl('x'):
              case 'Y':
    case 'z': case 'Z':
    case '^':           case Ctl('^'):
    case '\b': case '\177':
leave_pager:
        g_reread = false;
        if (std::strchr("nNpP\016\020", *g_buf) == nullptr //
            && std::strchr("wWsSe:!&|/?123456789.", *g_buf) != nullptr)
        {
            set_default_cmd();
            color_object(COLOR_CMD, true);
            interp_search(g_cmd_buf, sizeof g_cmd_buf, g_mail_call, g_buf);
            std::printf(g_prompt.c_str(),g_cmd_buf,
                   current_char_subst(),
                   g_default_cmd.c_str());  /* print prompt, whatever it is */
            color_pop();        /* of COLOR_CMD */
            std::putchar(' ');
            std::fflush(stdout);
        }
        return PS_RAISE;        /* and pretend we were at end */

    case 'd':         /* half page */
    case Ctl('d'):
        s_special = true;
        s_slines = g_tc_LINES / 2 + 1;
        /* no divide-by-zero, thank you */
        if (g_tc_LINES > 2 && (g_tc_LINES & 1) && g_art_line_num % (g_tc_LINES-2) >= g_tc_LINES/2 - 1)
        {
            s_slines++;
        }
        goto go_forward;

    case 'y':
    case ' ': /* continue current article */
        if (g_erase_screen)     /* -e? */
        {
            if (g_erase_each_line)
            {
                home_cursor();
            }
            else
            {
                clear();        /* clear screen */
            }
            std::fflush(stdout);
        }
        else
        {
            s_special = true;
            s_slines = g_tc_LINES;
        }
      go_forward:
          if (*line_ptr(s_alinebeg) != '\f' && (!g_page_stop || s_continuation || !execute(&g_page_compex, line_ptr(s_alinebeg))))
          {
              if (!s_special //
                  || (g_marking && (*g_buf != 'd' || (g_marking_areas & HALFPAGE_MARKING))))
              {
                s_restart = s_alinebeg;
                g_art_line_num--;     /* restart this line */
                g_art_pos = s_alinebeg;
                if (s_special)
                {
                    up_line();
                }
                else
                {
                    g_top_line = g_art_line_num;
                }
                if (g_marking)
                {
                    g_highlight = g_art_line_num;
                }
              }
              else
              {
                  s_slines--;
              }
        }
        return PS_NORM;

    case 'i':
        g_auto_view_inline = !g_auto_view_inline;
        if (g_auto_view_inline != 0)
        {
            g_first_view = 0;
        }
        std::printf("\nAuto-View inlined mime is %s\n", g_auto_view_inline? "on" : "off");
        termdown(2);
        break;

    case 'q': /* quit this article? */
        return PS_TOEND;

    default:
        std::fputs(g_hforhelp,stdout);
        termdown(1);
        settle_down();
        return PS_ASK;
    }
    return PS_ASK;
}

bool inner_more()
{
    if (g_art_pos < g_inner_search)               /* not even on page yet? */
    {
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("Not on page %ld < %ld\n",(long)g_artpos,(long)g_innersearch);
        }
#endif
        return true;
    }
    if (g_art_pos == g_inner_search)      /* just got onto page? */
    {
        s_isrchline = g_art_line_num;        /* remember first line after */
        if (g_inner_light)
        {
            g_highlight = g_inner_light;
        }
        else
        {
            g_highlight = g_art_line_num - 1;
        }
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("There it is %ld = %ld, %d @ %d\n",(long)g_artpos,
                (long)g_innersearch,g_hide_everything,g_highlight);
            termdown(1);
        }
#endif
        if (g_hide_everything)          /* forced refresh? */
        {
            g_top_line = std::max(g_art_line_num - g_g_line - 1, -1);
            return false;               /* let refresh do it all */
        }
    }
#ifdef DEBUG
    if (debug & DEB_INNERSRCH)
    {
        std::printf("Not far enough? %d <? %d + %d\n",g_artline,s_isrchline,g_gline);
        termdown(1);
    }
#endif
    if (g_art_line_num < s_isrchline + g_g_line)
    {
        return true;
    }
    return false;
}

/* On click:
 *    btn = 0 (left), 1 (middle), or 2 (right) + 4 if double-clicked;
 *    x = 0 to g_tc_COLS-1; y = 0 to g_tc_LINES-1;
 *    btn_clk = 0, 1, or 2 (no 4); x_clk = x; y_clk = y.
 * On release:
 *    btn = 3; x = released x; y = released y;
 *    btn_clk = click's 0, 1, or 2; x_clk = clicked x; y_clk = clicked y.
 */
void pager_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk)
{
    if (check_mousebar(btn, x,y, btn_clk, x_clk,y_clk))
    {
        return;
    }

    if (btn != 3)
    {
        return;
    }

    Article *ap = get_tree_artp(x_clk, y_clk + g_top_line + 1 + g_term_scrolled);
    if (ap && ap != get_tree_artp(x,y+g_top_line+1+g_term_scrolled))
    {
        return;
    }

    switch (btn_clk)
    {
    case 0:
        if (ap)
        {
            if (ap == g_artp)
            {
                return;
            }
            g_artp = ap;
            g_art = article_num(ap);
            g_reread = true;
            pushchar(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            pushchar(' ');
        }
        else if (g_top_line != -1)
        {
            pushchar('b');
        }
        break;

    case 1:
        if (ap)
        {
            select_subthread(ap, AUTO_KILL_NONE);
            s_special = true;
            s_slines = 1;
            pushchar(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            pushchar('\n');
        }
        else if (g_top_line != -1)
        {
            pushchar('B');
        }
        break;

    case 2:
        if (ap)
        {
            kill_subthread(ap, AUTO_KILL_NONE);
            s_special = true;
            s_slines = 1;
            pushchar(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            pushchar('n');
        }
        else
        {
            pushchar(Ctl('r'));
        }
        break;
    }
}
