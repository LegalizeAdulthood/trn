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
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

int g_max_tree_lines{6};

static constexpr std::string_view s_letters{"123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+"};
static constexpr int              LAST_ALPHANUM{9 + 26 + 26};
static_assert(LAST_ALPHANUM == static_cast<int>(s_letters.size() - 1));

static constexpr int TREE_INDENT_STEP{5};
static Article *s_tree_article{};
static int      s_max_depth{};
static int      s_max_line{-1}; // TODO: ArticleLine?
static int      s_first_depth{};
static int      s_first_line{};
static int      s_my_depth{};
static int      s_my_line{};
static int      s_header_indent{};
static std::vector<std::string> s_tree_lines{};

struct TreeRenderState
{
    int                      first_depth{};
    int                      max_depth{};
    int                      first_line{};
    int                      max_line{};
    int                      line_num{};
    int                      node_line_count{};
    bool                     node_on_line{};
    bool                     stop_at_max_depth{};
    std::string              line;
    std::string              prefix;
    std::vector<std::string> lines;
};

static void     find_depth(Article *article, int depth);
static void     cache_tree(Article *ap, int depth, TreeRenderState &render);
static Article *find_artp(Article *article, int x);
static std::vector<std::string> render_tree_lines(Article *article, TreeRenderState &render);
static void     print_tree_line(std::string_view tree_line);

