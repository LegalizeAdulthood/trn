/* rt-wumpus.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-wumpus.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/color.h>
#include <trn/head.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

int g_max_tree_lines{6};

static constexpr std::string_view s_letters{"123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+"};
static constexpr int              LAST_ALPHANUM{9 + 26 + 26};
static_assert(LAST_ALPHANUM == static_cast<int>(s_letters.size() - 1));

// clang-format off
static char s_tree_indent[] = {
    ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0
};
// clang-format on
static Article *s_tree_article{};
static int      s_max_depth{};
static int      s_max_line{-1}; // TODO: ArticleLine?
static int      s_first_depth{};
static int      s_first_line{};
static int      s_my_depth{};
static int      s_my_line{};
static bool     s_node_on_line{};
static int      s_node_line_cnt{};
static int      s_line_num{};
static int      s_header_indent{};
static std::array<std::string, 11> s_tree_lines{};
static std::string                 s_tree_buff;

static void     find_depth(Article *article, int depth);
static void     cache_tree(Article *ap, int depth, char *cp);
static Article *find_artp(Article *article, int x);
static void     display_tree(Article *article, char *cp);

// Prepare tree display for inclusion in the article header.
void init_tree()
{
    Article*thread;

    while (s_max_line >= 0)
    {
        s_tree_lines[s_max_line--].clear();
    }

    if (!(s_tree_article = g_curr_artp) || !s_tree_article->m_subj)
    {
        return;
    }
    if (!(thread = s_tree_article->m_subj->m_thread))
    {
        return;
    }
    // Enumerate our subjects for display
    Subject *sp = thread->m_subj;
    int      num = 0;
    do
    {
        sp->m_misc = num++;
        sp = sp->m_thread_link;
    } while (sp != thread->m_subj);

    s_max_depth = 0;
    s_max_line = 0;
    s_my_depth = 0;
    s_my_line = 0;
    s_node_line_cnt = 0;
    find_depth(thread, 0);

    if (s_max_depth <= 5)
    {
        s_first_depth = 0;
    }
    else
    {
        if (s_my_depth+2 > s_max_depth)
        {
            s_first_depth = s_max_depth - 5;
        }
        else
        {
            s_first_depth = s_my_depth - 3;
            s_first_depth = std::max(s_first_depth, 0);
        }
        s_max_depth = s_first_depth + 5;
    }
    if (--s_max_line < g_max_tree_lines)
    {
        s_first_line = 0;
    }
    else
    {
        if (s_my_line + g_max_tree_lines/2 > s_max_line)
        {
            s_first_line = s_max_line - (g_max_tree_lines - 1);
        }
        else
        {
            s_first_line = s_my_line - (g_max_tree_lines - 1) / 2;
            s_first_line = std::max(s_first_line, 0);
        }
        s_max_line = s_first_line + g_max_tree_lines-1;
    }

    s_tree_buff.clear();
    s_tree_buff.reserve(128);
    s_tree_buff += ' ';
    s_node_on_line = false;
    s_line_num = 0;
    // cache our portion of the tree
    cache_tree(thread, 0, s_tree_indent);

    s_max_depth = (s_max_depth-s_first_depth+1) * 5;    // turn depth into char width
    s_max_line -= s_first_line;                 // turn s_max_line into count
    // shorten tree if lower lines aren't visible
    if (s_node_line_cnt < s_max_line)
    {
        s_max_line = s_node_line_cnt + 1;
    }
}

// A recursive routine to find the maximum tree extents and where we are.
static void find_depth(Article *article, int depth)
{
    s_max_depth = std::max(depth, s_max_depth);
    while (true)
    {
        if (article == s_tree_article)
        {
            s_my_depth = depth;
            s_my_line = s_max_line;
        }
        if (article->m_child1)
        {
            find_depth(article->m_child1, depth + 1);
        }
        else
        {
            s_max_line++;
        }
        if (!(article = article->m_sibling))
        {
            break;
        }
    }
}

// Place the tree display in a maximum of 11 lines x 6 nodes.
static void cache_tree(Article *ap, int depth, char *cp)
{
    int depth_mode;

    cp[1] = ' ';
    if (depth >= s_first_depth && depth <= s_max_depth)
    {
        cp += 5;
        depth_mode = 1;
    }
    else if (depth + 1 == s_first_depth)
    {
        depth_mode = 2;
    }
    else
    {
        cp = s_tree_indent;
        depth_mode = 0;
    }
    while (true)
    {
        switch (depth_mode)
        {
        case 1:
        {
            char ch;

            s_tree_buff += ((ap->m_flags & AF_HAS_RE) || ap->m_parent) ? '-' : ' ';
            if (ap == s_tree_article)
            {
                s_tree_buff += '*';
            }
            if (!(ap->m_flags & AF_UNREAD))
            {
                s_tree_buff += '(';
                ch = ')';
            }
            else if (!g_selected_only || (ap->m_flags & AF_SEL))
            {
                s_tree_buff += '[';
                ch = ']';
            }
            else
            {
                s_tree_buff += '<';
                ch = '>';
            }
            if (ap == g_recent_artp && ap != s_tree_article)
            {
                s_tree_buff += '@';
            }
            s_tree_buff += ap->thread_letter();
            s_tree_buff += ch;
            if (ap->m_child1)
            {
                s_tree_buff += ap->m_child1->m_sibling ? '+' : '-';
            }
            if (ap->m_sibling)
            {
                *cp = '|';
            }
            else
            {
                *cp = ' ';
            }
            s_node_on_line = true;
            break;
        }

        case 2:
            s_tree_buff.front() = (!ap->m_child1) ? ' ' : (ap->m_child1->m_sibling) ? '+' : '-';
            break;

        default:
            break;
        }
        if (ap->m_child1)
        {
            cache_tree(ap->m_child1, depth+1, cp);
            cp[1] = '\0';
        }
        else
        {
            if (!s_node_on_line && s_first_line == s_line_num)
            {
                s_first_line++;
            }
            if (s_line_num >= s_first_line)
            {
                if (s_tree_buff.back() == ' ')
                {
                    s_tree_buff.pop_back();
                }
                s_tree_lines[s_line_num - s_first_line] = s_tree_buff;
                if (s_node_on_line)
                {
                    s_node_line_cnt = s_line_num - s_first_line;
                }
            }
            s_line_num++;
            s_node_on_line = false;
        }
        if (!(ap = ap->m_sibling) || s_line_num > s_max_line)
        {
            break;
        }
        if (!ap->m_sibling)
        {
            *cp = '\\';
        }
        if (!s_first_depth)
        {
            s_tree_indent[5] = ' ';
        }
        s_tree_buff = s_tree_indent + 5;
    }
}

static int s_find_artp_y{};

Article *get_tree_artp(int x, int y)
{
    if (!s_tree_article || !s_tree_article->m_subj)
    {
        return nullptr;
    }
    Article *ap = s_tree_article->m_subj->m_thread;
    x -= g_tc_COLS-1 - s_max_depth;
    if (x < 0 || y > s_max_line || !ap)
    {
        return nullptr;
    }
    x = (x-(x==s_max_depth))/5 + s_first_depth;
    s_find_artp_y = y + s_first_line;
    ap = find_artp(ap, x);
    return ap;
}

// A recursive routine to find the maximum tree extents and where we are.
static Article *find_artp(Article *article, int x)
{
    while (true)
    {
        if (!x && !s_find_artp_y)
        {
            return article;
        }
        if (article->m_child1)
        {
            Article* ap = find_artp(article->m_child1, x-1);
            if (ap)
            {
                return ap;
            }
        }
        else if (!s_find_artp_y--)
        {
            return nullptr;
        }
        if (!(article = article->m_sibling))
        {
            break;
        }
    }
    return nullptr;
}

inline bool header_conv()
{
    return g_char_subst[0] == 'a' || g_char_subst[0] == 'm';
}

// Output a header line with possible tree display on the right hand side.
// Does automatic wrapping of lines that are too long.
ArticleLine tree_puts(std::string_view orig_line, ArticleLine header_line, int is_subject)
{
    char       *tmpbuf;
    char       *line;
    char       *end;
    int         wrap_at;
    ArticleLine start_line = header_line;
    int         i;
    char        ch;
    char       *cp;
    std::string substituted_line;
    const std::string_view line_text = orig_line.substr(0, orig_line.find('\n'));
    const int              len = static_cast<int>(line_text.size());

    // Make a modifiable copy of the line
    // Copy line, filtering encoded and control characters.
    tmpbuf = safe_malloc(len + 2); // yes, I mean "2"
    line = tmpbuf;
    if (g_do_hiding)
    {
        end = line + decode_header(line, line_text);
    }
    else
    {
        if (!line_text.empty())
        {
            std::memcpy(line, line_text.data(), line_text.size());
        }
        line[line_text.size()] = '\0';
        dectrl(line);
        end = line + len;
    }
    if (header_conv())
    {
        substituted_line = str_char_subst(line, *g_char_subst);
        line = substituted_line.data();
        end = line + substituted_line.size();
    }

    if (!*line)
    {
        if (header_conv())
        {
            substituted_line = " ";
            line = substituted_line.data();
        }
        else
        {
            std::strcpy(line, " ");
        }
        end = line + 1;
    }

    color_object(COLOR_HEADER, true);
    // If this is the first subject line, output it with a preceding [1]
    if (is_subject && !std::isspace(*line))
    {
        if (g_threaded_group)
        {
            color_object(COLOR_TREE_MARK, true);
            std::putchar('[');
            std::putchar(g_curr_artp->thread_letter());
            std::putchar(']');
            color_pop();
            std::putchar(' ');
            s_header_indent = 4;
        }
        else
        {
            std::fputs("Subject: ", stdout);
            s_header_indent = 9;
        }
        i = 0;
    }
    else
    {
        if (*line != ' ')
        {
            // A "normal" header line -- output keyword and set s_header_indent
            // _except_ for the first line, which is a non-standard header.
            if (!header_line || !(cp = std::strchr(line, ':')) || *++cp != ' ')
            {
                s_header_indent = 0;
            }
            else
            {
                *cp = '\0';
                std::fputs(line, stdout);
                std::putchar(' ');
                s_header_indent = ++cp - line;
                line = cp;
                if (!*line)
                {
                    *--line = ' ';
                }
            }
            i = 0;
        }
        else
        {
            // Skip whitespace of continuation lines and prepare to indent
            line = skip_eq(++line, ' ');
            i = s_header_indent;
        }
    }
    for (; *line; i = s_header_indent)
    {
        maybe_eol();
        if (i)
        {
            std::putchar('+');
            while (--i)
            {
                std::putchar(' ');
            }
        }
        g_term_col = s_header_indent;
        // If no (more) tree lines, wrap at g_tc_COLS-1
        if (s_max_line < 0 || header_line > s_max_line+1)
        {
            wrap_at = g_tc_COLS - 1;
        }
        else
        {
            wrap_at = g_tc_COLS - s_max_depth - 3;
        }
        // Figure padding between header and tree output, wrapping long lines
        int pad_cnt = wrap_at - (end - line + s_header_indent);
        if (pad_cnt <= 0)
        {
            cp = line + (int)(wrap_at - s_header_indent - 1);
            pad_cnt = 1;
            while (cp > line && *cp != ' ')
            {
                if (*--cp == ',' || *cp == '.' || *cp == '-' || *cp == '!')
                {
                    cp++;
                    break;
                }
                pad_cnt++;
            }
            if (cp == line)
            {
                cp += wrap_at - s_header_indent;
                pad_cnt = 0;
            }
            ch = *cp;
            *cp = '\0';
            // keep rn's backpager happy
            virtual_write(g_art_line_num, virtual_read(line_before(g_art_line_num)));
            ++g_art_line_num;
        }
        else
        {
            cp = end;
            ch = '\0';
        }
        if (is_subject)
        {
            color_string(COLOR_SUBJECT, line);
        }
        else if (s_header_indent == 0 && *line != '+')
        {
            color_string(COLOR_ART_LINE1, line);
        }
        else
        {
            std::fputs(line, stdout);
        }
        *cp = ch;
        // Skip whitespace in wrapped line
        while (*cp == ' ')
        {
            cp++;
        }
        line = cp;
        // Check if we've got any tree lines to output
        if (wrap_at != g_tc_COLS - 1 && header_line <= s_max_line)
        {
            do
            {
                std::putchar(' ');
            } while (pad_cnt--);
            g_term_col = wrap_at;
            // Check string for the '*' flagging our current node
            // and the '@' flagging our prior node.
            std::string_view tree_line{s_tree_lines[header_line.value_of()]};
            color_object(COLOR_TREE, true);
            // Handle standout output for '*' and '@' marked nodes, then
            // continue with the rest of the line.
            for (std::size_t marker = tree_line.find_first_of("*@"); marker != std::string_view::npos;
                 marker = tree_line.find_first_of("*@"))
            {
                fmt::print("{}", tree_line.substr(0, marker));
                const std::size_t marked_size = tree_line[marker] == '*' ? 3 : 1;
                tree_line.remove_prefix(marker + 1);
                color_object(COLOR_TREE_MARK, true);
                fmt::print("{}", tree_line.substr(0, marked_size));
                color_pop(); // of COLOR_TREE_MARK
                tree_line.remove_prefix(marked_size);
            }
            fmt::print("{}", tree_line);
            color_pop(); // of COLOR_TREE
        }// if
        newline();
        ++header_line;
    }// for remainder of line

    // free allocated copy of line
    std::free(tmpbuf);

    color_pop();        // of COLOR_HEADER
    // return number of lines displayed
    return header_line - start_line;
}

// Output any parts of the tree that are left to display.  Called at the
// end of each header.
//
ArticleLine finish_tree(ArticleLine last_line)
{
    ArticleLine start_line = last_line;

    while (last_line <= s_max_line)
    {
        ++g_art_line_num;
        last_line += tree_puts("+", last_line, 0);
        virtual_write(g_art_line_num, g_art_pos);    // keep rn's backpager happy
    }
    return last_line - start_line;
}

// Output the entire article tree for the user.
//
// TODO: why does this check ap for nullptr?
//
void entire_tree(Article* ap)
{
    if (!ap)
    {
        if (g_verbose)
        {
            std::fputs("\nNo article tree to display.\n", stdout);
        }
        else
        {
            std::fputs("\nNo tree.\n", stdout);
        }
        term_down(2);
        return;
    }

    if (!g_threaded_group)
    {
        g_threaded_group = true;
        std::printf("Threading the group. ");
        std::fflush(stdout);
        thread_open();
        if (!g_threaded_group)
        {
            std::printf("*failed*\n");
            term_down(1);
            return;
        }
        count_subjects(CS_NORM);
        newline();
    }
    if (!(ap->m_flags & AF_THREADED))
    {
        parse_header(ap->article_num());
    }
    if (check_page_line())
    {
        return;
    }
    newline();
    Article *thread = ap->m_subj->m_thread;
    // Enumerate our subjects for display
    Subject *sp = thread->m_subj;
    int      num = 0;
    do
    {
        if (check_page_line())
        {
            return;
        }
        fmt::print("[{}] {}\n", s_letters[std::min(num, LAST_ALPHANUM)], sp->stripped_text());
        term_down(1);
        sp->m_misc = num++;
        sp = sp->m_thread_link;
    } while (sp != thread->m_subj);
    if (check_page_line())
    {
        return;
    }
    newline();
    if (check_page_line())
    {
        return;
    }
    std::putchar(' ');
    g_buf[3] = '\0';
    display_tree(thread, s_tree_indent);

    if (check_page_line())
    {
        return;
    }
    newline();
}

// A recursive routine to output the entire article tree.
//
static void display_tree(Article *article, char *cp)
{
    if (cp - s_tree_indent > g_tc_COLS || g_page_line < 0)
    {
        return;
    }
    cp[1] = ' ';
    cp += 5;
    color_object(COLOR_TREE, true);
    while (true)
    {
        std::putchar(((article->m_flags&AF_HAS_RE) || article->m_parent) ? '-' : ' ');
        if (!(article->m_flags & AF_UNREAD))
        {
            g_buf[0] = '(';
            g_buf[2] = ')';
        }
        else if (!g_selected_only || (article->m_flags & AF_SEL))
        {
            g_buf[0] = '[';
            g_buf[2] = ']';
        }
        else
        {
            g_buf[0] = '<';
            g_buf[2] = '>';
        }
        g_buf[1] = article->thread_letter();
        if (article == g_curr_artp)
        {
            color_string(COLOR_TREE_MARK,g_buf);
        }
        else if (article == g_recent_artp)
        {
            std::putchar(g_buf[0]);
            color_object(COLOR_TREE_MARK, true);
            std::putchar(g_buf[1]);
            color_pop();        // of COLOR_TREE_MARK
            std::putchar(g_buf[2]);
        }
        else
        {
            std::fputs(g_buf, stdout);
        }

        if (article->m_sibling)
        {
            *cp = '|';
        }
        else
        {
            *cp = ' ';
        }
        if (article->m_child1)
        {
            std::putchar((article->m_child1->m_sibling)? '+' : '-');
            color_pop();        // of COLOR_TREE
            display_tree(article->m_child1, cp);
            color_object(COLOR_TREE, true);
            cp[1] = '\0';
        }
        else
        {
            newline();
        }
        if (!(article = article->m_sibling))
        {
            break;
        }
        if (!article->m_sibling)
        {
            *cp = '\\';
        }
        s_tree_indent[5] = ' ';
        if (check_page_line())
        {
            color_pop();
            return;
        }
        std::fputs(s_tree_indent+5, stdout);
    }
    color_pop();        // of COLOR_TREE
}

// Calculate the subject letter representation.  "Place-holder" nodes
// are marked with a ' ', others get a letter in the sequence:
//      ' ', '1'-'9', 'A'-'Z', 'a'-'z', '+'
//
// TODO: decouple from s_letters
char Article::thread_letter()
{
    int subj = m_subj->m_misc;

    if (!(m_flags & AF_CACHED)
     && (g_abs_first < g_first_cached || g_last_cached < g_last_art
      || !g_cached_all_in_range))
    {
        return '?';
    }
    if (!(m_flags & AF_EXISTS))
    {
        return ' ';
    }
    return s_letters[subj > 9+26+26 ? 9+26+26 : subj];
}
