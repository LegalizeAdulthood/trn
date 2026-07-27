/* art.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/art-internal.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/help.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/mime.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngstuff.h>
#include <trn/nntp.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rt-wumpus.h>
#include <trn/rthread.h>
#include <trn/search.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

ArticleLine     g_highlight{-1};          // next line to be highlighted
ArticleLine     g_first_view{};           //
ArticlePosition g_raw_art_size{};         // size in bytes of raw article
ArticlePosition g_art_size{};             // size in bytes of article
std::string     g_art_line;               // place for article lines
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
std::string     g_first_line;             // s_special first line?
std::string     g_hide_line;              // custom line hiding?
std::string     g_page_stop;              // custom page terminator?
CompiledRegex   g_hide_compex{};          //
CompiledRegex   g_page_compex{};          //
bool            g_dont_filter_control{};  // -j

inline char *line_ptr(ArticlePosition pos)
{
    return g_art_buf + (pos - g_header_type[PAST_HEADER].min_pos).value_of();
}
inline ArticlePosition line_offset(const char *line_begin, const char *ptr, ArticlePosition begin_pos)
{
    return begin_pos + ArticlePosition{static_cast<long>(ptr - line_begin)};
}

static bool            s_special{};         // is next page special length?
static int             s_special_lines{};   // how long to make page when special
static ArticlePosition s_restart{};         // if nonzero, the place where last line left off on line split
static ArticlePosition s_a_line_begin{};    // where in file current line began
static int             s_more_prompt_col{}; // non-zero when the more prompt is indented
static ArticleLine     s_i_search_line{};   // last line to display
static CompiledRegex   s_gcompex{};         // in article search pattern
static bool            s_first_page{};      // is this the 1st page of article?
static bool            s_continuation{};    // this line/header is being continued

static std::string finish_pager_command(std::string_view command, bool donewline);
static std::string finish_pager_dbl_command(std::string_view command);
static bool        maybe_set_color(const char *line_begin, const char *cp, bool back_search);
static bool        inner_more();
static bool        pager_command_needs_completion(std::string_view command);

void art_init()
{
    s_gcompex.init_compex();
}

DoArticleResult do_article()
{
    std::string article_command;
    return do_article(article_command);
}

DoArticleResult do_article(std::string &article_command)
{
    char* s;
    bool hide_this_line = false; // hidden header line?
    bool under_lining = false;   // are we underlining a word?
    char* buf_ptr = g_art_line.data(); // pointer to input buffer
    const char *buf_begin = g_art_line.data();
    std::string body_line;
    std::string from_line;
    std::string date_line;
    std::string pager_command;
    int out_pos;                  // column position of output
    bool notes_files = false;     // might there be notes files junk?
    MinorMode old_mode = g_mode;
    bool output_ok = true;

    article_command.clear();

    if (g_data_source->m_flags & DF_REMOTE)
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
    if (g_mouse_bar_cnt > 3)
    {
        g_prompt = fmt::format("%sEnd of art {} (of {}) %s[%s]", g_art.value_of(), g_last_art.value_of());
    }
    else
    {
        g_prompt =
            fmt::format("%sEnd of article {} (of {}) %s-- what next? [%s]", g_art.value_of(), g_last_art.value_of());
    }
    g_int_count = 0;            // interrupt count is 0
    s_first_page = g_top_line < 0;
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
            if (g_art_pos < 0)
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
        if (s_first_page)
        {
            if (!g_first_line.empty())
            {
                const std::string first_line = do_interp(g_first_line);
                line_num += tree_puts(first_line, line_num + g_top_line, 0);
            }
            else
            {
                ArticleNum i;

                int selected = (g_curr_artp->m_flags & AF_SEL);
                int unseen = article_unread(g_art) ? 1 : 0;
                std::string title_line;
                bool        has_title_suffix = false;
                title_line.reserve(LINE_BUF_LEN);
                fmt::format_to(std::back_inserter(title_line), "{}{} #{}", g_newsgroup_name, g_moderated,
                               g_art.value_of());
                if (g_selected_only)
                {
                    value_of(i) = g_selected_count - (unseen && selected);
                    const long unread_count = static_cast<long>(g_newsgroup_ptr->m_to_read) - g_selected_count //
                                              - (!selected && unseen);
                    fmt::format_to(std::back_inserter(title_line), " ({} + {} more", i.value_of(), unread_count);
                    has_title_suffix = true;
                }
                else if ((i = ArticleNum{g_newsgroup_ptr->m_to_read - unseen}) != 0 //
                         || (!g_threaded_group && g_dm_count))
                {
                    fmt::format_to(std::back_inserter(title_line), " ({} more", i.value_of());
                    has_title_suffix = true;
                }
                if (!g_threaded_group && g_dm_count)
                {
                    fmt::format_to(std::back_inserter(title_line), " + {} Marked to return",
                                   static_cast<long>(g_dm_count));
                }
                if (has_title_suffix)
                {
                    title_line += ')';
                }
                line_num += tree_puts(title_line, line_num + g_top_line, 0);
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
                buf_begin = body_line.data();
                buf_ptr = body_line.data() + (s_restart - s_a_line_begin).value_of(); // then start again here
                s_restart = ArticlePosition{};                                        // and reset the flag
                s_continuation = true;
                if (restart_color && g_do_hiding && !g_in_header)
                {
                    maybe_set_color(buf_begin, buf_ptr, true);
                }
            }
            else if (g_in_header && *(buf_ptr = g_head_buf.data() + g_art_pos.value_of()))
            {
                buf_begin = buf_ptr;
                s_continuation = is_hor_space(*buf_ptr);
            }
            else
            {
                if (!read_art_buf(body_line, g_auto_view_inline))
                {
                    s_special = false;
                    if (g_inner_search)
                    {
                        (void) inner_more();
                    }
                    break;
                }
                buf_begin = body_line.data();
                buf_ptr = body_line.data();
                if (g_do_hiding && !g_in_header)
                {
                    s_continuation = maybe_set_color(buf_begin, buf_ptr, restart_color);
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
                    line_num += finish_tree(line_num+g_top_line);
                    end_header();
                    seek_art(g_art_buf_seek);
                }
            }
            else if (notes_files && g_do_hiding && !s_continuation //
                     && *buf_ptr == '#' && std::isupper(buf_ptr[1]) //
                     && buf_ptr[2] == ':')
            {
                if (!read_art_buf(body_line, g_auto_view_inline))
                {
                    break;
                }
                buf_begin = body_line.data();
                buf_ptr = body_line.data();
                for (s = buf_ptr; *s && *s != '\n' && *s != '!'; s++)
                {
                }
                if (*s != '!')
                {
                    std::string ignored_line;
                    (void) read_art_buf(ignored_line, g_auto_view_inline);
                }
                mime_set_article();
                clear_art_buf();         // exclude notes files droppings
                g_header_type[PAST_HEADER].min_pos = tell_art();
                g_art_buf_seek = tell_art();
                hide_this_line = true;  // and do not print either
                notes_files = false;
            }
            if (!g_hide_line.empty() && !s_continuation && g_hide_compex.execute(buf_ptr))
            {
                hide_this_line = true;
            }
            if (g_in_header && g_do_hiding && (g_header_type[g_in_header].flags & HT_MAGIC))
            {
                switch (g_in_header)
                {
                case NEWSGROUPS_LINE:
                {
                    std::string_view newsgroups{buf_ptr};
                    newsgroups = newsgroups.substr(0, newsgroups.find('\n'));
                    newsgroups.remove_prefix(std::min(newsgroups.size(), sizeof("Newsgroups: ") - 1));
                    hide_this_line = newsgroups.find(',') == std::string_view::npos && newsgroups == g_newsgroup_name;
                    break;
                }

                case EXPIR_LINE:
                    if (!(g_header_type[EXPIR_LINE].flags & HT_HIDE))
                    {
                        s = buf_ptr + g_header_type[EXPIR_LINE].length + 1;
                        hide_this_line = *s != ' ' || s[1] == '\n';
                    }
                    break;

                case FROM_LINE:
                {
                    from_line.reserve(LINE_BUF_LEN);
                    const std::string_view from_text{buf_ptr};
                    from_line = from_text.substr(0, from_text.find('\n'));
                    const std::string name{extract_name(std::string_view{from_line}.substr(6))};
                    if (!name.empty())
                    {
                        from_line.resize(6);
                        from_line += name;
                        buf_begin = from_line.data();
                        buf_ptr = from_line.data();
                    }
                    break;
                }

                case DATE_LINE:
                    if (g_curr_artp->m_date != -1)
                    {
                        date_line.clear();
                        date_line.reserve(LINE_BUF_LEN);
                        date_line.append(buf_ptr, 6);
                        date_line.resize(LINE_BUF_LEN);
                        const std::string local_time_format = get_env_var("LOCALTIMEFMT", LOCALTIME_FMT);
                        const std::size_t date_length =
                            std::strftime(date_line.data() + 6, date_line.size() - 6, local_time_format.c_str(),
                                          std::localtime(&g_curr_artp->m_date));
                        date_line.resize(6 + date_length);
                        buf_begin = date_line.data();
                        buf_ptr = date_line.data();
                    }
                    break;
                }
            }
            if (g_in_header == SUBJ_LINE && g_do_hiding   //
                && (g_header_type[SUBJ_LINE].flags & HT_MAGIC)) // handle the subject
            {
                const std::string_view cached_subj = g_artp->get_cached_line_view(SUBJ_LINE, false);
                if (!cached_subj.empty() && s_continuation)
                {
                    // continuation lines were already output
                    --line_num;
                }
                else
                {
                    std::string_view subject_text{buf_ptr};
                    notes_files = subject_text.size() >= 10 &&
                                  in_string(subject_text.substr(subject_text.size() - 10), " - (nf", true);
                    ++g_art_line_num;
                    if (cached_subj.empty())
                    {
                        subject_text.remove_prefix(s_continuation ? 0 : 9);
                    }
                    else
                    {
                        subject_text = cached_subj;
                    }
                    // tree_puts(, ,1) underlines subject
                    line_num += line_before(tree_puts(subject_text, line_num + g_top_line, 1));
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
                line_num += line_before(tree_puts(buf_ptr, line_num + g_top_line, 0));
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
                if (!g_page_stop.empty() && !s_continuation && g_page_compex.execute(buf_ptr))
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
                            const char *next_buf_ptr = buf_ptr;
                            i = put_char_adv(&next_buf_ptr, output_ok);
                            buf_ptr += next_buf_ptr - buf_ptr;
                            buf_ptr--;
#else // !USE_UTF_HACK
                            i = putsubstchar(*bufptr, g_tc_COLS - out_pos, outputok);
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
                        if (g_debug & DEB_INNERSRCH && out_pos < g_tc_COLS - 6)
                        {
                            standout();
                            std::printf("%4d",g_art_line_num.value_of());
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
                        if (buf_ptr == buf_begin && g_highlight != g_art_line_num)
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
                    s_restart = line_offset(buf_begin, buf_ptr, s_a_line_begin); // restart here next time
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
                g_top_line = std::max(line_after(g_art_line_num) - ArticleLine{g_tc_LINES}, g_top_line);
            }

            // determine actual position in file

            if (s_restart)      // stranded somewhere in the buffer?
            {
                g_art_pos += s_restart - s_a_line_begin;
            }
            else if (g_in_header)
            {
                const std::string_view::size_type line_end = std::string_view{g_head_buf}.find(
                    '\n', static_cast<std::string_view::size_type>(g_art_pos.value_of()));
                g_art_pos = ArticlePosition{static_cast<long>(line_end + 1)};
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
            pager_command = g_hide_everything;
            g_hide_everything = 0;
            goto fake_command;
        }
        if (line_num >= 32700)   // did last line have form feed?
        {
            // remember by negating pos in file
            virtual_write(line_before(g_art_line_num), -virtual_read(line_before(g_art_line_num)));
        }

        s_special = false;      // end of page, so reset page length
        s_first_page = false;    // and say it is not 1st time thru
        g_highlight = ArticleLine{-1};

        // extra loop bombout

        if (g_art_size < 0 && (g_raw_art_size = nntp_art_size()) >= 0)
        {
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
        }
recheck_pager:
        if (g_do_hiding && g_art_buf_pos == g_art_buf_len)
        {
            // If we're filtering we need to figure out if any
            // remaining text is going to vanish or not.
            ArticlePosition seek_pos = g_art_buf_pos + g_header_type[PAST_HEADER].min_pos;
            std::string     remaining_line;
            (void) read_art_buf(remaining_line, false);
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
        {
            const std::string percent =
                g_art_size < 0 ? "?" : std::to_string(g_art_pos.value_of() * 100 / g_art_size.value_of());
            const std::string more_prompt = fmt::format("{}--MORE--({}%)", current_char_subst(), percent);
            out_pos = g_term_col + static_cast<int>(more_prompt.size());
            draw_mouse_bar(g_tc_COLS - (g_term_line == g_tc_LINES - 1 ? out_pos + 5 : 0), true);
            color_string(COLOR_MORE, more_prompt);
        }
        std::fflush(stdout);
        g_term_col = out_pos;
        eat_typeahead();
#ifdef DEBUG
        if (g_debug & DEB_CHECKPOINTING)
        {
            std::printf("(%d %d %d)",g_check_count,line_num.value_of(),g_art_line_num.value_of());
            std::fflush(stdout);
        }
#endif
        if (g_check_count >= g_do_check_when && line_num.value_of() == g_tc_LINES &&
            (g_art_line_num > 40 || g_check_count >= g_do_check_when + 10))
        {
                            // while he is reading a whole page
                            // in an article he is interested in
            g_check_count = 0;
            checkpoint_newsrcs();       // update all newsrcs
            update_thread_kill_file();
        }
        cache_until_key();
        if (g_art_size < 0 && (g_raw_art_size = nntp_art_size()) >= 0)
        {
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
            goto_xy(s_more_prompt_col,g_term_line);
            goto recheck_pager;
        }
        set_mode(g_general_mode,MM_PAGER);
        pager_command = get_cmd();
        if (errno)
        {
            if (g_tc_LINES < 100 && !g_int_count)
            {
                pager_command = '\f'; // on CONT fake up refresh
            }
            else
            {
                pager_command = 'q';   // on INTR or paper just quit
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
        switch (page_switch(pager_command))
        {
        case PS_ASK:  // reprompt "--MORE--..."
            goto reask_pager;

        case PS_RAISE:        // reparse on article level
            article_command = pager_command;
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

static bool maybe_set_color(const char *line_begin, const char *cp, bool back_search)
{
    const char ch = (cp == line_begin ? 0 : cp[-1]);
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
            while (cp > line_begin && cp[-1] != '\n')
            {
                cp--;
            }
            maybe_set_color(line_begin, cp, false);
        }
        return true;
    }
    else
    {
        cp = skip_hor_space(cp);
        if (std::string_view{">}]#!:|"}.find(*cp) != std::string_view::npos)
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

static bool pager_command_needs_completion(std::string_view command)
{
    return command.size() > 1 && command[1] == FINISH_CMD;
}

static std::string finish_pager_command(std::string_view command, bool donewline)
{
    if (!pager_command_needs_completion(command))
    {
        return std::string{command};
    }
    return finish_command(command.substr(0, 1), donewline);
}

static std::string finish_pager_dbl_command(std::string_view command)
{
    if (!pager_command_needs_completion(command))
    {
        return std::string{command};
    }
    return finish_dbl_char(command.substr(0, 1));
}

PageSwitchResult page_switch(std::string_view command)
{
    std::string pager_line;
    std::string default_command;
    if (command.empty() || command.front() == '\0')
    {
        default_command = "n";
        command = default_command;
    }
    const char command_char = command.front();

    switch (command_char)
    {
    case '!':                 // shell escape
        escapade();
        return PS_ASK;

    case Ctl('i'):
    {
        ArticleLine i = g_art_line_num;
        g_g_line = 3;
        pager_line = line_ptr(s_a_line_begin);
        while (!pager_line.empty() && at_nl(pager_line.front()) && i >= g_top_line)
        {
            ArticlePosition pos = virtual_read(--i);
            if (pos < 0)
            {
                pos = -pos;
            }
            if (pos < g_header_type[PAST_HEADER].min_pos)
            {
                break;
            }
            seek_art_buf(pos);
            if (!read_art_buf(pager_line, false))
            {
                pager_line = line_ptr(s_a_line_begin);
                break;
            }
        }
        const char        search_char = pager_line.empty() ? '\0' : pager_line.front();
        const std::string search_pattern = fmt::format("^[^{}\n]", search_char);
        s_gcompex.compile(search_pattern, true, true);
        goto caseG;
    }

    case Ctl('g'):
        g_g_line = 3;
        s_gcompex.compile("^Subject:", true, true);
        goto caseG;

    case 'g':         // in-article search
    {
        const std::string full_command = finish_pager_command(command, false);
        if (full_command.empty())// get rest of command
        {
            return PS_ASK;
        }
        std::string_view pattern{full_command};
        pattern.remove_prefix(1);
        if (!pattern.empty() && std::isspace(static_cast<unsigned char>(pattern.front())))
        {
            pattern.remove_prefix(1);
        }
        const char       *compile_error = s_gcompex.compile(pattern, true, true);
        if (compile_error != nullptr)
        {
                            // compile regular expression
            std::printf("\n%s\n", compile_error);
            term_down(2);
            return PS_ASK;
        }
        erase_line(false);      // erase the prompt
    }
        // FALL THROUGH

caseG:
    case 'G':
    {
        ArticlePosition start_where;
        bool success;

        if (g_g_line < 0 || g_g_line > g_tc_LINES-2)
        {
            g_g_line = g_tc_LINES - 2;
        }
#ifdef DEBUG
        if (g_debug & DEB_INNERSRCH)
        {
            std::printf("Start here? %d  >=? %d\n",g_top_line.value_of() + g_g_line + 1,g_art_line_num.value_of());
            term_down(1);
        }
#endif
        if (command_char == Ctl('i') || g_top_line + ArticleLine{g_g_line + 1} >= g_art_line_num)
        {
            start_where = g_art_pos;
                        // in case we had a line wrap
        }
        else
        {
            start_where = virtual_read(g_top_line + ArticleLine{g_g_line + 1});
            if (start_where < 0)
            {
                start_where = -start_where;
            }
        }
        start_where = std::max(start_where, g_header_type[PAST_HEADER].min_pos);
        seek_art_buf(start_where);
        g_inner_light = ArticleLine{};
        g_inner_search = ArticlePosition{}; // assume not found
        while (read_art_buf(pager_line, false))
        {
            const std::string::size_type newline = pager_line.find('\n');
            const std::string            search_line =
                newline == std::string::npos ? pager_line : pager_line.substr(0, newline + 1);
#ifdef DEBUG
            if (g_debug & DEB_INNERSRCH)
            {
                std::printf("Test %s\n",search_line.c_str());
            }
#endif
            success = s_gcompex.execute(search_line.c_str()) != nullptr;
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
        if (g_debug & DEB_INNERSRCH)
        {
            std::printf("On page? %ld <=? %ld\n",g_inner_search.value_of(),g_art_pos.value_of());
            term_down(1);
        }
#endif
        if (g_inner_search <= g_art_pos)          // already on page?
        {
            if (g_inner_search < g_art_pos)
            {
                g_art_line_num = line_after(g_top_line);
                while (virtual_read(g_art_line_num) < g_inner_search)
                {
                    ++g_art_line_num;
                }
            }
            g_highlight = line_before(g_art_line_num);
#ifdef DEBUG
            if (g_debug & DEB_INNERSRCH)
            {
                std::printf("@ %d\n",g_highlight.value_of());
                term_down(1);
            }
#endif
            g_top_line = g_highlight - ArticleLine{g_g_line};
            g_top_line = std::max(g_top_line, ArticleLine{-1});
            g_inner_search = ArticlePosition{};
            return page_switch("\f");
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
        if (g_debug & DEB_INNERSRCH)
        {
            std::printf("Topline = %d", g_top_line.value_of());
            std::string debug_pause;
            std::getline(std::cin, debug_pause);
        }
#endif
        clear();
        g_do_fseek = true;
        g_art_line_num = g_top_line;
        g_art_line_num = std::max(g_art_line_num, ArticleLine{});
        s_first_page = g_top_line < 0;
        return PS_NORM;

    case Ctl('e'):
        if (g_art_size < 0)
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
        g_inner_light = line_before(g_art_line_num);
        g_inner_search = g_art_size;
        g_g_line = 0;
        g_hide_everything = 'b';
        return PS_NORM;

    case 'B':         // one line up
        if (g_top_line < 0)
        {
            break;
        }
        if (*g_tc_IL && *g_tc_HO)
        {
            home_cursor();
            insert_line();
            carriage_return();
            ArticlePosition pos = virtual_read(line_before(g_top_line));
            if (pos < 0)
            {
                pos = -pos;
            }
            if (pos >= g_header_type[PAST_HEADER].min_pos)
            {
                seek_art_buf(pos);
                if (read_art_buf(pager_line, false))
                {
                    g_art_pos = virtual_read(g_top_line);
                    if (g_art_pos < 0)
                    {
                        g_art_pos = -g_art_pos;
                    }
                    const char *line = pager_line.c_str();
                    maybe_set_color(line, line, true);
                    for (pos = g_art_pos - pos; pos-- && !at_nl(*line); line++)
                    {
                        std::putchar(*line);
                    }
                    color_default();
                    std::putchar('\n');
                    --g_top_line;
                    g_art_pos = virtual_read(--g_art_line_num);
                    if (g_art_pos < 0)
                    {
                        g_art_pos = -g_art_pos;
                    }
                    seek_art_buf(g_art_pos);
                    s_a_line_begin = virtual_read(line_before(g_art_line_num));
                    if (s_a_line_begin < 0)
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
        if (command_char == 'B')
        {
            target = line_before(g_top_line);
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
        if (g_art_line_num >= 0)
        {
            do
            {
                --g_art_line_num;
            } while (g_art_line_num >= 0 && g_art_line_num > target &&
                     virtual_read(line_before(g_art_line_num)) >= 0);
        }
        g_top_line = g_art_line_num;  // remember top line of screen
                                // (line # within article file)
        g_art_line_num = std::max(g_art_line_num, ArticleLine{});
        s_first_page = g_top_line < 0;
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
    {
        const std::string full_command = finish_pager_dbl_command(command);
        if (full_command.empty())
        {
            return PS_ASK;
        }
        const char second_char =
            full_command.size() > 1 ? static_cast<char>(static_cast<unsigned char>(full_command[1]) & 0177) : '\0';
        switch (second_char)
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
    }

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
        g_artp->mark_as_read();   // mark article as read
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
        if (std::string_view{"nNpP\016\020"}.find(command_char) == std::string_view::npos //
            && std::string_view{"wWsSe:!&|/?123456789."}.find(command_char) != std::string_view::npos)
        {
            set_default_cmd();
            color_object(COLOR_CMD, true);
            const std::string mail_call = interp_search(g_mail_call, command);
            std::printf(g_prompt.c_str(), mail_call.c_str(), current_char_subst().c_str(),
                        g_default_cmd.c_str()); // print prompt, whatever it is
            color_pop();                        // of COLOR_CMD
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
        pager_line = line_ptr(s_a_line_begin);
        if ((pager_line.empty() || pager_line.front() != '\f') &&
            (g_page_stop.empty() || s_continuation || !g_page_compex.execute(pager_line.c_str())))
          {
              if (!s_special //
                  || (g_marking && (command_char != 'd' || (g_marking_areas & HALF_PAGE_MARKING))))
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
        std::fputs("Type h for help.\n", stdout);
        term_down(1);
        settle_down();
        return PS_ASK;
    }
    return PS_ASK;
}

static bool inner_more()
{
    if (g_art_pos < g_inner_search)               // not even on page yet?
    {
#ifdef DEBUG
        if (g_debug & DEB_INNERSRCH)
        {
            std::printf("Not on page %ld < %ld\n",g_art_pos.value_of(),g_inner_search.value_of());
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
            g_highlight = line_before(g_art_line_num);
        }
#ifdef DEBUG
        if (g_debug & DEB_INNERSRCH)
        {
            std::printf("There it is %ld = %ld, %d @ %d\n",g_art_pos.value_of(),
                g_inner_search.value_of(),g_hide_everything,g_highlight.value_of());
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
    if (g_debug & DEB_INNERSRCH)
    {
        std::printf("Not far enough? %d <? %d + %d\n",g_art_line_num.value_of(),s_i_search_line.value_of(),g_g_line);
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
            g_art = ap->article_num();
            g_reread = true;
            push_char(Ctl('r'));
        }
        else if (y > g_tc_LINES/2)
        {
            push_char(' ');
        }
        else if (g_top_line != -1)
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
        else if (g_top_line != -1)
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