// Prepare tree display for inclusion in the article header.
void init_tree()
{
    Article*thread;

    s_tree_lines.clear();
    s_max_line = -1;

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

    TreeRenderState render;
    render.first_depth = s_first_depth;
    render.max_depth = s_max_depth;
    render.first_line = s_first_line;
    render.max_line = s_max_line;
    s_tree_lines = render_tree_lines(thread, render);
    s_first_line = render.first_line;

    s_max_depth = (s_max_depth-s_first_depth+1) * 5;    // turn depth into char width
    s_max_line -= s_first_line;                 // turn s_max_line into count
    // shorten tree if lower lines aren't visible
    if (render.node_line_count < s_max_line)
    {
        s_max_line = render.node_line_count + 1;
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

static void set_tree_prefix(std::string &prefix, int visible_depth, char ch)
{
    const std::size_t offset = static_cast<std::size_t>(visible_depth * TREE_INDENT_STEP);
    if (prefix.size() <= offset)
    {
        prefix.resize(offset + 1, ' ');
    }
    prefix[offset] = ch;
}

static void truncate_tree_prefix(std::string &prefix, int visible_depth)
{
    const std::size_t length = static_cast<std::size_t>(visible_depth * TREE_INDENT_STEP + 1);
    if (prefix.size() > length)
    {
        prefix.resize(length);
    }
}

static void append_tree_node(std::string &line, Article *article)
{
    char ch;

    line += ((article->m_flags & AF_HAS_RE) || article->m_parent) ? '-' : ' ';
    if (article == g_curr_artp)
    {
        line += '*';
    }
    if (!(article->m_flags & AF_UNREAD))
    {
        line += '(';
        ch = ')';
    }
    else if (!g_selected_only || (article->m_flags & AF_SEL))
    {
        line += '[';
        ch = ']';
    }
    else
    {
        line += '<';
        ch = '>';
    }
    if (article == g_recent_artp && article != g_curr_artp)
    {
        line += '@';
    }
    line += article->thread_letter();
    line += ch;
    if (article->m_child1)
    {
        line += article->m_child1->m_sibling ? '+' : '-';
    }
}

static std::vector<std::string> render_tree_lines(Article *article, TreeRenderState &render)
{
    render.line.clear();
    render.line.reserve(128);
    render.line += ' ';
    render.prefix.clear();
    render.lines.clear();
    render.line_num = 0;
    render.node_line_count = 0;
    render.node_on_line = false;

    cache_tree(article, 0, render);
    return std::move(render.lines);
}

// Build the tree display lines for the current render window.
static void cache_tree(Article *ap, int depth, TreeRenderState &render)
{
    int depth_mode;
    int visible_depth = depth - render.first_depth;

    if (render.stop_at_max_depth && depth > render.max_depth)
    {
        return;
    }
    if (visible_depth >= 0 && depth <= render.max_depth)
    {
        depth_mode = 1;
    }
    else if (depth + 1 == render.first_depth)
    {
        depth_mode = 2;
    }
    else
    {
        depth_mode = 0;
    }
    while (true)
    {
        switch (depth_mode)
        {
        case 1:
            append_tree_node(render.line, ap);
            set_tree_prefix(render.prefix, visible_depth, ap->m_sibling ? '|' : ' ');
            render.node_on_line = true;
            break;

        case 2:
            if (render.line.empty())
            {
                render.line += ' ';
            }
            render.line.front() = (!ap->m_child1) ? ' ' : (ap->m_child1->m_sibling) ? '+' : '-';
            break;

        default:
            break;
        }
        if (ap->m_child1)
        {
            cache_tree(ap->m_child1, depth + 1, render);
            if (depth_mode == 1)
            {
                truncate_tree_prefix(render.prefix, visible_depth);
            }
        }
        else
        {
            if (!render.node_on_line && render.first_line == render.line_num)
            {
                render.first_line++;
            }
            if (render.line_num >= render.first_line)
            {
                if (!render.line.empty() && render.line.back() == ' ')
                {
                    render.line.pop_back();
                }
                render.lines.push_back(render.line);
                if (render.node_on_line)
                {
                    render.node_line_count = render.line_num - render.first_line;
                }
            }
            render.line_num++;
            render.node_on_line = false;
        }
        if (!(ap = ap->m_sibling) || render.line_num > render.max_line)
        {
            break;
        }
        if (depth_mode == 1 && !ap->m_sibling)
        {
            set_tree_prefix(render.prefix, visible_depth, '\\');
        }
        if (!render.first_depth && !render.prefix.empty())
        {
            render.prefix.front() = ' ';
        }
        render.line = render.prefix;
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

static void print_tree_line(std::string_view tree_line)
{
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
}

// Output a header line with possible tree display on the right hand side.
// Does automatic wrapping of lines that are too long.
ArticleLine tree_puts(std::string_view orig_line, ArticleLine header_line, int is_subject)
{
    char       *line;
    char       *end;
    int         wrap_at;
    ArticleLine start_line = header_line;
    int         i;
    char        ch;
    char       *cp;
    std::string substituted_line;
    const std::string_view line_text = orig_line.substr(0, orig_line.find('\n'));

    // Make a modifiable copy of the line
    // Copy line, filtering encoded and control characters.
    std::string line_buffer;
    line_buffer.reserve(line_text.size() + 2);
    if (g_do_hiding)
    {
        line_buffer = decode_header(line_text);
    }
    else
    {
        line_buffer.assign(line_text);
    }
    const std::size_t line_size = line_buffer.size();
    line_buffer.resize(line_size + 2, '\0');
    line = line_buffer.data();
    end = line + line_size;
    if (!g_do_hiding)
    {
        dectrl(line_buffer);
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
            line[0] = ' ';
            line[1] = '\0';
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
            color_object(COLOR_TREE, true);
            if (header_line.value_of() < static_cast<int>(s_tree_lines.size()))
            {
                print_tree_line(s_tree_lines[header_line.value_of()]);
            }
            color_pop(); // of COLOR_TREE
        }// if
        newline();
        ++header_line;
    }// for remainder of line

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
        fmt::print("[{}] {}\n", s_letters[std::min(num, LAST_ALPHANUM)], sp->stripped_view());
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

    TreeRenderState render;
    render.first_depth = 0;
    render.max_depth = g_tc_COLS / TREE_INDENT_STEP;
    render.first_line = 0;
    render.max_line = std::numeric_limits<int>::max();
    render.stop_at_max_depth = true;

    for (const std::string &tree_line : render_tree_lines(thread, render))
    {
        if (check_page_line())
        {
            return;
        }
        color_object(COLOR_TREE, true);
        print_tree_line(tree_line);
        color_pop(); // of COLOR_TREE
        newline();
    }
    newline();
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
