/* art.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.


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

ArticleLine     g_highlight{-1};          // next line to be highlighted
ArticleLine     g_first_view{};           //
ArticlePosition g_raw_art_size{};         // size in bytes of raw article
ArticlePosition g_art_size{};             // size in bytes of article
char            g_art_line[LINE_BUF_LEN]; // place for article lines
int             g_g_line{};               // TODO: ArticleLine?
ArticlePosition g_inner_search{};         // g_art_pos of end of line we want to visit
ArticleLine     g_inner_light{};          // highlight position for g_inner_search or 0
char            g_hide_everything{};      // if set, do not write page now, ...but execute char when done with page
bool            g_reread{};               // consider current art temporarily unread?
bool            g_do_fseek{};             // should we back up in article file?
bool            g_old_subject{};          // not 1st art in subject thread
ArticleLine     g_top_line{-1};           // top line of current screen
bool            g_do_hiding{true};        // hide header lines with -h?
bool            g_is_mime{};              // process mime in an article?
bool            g_multimedia_mime{};      // images/audio to see/hear?
bool            g_rotate{};               // has rotation been requested?
std::string     g_prompt;                 // current prompt
char           *g_first_line{};           // s_special first line?
const char     *g_hide_line{};            // custom line hiding?
const char     *g_page_stop{};            // custom page terminator?
CompiledRegex   g_hide_compex{};          //
CompiledRegex   g_page_compex{};          //
bool            g_dont_filter_control{};  // -j

inline char *line_ptr(ArticlePosition pos)
{
    return g_art_buf + (pos - g_header_type[PAST_HEADER].min_pos).value_of();
}
inline ArticlePosition line_offset(char *ptr)
{
    return ArticlePosition{ptr - g_art_buf} + g_header_type[PAST_HEADER].min_pos;
}

// page_switch() return values
enum PageSwitchResult
{
    PS_NORM = 0,
    PS_ASK = 1,
    PS_RAISE = 2,
    PS_TO_END = 3
};

static bool            s_special{};         // is next page special length?
static int             s_special_lines{};   // how long to make page when special
static ArticlePosition s_restart{};         // if nonzero, the place where last line left off on line split
static ArticlePosition s_a_line_begin{};    // where in file current line began
static int             s_more_prompt_col{}; // non-zero when the more prompt is indented
static ArticleLine     s_i_search_line{};   // last line to display
static CompiledRegex   s_gcompex{};         // in article search pattern
static bool            s_first_page{};      // is this the 1st page of article?
static bool            s_continuation{};    // this line/header is being continued

static PageSwitchResult page_switch();

void art_init()
{
    init_compex(&s_gcompex);
}

DoArticleResult do_article()
{
    char* s;
    bool hide_this_line = false; // hidden header line?
    bool under_lining = false;   // are we underlining a word?
    char* buf_ptr = g_art_line;   // pointer to input buffer
    int out_pos;                  // column position of output
    static char prompt_buf[64];  // place to hold prompt
    bool notes_files = false;     // might there be notes files junk?
    MinorMode old_mode = g_mode;
    bool output_ok = true;

    if (g_data_source->flags & DF_REMOTE)
    {
        const ArticlePosition art_size{nntp_art_size()};
        g_raw_art_size = art_size;
        g_art_size = art_size;
    }
    else
    {
        stat_t art_stat{};
        if (fstat(fileno(g_art_fp),&art_stat))   // get article file stats
        {
            return DA_CLEAN;
        }
        if (!S_ISREG(art_stat.st_mode))
        {
            return DA_NORM;
        }
        const ArticlePosition art_size{art_stat.st_size};
        g_raw_art_size = art_size;
        g_art_size = art_size;
    }
    std::sprintf(prompt_buf, g_mouse_bar_cnt>3? "%%sEnd of art %ld (of %ld) %%s[%%s]"
        : "%%sEnd of article %ld (of %ld) %%s-- what next? [%%s]",
        g_art.value_of(), g_last_art.value_of());   // format prompt string
    g_prompt = prompt_buf;
    g_int_count = 0;            // interrupt count is 0
    s_first_page = (g_top_line < ArticleLine{});
    if (s_first_page != 0)
    {
        parse_header(g_art);
        mime_set_article();
        clear_art_buf();
        g_art_buf_seek = g_header_type[PAST_HEADER].min_pos;
        seek_art(g_header_type[PAST_HEADER].min_pos);
    }
    g_term_scrolled = 0;

    while (true) // for each page
    {
        if (g_threaded_group && g_max_tree_lines)
        {
            init_tree(); // init tree display
        }
        TRN_ASSERT(g_art == g_open_art);
        if (g_do_fseek)
        {
            parse_header(g_art);         // make sure header is ours
            if (!*g_art_buf)
            {
                mime_set_article();
                g_art_buf_seek = g_header_type[PAST_HEADER].min_pos;
            }
            g_art_pos = virtual_read(g_art_line_num);
            if (g_art_pos < ArticlePosition{})
            {
                g_art_pos = -g_art_pos; // labs(), anyone?
            }
            if (s_first_page)
            {
                g_art_pos = ArticlePosition{};
            }
            if (g_art_pos < g_header_type[PAST_HEADER].min_pos)
            {
                g_in_header = SOME_LINE;
                seek_art(g_header_type[PAST_HEADER].min_pos);
                seek_art_buf(g_header_type[PAST_HEADER].min_pos);
            }
            else
            {
                seek_art(g_art_buf_seek);
                seek_art_buf(g_art_pos);
            }
            g_do_fseek = false;
            s_restart = ArticlePosition{};
        }
        ArticleLine line_num{1};
#if 0 // This causes a bug (headers displayed twice sometimes when you press v then ^R)
        if (!g_do_hiding)
        {
            g_is_mime = false;
        }
#endif
        if (s_first_page)
        {
            if (g_first_line)
            {
                interp(g_art_line,sizeof g_art_line,g_first_line);
                line_num += ArticleLine{tree_puts(g_art_line,line_num+g_top_line,0)};
            }
            else
            {
                ArticleNum i;

                int selected = (g_curr_artp->flags & AF_SEL);
                int unseen = article_unread(g_art) ? 1 : 0;
                std::sprintf(g_art_line,"%s%s #%ld",g_newsgroup_name.c_str(),g_moderated.c_str(), g_art.value_of());
                if (g_selected_only)
                {
                    value_of(i) = g_selected_count - (unseen && selected);
                    std::sprintf(g_art_line+std::strlen(g_art_line)," (%ld + %ld more)",
                            i.value_of(),(long)g_newsgroup_ptr->to_read - g_selected_count
                                        - (!selected && unseen));
                }
                else if ((i = ArticleNum{g_newsgroup_ptr->to_read - unseen}) != ArticleNum{} //
                         || (!g_threaded_group && g_dm_count))
                {
                    std::sprintf(g_art_line+std::strlen(g_art_line),
                            " (%ld more)", i.value_of());
                }
                if (!g_threaded_group && g_dm_count)
                {
                    std::sprintf(g_art_line + std::strlen(g_art_line) - 1, " + %ld Marked to return)", (long) g_dm_count);
                }
                line_num += ArticleLine{tree_puts(g_art_line,line_num+g_top_line,0)};
            }
            start_header(g_art);
            g_force_last = false;        // we will have our day in court
            s_restart = ArticlePosition{};
            g_art_line_num = ArticleLine{};              // start counting lines
            g_art_pos = ArticlePosition{};
            virtual_write(g_art_line_num,g_art_pos); // remember pos in file
        }
        for (bool restart_color = true; // line_num already set
             g_inner_search                   ? (g_in_header || inner_more())
             : s_special                      ? (line_num.value_of() < s_special_lines)
             : (s_first_page && !g_in_header) ? (line_num < g_init_lines)
                                              : (line_num.value_of() < g_tc_LINES);
             ++line_num)
        {                               // for each line on page
            if (g_int_count)            // exit via interrupt?
            {
                newline();              // get to left margin
                g_int_count = 0;        // reset interrupt count
                set_mode(g_general_mode,old_mode);
                s_special = false;
                return DA_NORM;         // skip out of loops
            }
            if (s_restart)              // did not finish last line?
            {
                buf_ptr = line_ptr(s_restart); // then start again here
                s_restart = ArticlePosition{}; // and reset the flag
                s_continuation = true;
                if (restart_color && g_do_hiding && !g_in_header)
                {
                    maybe_set_color(buf_ptr, true);
                }
            }
            else if (g_in_header && *(buf_ptr = g_head_buf + g_art_pos.value_of()))
            {
                s_continuation = is_hor_space(*buf_ptr);
            }
            else
            {
                buf_ptr = read_art_buf(g_auto_view_inline);
                if (buf_ptr == nullptr)
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
                    s_continuation = maybe_set_color(buf_ptr, restart_color);
                }
                else
                {
                    s_continuation = false;
                }
            }
            s_a_line_begin = g_art_pos;      // remember where we began
            restart_color = false;
            if (g_in_header)
            {
                hide_this_line = parse_line(buf_ptr,g_do_hiding,hide_this_line);
                if (!g_in_header)
                {
                    line_num += ArticleLine{finish_tree(line_num+g_top_line)};
                    end_header();
                    seek_art(g_art_buf_seek);
                }
            }
            else if (notes_files && g_do_hiding && !s_continuation //
                     && *buf_ptr == '#' && std::isupper(buf_ptr[1]) //
                     && buf_ptr[2] == ':')
            {
                buf_ptr = read_art_buf(g_auto_view_inline);
                if (buf_ptr == nullptr)
                {
                    break;
                }
                for (s = buf_ptr; *s && *s != '\n' && *s != '!'; s++)
                {
                }
                if (*s != '!')
                {
                    read_art_buf(g_auto_view_inline);
                }
                mime_set_article();
                clear_art_buf();         // exclude notes files droppings
                g_header_type[PAST_HEADER].min_pos = tell_art();
                g_art_buf_seek = tell_art();
                hide_this_line = true;  // and do not print either
                notes_files = false;
            }
            if (g_hide_line && !s_continuation && execute(&g_hide_compex,buf_ptr))
            {
                hide_this_line = true;
            }
            if (g_in_header && g_do_hiding && (g_header_type[g_in_header].flags & HT_MAGIC))
            {
                switch (g_in_header)
                {
                case NEWSGROUPS_LINE:
                    s = std::strchr(buf_ptr, '\n');
                    if (s != nullptr)
                    {
                        *s = '\0';
                    }
                    hide_this_line = (std::strchr(buf_ptr,',') == nullptr)
                        && !strcmp(buf_ptr+12,g_newsgroup_name.c_str());
                    if (s != nullptr)
                    {
                        *s = '\n';
                    }
                    break;

                case EXPIR_LINE:
                    if (!(g_header_type[EXPIR_LINE].flags & HT_HIDE))
                    {
                        s = buf_ptr + g_header_type[EXPIR_LINE].length + 1;
                        hide_this_line = *s != ' ' || s[1] == '\n';
                    }
                    break;

                case FROM_LINE:
                    if ((s = std::strchr(buf_ptr,'\n')) != nullptr
                     && s-buf_ptr < sizeof g_art_line)
                    {
                        safe_copy(g_art_line,buf_ptr,s-buf_ptr+1);
                    }
                    else
                    {
                        safe_copy(g_art_line,buf_ptr,sizeof g_art_line);
                    }
                    s = extract_name(g_art_line + 6);
                    if (s != nullptr)
                    {
                        std::strcpy(g_art_line+6,s);
                        buf_ptr = g_art_line;
                    }
                    break;

                case DATE_LINE:
                    if (g_curr_artp->date != -1)
                    {
                        std::strncpy(g_art_line,buf_ptr,6);
                        std::strftime(g_art_line+6, (sizeof g_art_line)-6,
                                 get_val_const("LOCALTIMEFMT", LOCALTIMEFMT),
                                 std::localtime(&g_curr_artp->date));
                        buf_ptr = g_art_line;
                    }
                    break;
                }
            }
            if (g_in_header == SUBJ_LINE && g_do_hiding   //
                && (g_header_type[SUBJ_LINE].flags & HT_MAGIC)) // handle the subject
            {
                s = get_cached_line(g_artp, SUBJ_LINE, false);
                if (s && s_continuation)
                {
                    // continuation lines were already output
                    --line_num;
                }
                else
                {
                    int length = std::strlen(buf_ptr+1);
                    notes_files = in_string(&buf_ptr[length-10]," - (nf", true)!=nullptr;
                    ++g_art_line_num;
                    if (!s)
                    {
                        buf_ptr += (s_continuation ? 0 : 9);
                    }
                    else
                    {
                        buf_ptr = s;
                    }
                    // tree_puts(, ,1) underlines subject
                    line_num += ArticleLine{tree_puts(buf_ptr,line_num+g_top_line,1)-1};
                }
            }
            else if (hide_this_line && g_do_hiding)     // do not print line?
            {
                --line_num;                        // compensate for line_num++
                if (!g_in_header)
                {
                    hide_this_line = false;
                }
            }
            else if (g_in_header)
            {
                ++g_art_line_num;
                line_num += ArticleLine{tree_puts(buf_ptr,line_num+g_top_line,0)-1};
            }
            else                          // just a normal line
            {
                if (output_ok && g_erase_each_line)
                {
                    erase_line(false);
                }
                if (g_highlight == g_art_line_num)   // this line to be highlight?
                {
                    if (g_marking == STANDOUT)
                    {
#ifdef NO_FIREWORKS
                        if (g_erase_screen)
                        {
                            no_so_fire();
                        }
#endif
                        standout();
                    }
                    else
                    {
#ifdef NO_FIREWORKS
                        if (g_erase_screen)
                        {
                            no_ul_fire();
                        }
#endif
                        underline();
                        carriage_return();
                    }
                    if (*buf_ptr == '\n')
                    {
                        std::putchar(' ');
                    }
                }
                output_ok = !g_hide_everything;
                if (g_page_stop && !s_continuation && execute(&g_page_compex,buf_ptr))
                {
                    line_num = ArticleLine{32700};
                }
                for (out_pos = 0; out_pos < g_tc_COLS; )   // while line has room
                {
                    if (at_norm_char(buf_ptr))       // normal char?
                    {
                        if (*buf_ptr == '_')
                        {
                            if (buf_ptr[1] == '\b')
                            {
                                if (output_ok && !under_lining && g_highlight != g_art_line_num)
                                {
                                    under_lining = true;
                                    if (g_tc_UG)
                                    {
                                        if (buf_ptr != g_buf && buf_ptr[-1] == ' ')
                                        {
                                            out_pos--;
                                            backspace();
                                        }
                                    }
                                    underline();
                                }
                                buf_ptr += 2;
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
                                    out_pos++;
                                    if (*buf_ptr == ' ')
                                    {
                                        goto skip_put;
                                    }
                                }
                            }
                        }
                        // handle rot-13 if wanted
                        if (g_rotate && !g_in_header && std::isalpha(*buf_ptr))
                        {
                            if (output_ok)
                            {
                                if ((*buf_ptr & 31) <= 13)
                                {
                                    std::putchar(*buf_ptr + 13);
                                }
                                else
                                {
                                    std::putchar(*buf_ptr - 13);
                                }
                            }
                            out_pos++;
                        }
                        else
                        {
                            int i;
#ifdef USE_UTF_HACK
                            if (out_pos + visual_width_at(buf_ptr) > g_tc_COLS)   // will line overflow?
                            {
                                newline();
                                out_pos = 0;
                                ++line_num;
                            }
                            i = put_char_adv(&buf_ptr, output_ok);
                            buf_ptr--;
#else // !USE_UTF_HACK
                            i = putsubstchar(*bufptr, g_tc_COLS - outpos, outputok);
#endif // USE_UTF_HACK
                            if (i < 0)
                            {
                                out_pos += -i - 1;
                                break;
                            }
                            out_pos += i;
                        }
                        if (*g_tc_UC && ((g_highlight == g_art_line_num && g_marking == STANDOUT) || under_lining))
                        {
                            backspace();
                            underchar();
                        }
skip_put:
                        buf_ptr++;
                    }
                    else if (at_nl(*buf_ptr) || !*buf_ptr)      // newline?
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
                        if (output_ok)
                        {
                            newline();
                        }
                        s_restart = ArticlePosition{};
                        out_pos = 1000;  // signal normal \n
                    }
                    else if (*buf_ptr == '\t')   // tab?
                    {
                        int inc_pos =  8 - out_pos % 8;
                        if (output_ok)
                        {
                            if (g_tc_GT)
                            {
                                std::putchar(*buf_ptr);
                            }
                            else
                            {
                                while (inc_pos--)
                                {
                                    std::putchar(' ');
                                }
                            }
                        }
                        buf_ptr++;
                        out_pos += 8 - out_pos % 8;
                    }
                    else if (*buf_ptr == '\f')   // form feed?
                    {
                        if (out_pos+2 > g_tc_COLS)
                        {
                            break;
                        }
                        if (output_ok)
                        {
                            std::fputs("^L", stdout);
                        }
                        if (buf_ptr == line_ptr(s_a_line_begin) && g_highlight != g_art_line_num)
                        {
                            line_num = ArticleLine{32700};
                            // how is that for a magic number?
                        }
                        buf_ptr++;
                        out_pos += 2;
                    }
                    else                  // other control char
                    {
                        if (g_dont_filter_control)
                        {
                            if (output_ok)
                            {
                                std::putchar(*buf_ptr);
                            }
                            out_pos++;
                        }
                        else if (*buf_ptr != '\r' || buf_ptr[1] != '\n')
                        {
                            if (out_pos+2 > g_tc_COLS)
                            {
                                break;
                            }
                            if (output_ok)
                            {
                                std::putchar('^');
                                if (g_highlight == g_art_line_num && *g_tc_UC && g_marking == STANDOUT)
                                {
                                    backspace();
                                    underchar();
                                    std::putchar((*buf_ptr & 0x7F) ^ 0x40);
                                    backspace();
                                    underchar();
                                }
                                else
                                {
                                    std::putchar((*buf_ptr & 0x7F) ^ 0x40);
                                }
                            }
                            out_pos += 2;
                        }
                        buf_ptr++;
                    }
                } // end of column loop

                if (out_pos < 1000)      // did line overflow?
                {
                    s_restart = line_offset(buf_ptr);// restart here next time
                    if (output_ok)
                    {
                        if (!g_tc_AM || g_tc_XN || out_pos < g_tc_COLS)
                        {
                            newline();
                        }
                        else
                        {
                            g_term_line++;
                        }
                    }
                    if (at_nl(*buf_ptr))         // skip the newline
                    {
                        s_restart = ArticlePosition{};
                    }
                }

                // handle normal end of output line formalities

                if (g_highlight == g_art_line_num)
                {
                    if (g_marking == STANDOUT)  // were we highlighting line?
                    {
                        un_standout();
                    }
                    else
                    {
                        un_underline();
                    }
                    carriage_return();
                    g_highlight = ArticleLine{-1};   // no more we are
                    // in case terminal highlighted rest of line earlier
                    // when we did an eol with highlight turned on:
                    if (g_erase_each_line)
                    {
                        erase_eol();
                    }
                }
                ++g_art_line_num;    // count the line just printed
                            // did we just scroll top line off?
                            // then recompute top line #
                g_top_line = std::max(g_art_line_num - ArticleLine{g_tc_LINES} + ArticleLine{1}, g_top_line);
            }

            // determine actual position in file

            if (s_restart)      // stranded somewhere in the buffer?
            {
                g_art_pos += s_restart - s_a_line_begin;
            }
            else if (g_in_header)
            {
                g_art_pos = ArticlePosition{std::strchr(g_head_buf + g_art_pos.value_of(), '\n') - g_head_buf + 1};
            }
            else
            {
                g_art_pos = g_art_buf_pos + g_header_type[PAST_HEADER].min_pos;
            }
            virtual_write(g_art_line_num,g_art_pos); // remember pos in file
        } // end of line loop

        g_inner_search = ArticlePosition{};
        if (g_hide_everything)
        {
            *g_buf = g_hide_everything;
            g_hide_everything = 0;
            goto fake_command;
        }
        if (line_num >= ArticleLine{32700})   // did last line have form feed?
        {
            virtual_write(g_art_line_num - ArticleLine{1}, -virtual_read(g_art_line_num - ArticleLine{1}));
                                // remember by negating pos in file
        }

        s_special = false;      // end of page, so reset page length
        s_first_page = false;    // and say it is not 1st time thru
        g_highlight = ArticleLine{-1};

        // extra loop bombout

        if (g_art_size < ArticlePosition{} && (g_raw_art_size = nntp_art_size()) >= ArticlePosition{})
        {
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
        }
recheck_pager:
        if (g_do_hiding && g_art_buf_pos == g_art_buf_len)
        {
            // If we're filtering we need to figure out if any
            // remaining text is going to vanish or not.
            ArticlePosition seek_pos = g_art_buf_pos + g_header_type[PAST_HEADER].min_pos;
            read_art_buf(false);
            seek_art_buf(seek_pos);
        }
        if (g_art_pos == g_art_size)  // did we just now reach EOF?
        {
            color_default();
            set_mode(g_general_mode,old_mode);
            return DA_NORM;     // avoid --MORE--(100%)
        }

// not done with this article, so pretend we are a pager

reask_pager:
        if (g_term_line >= g_tc_LINES)
        {
            g_term_scrolled += g_term_line - g_tc_LINES + 1;
            g_term_line = g_tc_LINES-1;
        }
        s_more_prompt_col = g_term_col;

        unflush_output();       // disable any ^O in effect
         maybe_eol();
        color_default();
        if (g_art_size < ArticlePosition{})
        {
            std::strcpy(g_cmd_buf, "?");
        }
        else
        {
            std::sprintf(g_cmd_buf, "%ld", (long) (g_art_pos.value_of() * 100 / g_art_size.value_of()));
        }
        std::sprintf(g_buf,"%s--MORE--(%s%%)",current_char_subst(),g_cmd_buf);
        out_pos = g_term_col + std::strlen(g_buf);
        draw_mouse_bar(g_tc_COLS - (g_term_line == g_tc_LINES-1? out_pos+5 : 0), true);
        color_string(COLOR_MORE,g_buf);
        std::fflush(stdout);
        g_term_col = out_pos;
        eat_typeahead();
#ifdef DEBUG
        if (debug & DEB_CHECKPOINTING)
        {
            std::printf("(%d %d %d)",g_check_count,line_num,g_art_line_num);
            std::fflush(stdout);
        }
#endif
        if (g_check_count >= g_do_check_when && line_num.value_of() == g_tc_LINES &&
            (g_art_line_num > ArticleLine{40} || g_check_count >= g_do_check_when + 10))
        {
                            // while he is reading a whole page
                            // in an article he is interested in
            g_check_count = 0;
            checkpoint_newsrcs();       // update all newsrcs
            update_thread_kill_file();
        }
        cache_until_key();
        if (g_art_size < ArticlePosition{} && (g_raw_art_size = nntp_art_size()) >= ArticlePosition{})
        {
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
            goto_xy(s_more_prompt_col,g_term_line);
            goto recheck_pager;
        }
        set_mode(g_general_mode,MM_PAGER);
        get_cmd(g_buf);
        if (errno)
        {
            if (g_tc_LINES < 100 && !g_int_count)
            {
                *g_buf = '\f'; // on CONT fake up refresh
            }
            else
            {
                *g_buf = 'q';   // on INTR or paper just quit
            }
        }
        erase_line(g_erase_screen && g_erase_each_line);

    fake_command:               // used by g_inner_search
        color_default();
        g_output_chase_phrase = true;

        // parse and process pager command

        if (g_mouse_bar_cnt)
        {
            clear_rest();
        }
        switch (page_switch())
        {
        case PS_ASK:  // reprompt "--MORE--..."
            goto reask_pager;

        case PS_RAISE:        // reparse on article level
            set_mode(g_general_mode,old_mode);
            return DA_RAISE;

        case PS_TO_END:        // fast pager loop exit
            set_mode(g_general_mode,old_mode);
            return DA_TO_END;

        case PS_NORM:         // display more article
            break;
        }
    } // end of page loop
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

// process pager commands

static PageSwitchResult page_switch()
{
    char* s;

    switch (*g_buf)
    {
    case '!':                 // shell escape
        escapade();
        return PS_ASK;

    case Ctl('i'):
    {
        ArticleLine i = g_art_line_num;
        g_g_line = 3;
        s = line_ptr(s_a_line_begin);
        while (at_nl(*s) && i >= g_top_line)
        {
            ArticlePosition pos = virtual_read(--i);
            if (pos < ArticlePosition{})
            {
                pos = -pos;
            }
            if (pos < g_header_type[PAST_HEADER].min_pos)
            {
                break;
            }
            seek_art_buf(pos);
            s = read_art_buf(false);
            if (s == nullptr)
            {
                s = line_ptr(s_a_line_begin);
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

    case 'g':         // in-article search
        if (!finish_command(false))// get rest of command
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
                            // compile regular expression
            std::printf("\n%s\n", s);
            term_down(2);
            return PS_ASK;
        }
        erase_line(false);      // erase the prompt
        // FALL THROUGH

caseG:
    case 'G':
    {
        ArticlePosition start_where;
        bool success;
        char* nl_ptr;
        char ch;

        if (g_g_line < 0 || g_g_line > g_tc_LINES-2)
        {
            g_g_line = g_tc_LINES - 2;
        }
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("Start here? %d  >=? %d\n",g_topline + g_gline + 1,g_artline);
            term_down(1);
        }
#endif
        if (*g_buf == Ctl('i') || g_top_line + ArticleLine{g_g_line + 1} >= g_art_line_num)
        {
            start_where = g_art_pos;
                        // in case we had a line wrap
        }
        else
        {
            start_where = virtual_read(g_top_line + ArticleLine{g_g_line + 1});
            if (start_where < ArticlePosition{})
            {
                start_where = -start_where;
            }
        }
        start_where = std::max(start_where, g_header_type[PAST_HEADER].min_pos);
        seek_art_buf(start_where);
        g_inner_light = ArticleLine{};
        g_inner_search = ArticlePosition{}; // assume not found
        while ((s = read_art_buf(false)) != nullptr)
        {
            nl_ptr = std::strchr(s, '\n');
            if (nl_ptr != nullptr)
            {
                ch = *++nl_ptr;
                *nl_ptr = '\0';
            }
#ifdef DEBUG
            if (debug & DEB_INNERSRCH)
            {
                std::printf("Test %s\n",s);
            }
#endif
            success = execute(&s_gcompex,s) != nullptr;
            if (nl_ptr)
            {
                *nl_ptr = ch;
            }
            if (success)
            {
                g_inner_search = g_art_buf_pos + g_header_type[PAST_HEADER].min_pos;
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
            std::printf("On page? %ld <=? %ld\n",(long)g_inner_search,(long)g_art_pos);
            term_down(1);
        }
#endif
        if (g_inner_search <= g_art_pos)          // already on page?
        {
            if (g_inner_search < g_art_pos)
            {
                g_art_line_num = g_top_line + ArticleLine{1};
                while (virtual_read(g_art_line_num) < g_inner_search)
                {
                    ++g_art_line_num;
                }
            }
            g_highlight = g_art_line_num - ArticleLine{1};
#ifdef DEBUG
            if (debug & DEB_INNERSRCH)
            {
                std::printf("@ %d\n",g_highlight);
                term_down(1);
            }
#endif
            g_top_line = g_highlight - ArticleLine{g_g_line};
            g_top_line = std::max(g_top_line, ArticleLine{-1});
            *g_buf = '\f';              // fake up a refresh
            g_inner_search = ArticlePosition{};
            return page_switch();
        }
        g_do_fseek = true;              // who knows how many lines it is?
        g_hide_everything = '\f';
        return PS_NORM;
    }

    case '\n':                        // one line down
    case '\r':
        s_special = true;
        s_special_lines = 2;
        return PS_NORM;

    case 'X':
        g_rotate = !g_rotate;
        // FALL THROUGH

    case 'l':
    case '\f':                // refresh screen
refresh_screen:
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("Topline = %d",g_top_line);
            std::fgets(g_buf, sizeof g_buf, stdin);
        }
#endif
        clear();
        g_do_fseek = true;
        g_art_line_num = g_top_line;
        g_art_line_num = std::max(g_art_line_num, ArticleLine{});
        s_first_page = (g_top_line < ArticleLine{});
        return PS_NORM;

    case Ctl('e'):
        if (g_art_size < ArticlePosition{})
        {
            nntp_finish_body(FB_OUTPUT);
            g_raw_art_size = nntp_art_size();
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
        }
        if (g_do_hiding)
        {
            seek_art_buf(g_art_size);
            seek_art_buf(g_art_pos);
        }
        g_top_line = g_art_line_num;
        g_inner_light = g_art_line_num - ArticleLine{1};
        g_inner_search = g_art_size;
        g_g_line = 0;
        g_hide_everything = 'b';
        return PS_NORM;

    case 'B':         // one line up
        if (g_top_line < ArticleLine{})
        {
            break;
        }
        if (*g_tc_IL && *g_tc_HO)
        {
            home_cursor();
            insert_line();
            carriage_return();
            ArticlePosition pos = virtual_read(g_top_line - ArticleLine{1});
            if (pos < ArticlePosition{})
            {
                pos = -pos;
            }
            if (pos >= g_header_type[PAST_HEADER].min_pos)
            {
                seek_art_buf(pos);
                s = read_art_buf(false);
                if (s != nullptr)
                {
                    g_art_pos = virtual_read(g_top_line);
                    if (g_art_pos < ArticlePosition{})
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
                    --g_top_line;
                    g_art_pos = virtual_read(--g_art_line_num);
                    if (g_art_pos < ArticlePosition{})
                    {
                        g_art_pos = -g_art_pos;
                    }
                    seek_art_buf(g_art_pos);
                    s_a_line_begin = virtual_read(g_art_line_num - ArticleLine{1});
                    if (s_a_line_begin < ArticlePosition{})
                    {
                        s_a_line_begin = -s_a_line_begin;
                    }
                    goto_xy(0, (g_art_line_num - g_top_line).value_of());
                    erase_line(false);
                    return PS_ASK;
                }
            }
        }
        // FALL THROUGH

    case 'b':
    case Ctl('b'):    // back up a page
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

        g_do_fseek = true;      // reposition article file
        if (*g_buf == 'B')
        {
            target = g_top_line - ArticleLine{1};
        }
        else
        {
            target = g_top_line - ArticleLine{g_tc_LINES - 2};
            if (g_marking && (g_marking_areas & BACK_PAGE_MARKING))
            {
                g_highlight = g_top_line;
            }
        }
        g_art_line_num = g_top_line;
        if (g_art_line_num >= ArticleLine{})
        {
            do
            {
                --g_art_line_num;
            } while (g_art_line_num >= ArticleLine{} && g_art_line_num > target &&
                     virtual_read(g_art_line_num - ArticleLine{1}) >= ArticlePosition{});
        }
        g_top_line = g_art_line_num;  // remember top line of screen
                                // (line # within article file)
        g_art_line_num = std::max(g_art_line_num, ArticleLine{});
        s_first_page = (g_top_line < ArticleLine{});
        return PS_NORM;
      }

    case 'H':         // help
        help_page();
        return PS_ASK;

    case 't':         // output thread data
        g_page_line = 1;
        entire_tree(g_curr_artp);
        return PS_ASK;

    case '_':
        if (!finish_dbl_char())
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

    case '\0':                // treat break as 'n'
        *g_buf = 'n';
        // FALL THROUGH

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
        mark_as_read(g_artp);   // mark article as read
        // FALL THROUGH

    case 'U': case ',':
    case '<': case '>':
    case '[': case ']':
    case '{': case '}':
    case '(': case ')':
    case ':':
    case '+':
    case Ctl('v'):            // verify crypto signature
    case ';':                 // enter article scan mode
    case '"':                 // append to local score file
    case '\'':                // score command
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
                   g_default_cmd.c_str());  // print prompt, whatever it is
            color_pop();        // of COLOR_CMD
            std::putchar(' ');
            std::fflush(stdout);
        }
        return PS_RAISE;        // and pretend we were at end

    case 'd':         // half page
    case Ctl('d'):
        s_special = true;
        s_special_lines = g_tc_LINES / 2 + 1;
        // no divide-by-zero, thank you
        if (g_tc_LINES > 2 && (g_tc_LINES & 1) &&
            g_art_line_num % ArticleLine{g_tc_LINES - 2} >= ArticleLine{g_tc_LINES / 2 - 1})
        {
            s_special_lines++;
        }
        goto go_forward;

    case 'y':
    case ' ': // continue current article
        if (g_erase_screen)     // -e?
        {
            if (g_erase_each_line)
            {
                home_cursor();
            }
            else
            {
                clear();        // clear screen
            }
            std::fflush(stdout);
        }
        else
        {
            s_special = true;
            s_special_lines = g_tc_LINES;
        }
go_forward:
          if (*line_ptr(s_a_line_begin) != '\f' && (!g_page_stop || s_continuation || !execute(&g_page_compex, line_ptr(s_a_line_begin))))
          {
              if (!s_special //
                  || (g_marking && (*g_buf != 'd' || (g_marking_areas & HALF_PAGE_MARKING))))
              {
                s_restart = s_a_line_begin;
                --g_art_line_num;     // restart this line
                g_art_pos = s_a_line_begin;
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
                  s_special_lines--;
              }
        }
        return PS_NORM;

    case 'i':
        g_auto_view_inline = !g_auto_view_inline;
        if (g_auto_view_inline != 0)
        {
            g_first_view = ArticleLine{};
        }
        std::printf("\nAuto-View inlined mime is %s\n", g_auto_view_inline? "on" : "off");
        term_down(2);
        break;

    case 'q': // quit this article?
        return PS_TO_END;

    default:
        std::fputs(g_h_for_help,stdout);
        term_down(1);
        settle_down();
        return PS_ASK;
    }
    return PS_ASK;
}

bool inner_more()
{
    if (g_art_pos < g_inner_search)               // not even on page yet?
    {
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("Not on page %ld < %ld\n",(long)g_artpos,(long)g_innersearch);
        }
#endif
        return true;
    }
    if (g_art_pos == g_inner_search)      // just got onto page?
    {
        s_i_search_line = g_art_line_num;        // remember first line after
        if (g_inner_light)
        {
            g_highlight = g_inner_light;
        }
        else
        {
            g_highlight = g_art_line_num - ArticleLine{1};
        }
#ifdef DEBUG
        if (debug & DEB_INNERSRCH)
        {
            std::printf("There it is %ld = %ld, %d @ %d\n",(long)g_artpos,
                (long)g_innersearch,g_hide_everything,g_highlight);
            term_down(1);
        }
#endif
        if (g_hide_everything)          // forced refresh?
        {
            g_top_line = std::max(g_art_line_num - ArticleLine{g_g_line + 1}, ArticleLine{-1});
            return false;               // let refresh do it all
        }
    }
#ifdef DEBUG
    if (debug & DEB_INNERSRCH)
    {
        std::printf("Not far enough? %d <? %d + %d\n",g_artline,s_isrchline,g_gline);
        term_down(1);
    }
#endif
    if (g_art_line_num < s_i_search_line + ArticleLine{g_g_line})
    {
        return true;
    }
    return false;
}

// On click:
//    btn = 0 (left), 1 (middle), or 2 (right) + 4 if double-clicked;
//    x = 0 to g_tc_COLS-1; y = 0 to g_tc_LINES-1;
//    btn_clk = 0, 1, or 2 (no 4); x_clk = x; y_clk = y.
// On release:
//    btn = 3; x = released x; y = released y;
//    btn_clk = click's 0, 1, or 2; x_clk = clicked x; y_clk = clicked y.
//
void pager_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk)
{
    if (check_mouse_bar(btn, x,y, btn_clk, x_clk,y_clk))
    {
        return;
    }

    if (btn != 3)
    {
        return;
    }

    Article *ap = get_tree_artp(x_clk, y_clk + g_top_line.value_of() + 1 + g_term_scrolled);
    if (ap && ap != get_tree_artp(x,y+g_top_line.value_of()+1+g_term_scrolled))
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
            push_char(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            push_char(' ');
        }
        else if (g_top_line != ArticleLine{-1})
        {
            push_char('b');
        }
        break;

    case 1:
        if (ap)
        {
            select_sub_thread(ap, AUTO_KILL_NONE);
            s_special = true;
            s_special_lines = 1;
            push_char(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            push_char('\n');
        }
        else if (g_top_line != ArticleLine{-1})
        {
            push_char('B');
        }
        break;

    case 2:
        if (ap)
        {
            kill_sub_thread(ap, AUTO_KILL_NONE);
            s_special = true;
            s_special_lines = 1;
            push_char(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            push_char('n');
        }
        else
        {
            push_char(Ctl('r'));
        }
        break;
    }
}
