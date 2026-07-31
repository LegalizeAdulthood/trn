/* rt-util.cpp
*  vi: set sw=4 ts=8 ai sm noet :
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-util.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>

char g_spin_char{' '};           // char to put back when we're done spinning
long g_spin_estimate{};          // best guess of how much work there is
long g_spin_todo{};              // the max word to do (might decrease)
int  g_spin_count{};             // counter for when to spin
bool g_performed_article_loop{}; //
bool g_bkgnd_spinner{};          // -B
bool g_unbroken_subjects{};      // -u

static int s_spin_marks{25}; // how many bargraph marks we want

static std::string compress_address(std::string_view name, int max);
static std::string compress_name_text(std::string_view name, int max);
static void output_change(std::string &out, long num, std::string_view obj_type, std::string_view modifier,
                          std::string_view action);

static bool is_space(char ch)
{
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

static bool is_alpha(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) != 0;
}

static bool is_digit(char ch)
{
    return std::isdigit(static_cast<unsigned char>(ch)) != 0;
}

static std::string_view trim_left(std::string_view text)
{
    while (!text.empty() && is_space(text.front()))
    {
        text.remove_prefix(1);
    }
    return text;
}

static std::string_view trim_right(std::string_view text)
{
    while (!text.empty() && is_space(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

// Name-munging routines written by Ross Ridge.
// Enhanced by Wayne Davison.
//

// Extract the full-name part of an email address, returning an empty view
// if not found.
//
std::string_view extract_name(std::string_view name)
{
    name = trim_left(name);
    const std::size_t lparen = name.find('(');
    const std::size_t rparen = name.rfind(')');
    const std::size_t langle = name.find('<');
    const bool        has_lparen = lparen != std::string_view::npos;
    const bool        has_rparen = rparen != std::string_view::npos;
    const bool        has_langle = langle != std::string_view::npos;
    if (!has_lparen && !has_langle)
    {
        return {};
    }

    std::string_view display_name;
    if (has_langle && (!has_lparen || !has_rparen || lparen > langle || rparen < langle))
    {
        if (langle == 0)
        {
            return {};
        }
        display_name = name.substr(0, langle);
    }
    else
    {
        const std::size_t name_begin = lparen + 1;
        display_name = name.substr(name_begin, has_rparen ? rparen - name_begin : std::string_view::npos);
        display_name = trim_left(display_name);
        if (display_name.empty())
        {
            return {};
        }
    }

    if (!display_name.empty() && display_name.front() == '"')
    {
        display_name.remove_prefix(1);
        display_name = trim_left(display_name);
        const std::size_t quote = display_name.rfind('"');
        if (quote != std::string_view::npos)
        {
            display_name = display_name.substr(0, quote);
        }
    }

    return trim_right(display_name);
}

static std::string trim_string(std::string_view text)
{
    return std::string{trim_right(trim_left(text))};
}

static int visible_length(std::string_view text)
{
#ifdef USE_UTF_HACK
    return visual_length_of(text);
#else
    return static_cast<int>(text.size());
#endif
}

static bool visible_fits(std::string_view text, int max)
{
    return visible_length(text) <= max;
}

static std::size_t next_character(std::string_view text, std::size_t pos)
{
    if (pos >= text.size())
    {
        return text.size();
    }
#ifdef USE_UTF_HACK
    const int width = byte_length_at(text.substr(pos));
    if (width > 0)
    {
        const std::size_t next = pos + static_cast<std::size_t>(width);
        return next < text.size() ? next : text.size();
    }
#endif
    return pos + 1;
}

static std::string first_character(std::string_view text)
{
    if (text.empty())
    {
        return {};
    }
    return std::string{text.substr(0, next_character(text, 0))};
}

static std::string truncate_visible(std::string_view text, int max)
{
    if (max <= 0)
    {
        return {};
    }
#ifndef USE_UTF_HACK
    const std::size_t size = static_cast<std::size_t>(max);
    return std::string{text.substr(0, size < text.size() ? size : text.size())};
#else
    std::string result;
    int         width = 0;
    for (std::size_t pos = 0; pos < text.size();)
    {
        const int char_bytes = byte_length_at(text.substr(pos));
        const int char_width = visual_width_at(text.substr(pos));
        if (char_bytes <= 0 || char_width < 0 || width + char_width > max)
        {
            break;
        }
        result += text.substr(pos, static_cast<std::size_t>(char_bytes));
        width += char_width;
        pos += static_cast<std::size_t>(char_bytes);
    }
    return result;
#endif
}

static std::size_t last_token_start(std::string_view text)
{
    std::size_t end = text.size();
    while (end > 0 && is_space(text[end - 1]))
    {
        --end;
    }
    std::size_t start = end;
    while (start > 0 && !is_space(text[start - 1]))
    {
        --start;
    }
    return start;
}

static std::size_t first_token_end(std::string_view text)
{
    std::size_t end = 0;
    while (end < text.size() && !is_space(text[end]))
    {
        end = next_character(text, end);
    }
    return end;
}

static bool contains_digit(std::string_view text)
{
    for (char ch : text)
    {
        if (is_digit(ch))
        {
            return true;
        }
    }
    return false;
}

static bool is_initial(std::string_view text)
{
    return text.size() == 1 || (text.size() == 2 && text[1] == '.');
}

static std::string join_name_parts(std::string_view first, std::string_view middle, std::string_view last)
{
    std::string result;
    if (!first.empty())
    {
        result += first;
    }
    if (!middle.empty())
    {
        if (!result.empty())
        {
            result += ' ';
        }
        result += middle;
    }
    if (!last.empty())
    {
        if (!result.empty())
        {
            result += ' ';
        }
        result += last;
    }
    return result;
}

static std::string remove_trailing_junk(std::string_view text)
{
    for (std::size_t pos = next_character(text, 0); pos < text.size(); pos = next_character(text, pos))
    {
        const char ch = text[pos];
        const char next = pos + 1 < text.size() ? text[pos + 1] : '\0';
        if (ch == ',' || ch == ';' || ch == '(' || ch == '@' || (ch == '-' && (next == '-' || next == ' ')))
        {
            return std::string{trim_right(text.substr(0, pos))};
        }
    }
    return std::string{text};
}

static std::string middle_initials(std::string_view middle)
{
    std::string result;
    for (std::size_t pos = 0; pos < middle.size();)
    {
        while (pos < middle.size() && is_space(middle[pos]))
        {
            ++pos;
        }
        if (pos >= middle.size())
        {
            break;
        }
        if (is_alpha(middle[pos]))
        {
            if (!result.empty())
            {
                result += ' ';
            }
            const std::size_t next = next_character(middle, pos);
            result += middle.substr(pos, next - pos);
        }
        while (pos < middle.size() && !is_space(middle[pos]))
        {
            pos = next_character(middle, pos);
        }
    }
    return result;
}

// If necessary, compress a net user's full name by playing games with
// initials and the middle name(s).  If we start with "Ross Douglas Ridge"
// we try "Ross D Ridge", "Ross Ridge", "R D Ridge" and finally "R Ridge"
// before simply truncating the thing.  We also turn "R. Douglas Ridge"
// into "Douglas Ridge" and "Ross Ridge D.D.S." into "Ross Ridge" as a
// first step of the compaction, if needed.
//
std::string compress_name(std::string_view name, int max)
{
    if (max <= 0)
    {
        return {};
    }

    return compress_name_text(name, max);
}

static std::string compress_name_text(std::string_view name, int max)
{
    std::string text = trim_string(name);
    if (text.empty() || visible_fits(text, max))
    {
        return text;
    }

    text = remove_trailing_junk(text);
    while (!text.empty())
    {
        text = std::string{trim_right(text)};
        if (visible_fits(text, max))
        {
            return text;
        }

        const std::size_t last_start = last_token_start(text);
        if (last_start == 0)
        {
            return truncate_visible(text, max);
        }

        const std::string_view last_token{text.data() + last_start, text.size() - last_start};
        if (!last_token.empty() && (last_token.back() == '.' || contains_digit(last_token)))
        {
            text = std::string{trim_right(std::string_view{text}.substr(0, last_start))};
            continue;
        }
        break;
    }
    if (text.empty())
    {
        return {};
    }

    const std::size_t last_start = last_token_start(text);
    std::string       last{text.substr(last_start)};
    const std::string before_last{trim_right(std::string_view{text}.substr(0, last_start))};
    const std::size_t first_end = first_token_end(before_last);
    std::string       first{before_last.substr(0, first_end)};
    std::string       middle =
        first_end < before_last.size() ? trim_string(std::string_view{before_last}.substr(first_end)) : std::string{};

    if (!middle.empty())
    {
        const bool middle_is_quoted = middle.size() >= 2 && middle.front() == '"' && middle.back() == '"';
        if (visible_fits(join_name_parts(middle, {}, last), max) &&
            ((is_initial(first) && !is_initial(middle)) || middle_is_quoted))
        {
            first = middle_is_quoted ? middle.substr(1, middle.size() - 2) : middle;
            middle.clear();
        }
        else if (middle_is_quoted)
        {
            const std::string quoted{middle.substr(1, middle.size() - 2)};
            if (!visible_fits(quoted, max))
            {
                return compress_name_text(quoted, max);
            }
        }
    }

    if (!middle.empty())
    {
        const std::string initials = middle_initials(middle);
        if (!initials.empty())
        {
            const std::string candidate = join_name_parts(first, initials, last);
            if (visible_fits(candidate, max))
            {
                return candidate;
            }
            middle = initials;
        }
        else
        {
            middle.clear();
        }
    }

    if (!middle.empty())
    {
        const std::string candidate = join_name_parts(first, {}, last);
        if (visible_fits(candidate, max))
        {
            return candidate;
        }
    }

    if (!first.empty())
    {
        const std::string candidate = join_name_parts(first_character(first), {}, last);
        if (visible_fits(candidate, max))
        {
            return candidate;
        }
        return join_name_parts(first_character(first), {}, truncate_visible(last, max - 2));
    }

    return truncate_visible(last, max);
}

// Compress an email address, trying to keep as much of the local part of
// the addresses as possible.  The order of precedence is @ ! %, but
// @ % ! may be better...
//
static std::string compress_address(std::string_view name, int max)
{
    if (max <= 0)
    {
        return {};
    }
    const std::size_t max_len = static_cast<std::size_t>(max);

    // Remove white space from both ends.
    name = trim_right(trim_left(name));
    if (name.empty())
    {
        return {};
    }
    if (name.front() == '<')
    {
        name.remove_prefix(1);
        if (!name.empty() && name.back() == '>')
        {
            name.remove_suffix(1);
        }
    }
    if (name.size() <= max_len)
    {
        return std::string{name};
    }

    std::size_t at = std::string_view::npos;
    std::size_t bang = std::string_view::npos;
    std::size_t hack = std::string_view::npos;
    for (std::size_t pos = 1; pos < name.size(); pos++)
    {
        // If there's whitespace in the middle then it's probably not
        // really an email address.
        if (is_space(name[pos]))
        {
            return std::string{name.substr(0, max_len)};
        }
        switch (name[pos])
        {
        case '@':
            if (at == std::string_view::npos)
            {
                at = pos;
            }
            break;

        case '!':
            if (at == std::string_view::npos)
            {
                bang = pos;
                hack = std::string_view::npos;
            }
            break;

        case '%':
            if (at == std::string_view::npos && hack == std::string_view::npos)
            {
                hack = pos;
            }
            break;
        }
    }
    if (at == std::string_view::npos)
    {
        at = name.size();
    }

    std::size_t start = 0;
    if (hack != std::string_view::npos)
    {
        if (bang != std::string_view::npos)
        {
            if (at - bang - 1 >= max_len)
            {
                start = bang + 1;
            }
            else if (at >= max_len)
            {
                start = at - max_len;
            }
        }
    }
    else if (bang != std::string_view::npos)
    {
        if (at >= max_len)
        {
            start = at - max_len;
        }
    }
    return std::string{name.substr(start, max_len)};
}

// Fit the author name in <max> chars.  Uses the comment portion if present
// and pads with spaces.
//
std::string compress_from(std::string_view from, int size)
{
    if (size <= 0)
    {
        return {};
    }

    std::string      buffer = str_char_subst(from, current_char_subst_mode());
    std::string_view name = extract_name(buffer);
    std::string      text;
    if (!name.empty())
    {
        text = compress_name(name, size);
    }
    else
    {
        text = compress_address(buffer, size);
    }

    std::size_t len = text.size();
    int         vis_len;
#ifdef USE_UTF_HACK
    vis_len = visual_length_of(text);
#else
    vis_len = static_cast<int>(len);
#endif
    if (len == 0)
    {
        text = "NO NAME";
        len = 7;
    }
    while (vis_len < size)
    {
        text.push_back(' ');
        vis_len++;
    }
    return text;
}

inline bool eq_ignore_case(char unknown, char lower)
{
    return std::tolower(unknown) == lower;
}

bool strip_one_re(std::string_view subject, std::string_view &remaining)
{
    bool        has_re = false;
    std::size_t pos = 0;
    while (pos < subject.size() && at_grey_space(subject.substr(pos)))
    {
        pos++;
    }
    if (subject.size() - pos >= 2 && eq_ignore_case(subject[pos], 'r') &&
        eq_ignore_case(subject[pos + 1], 'e')) // check for Re:
    {
        std::size_t end = pos + 2;
        if (end < subject.size() && subject[end] == '^') // allow Re^2:
        {
            end++;
            while (end < subject.size() && subject[end] <= '9' && subject[end] >= '0')
            {
                end++;
            }
        }
        if (end < subject.size() && subject[end] == ':')
        {
            end++;
            while (end < subject.size() && at_grey_space(subject.substr(end)))
            {
                end++;
            }
            pos = end;
            has_re = true;
        }
    }
    remaining = subject.substr(pos);

    return has_re;
}

// Parse the subject to look for any "Re[:^]"s at the start.
// Returns true if a Re was found and sets remaining to the interesting
// characters.
//
bool subject_has_re(std::string_view subject, std::string_view &remaining)
{
    bool has_re = false;
    while (strip_one_re(subject, subject))
    {
        has_re = true;
    }
    remaining = subject;
    return has_re;
}

bool subject_has_re(std::string_view subject)
{
    std::string_view remaining;
    return subject_has_re(subject, remaining);
}

// Output a subject in <max> chars.  Does intelligent trimming that tries to
// save the last two words on the line, excluding "(was: blah)" if needed.
//
// TODO: why does this check ap for nullptr?
//
std::string compress_subj(const Article *ap, int max)
{
    if (!ap)
    {
        return "<MISSING>";
    }
    if (max <= 0)
    {
        return {};
    }

    // Put a preceding '>' on subjects that are replies to other articles
    std::string subject;
    subject.reserve(LINE_BUF_LEN);
    Article *first = (g_threaded_group ? ap->m_subj->m_thread : ap->m_subj->m_articles);
    if (ap != first || (ap->m_flags & AF_HAS_RE) || (!(ap->m_flags & AF_UNREAD) ^ g_sel_rereading))
    {
        subject += '>';
    }
    subject += str_char_subst(ap->m_subj->stripped_view(), current_char_subst_mode());

    // Remove "(was: oldsubject)", because we already know the old subjects.
    // Also match "(Re: oldsubject)".  Allow possible spaces after the ('s.
    for (std::size_t open = subject.find('(', 1); open != std::string::npos; open = subject.find('(', open + 1))
    {
        std::size_t text = open + 1;
        while (text < subject.size() && subject[text] == ' ')
        {
            text++;
        }
        if (text + 3 < subject.size() && eq_ignore_case(subject[text], 'w') && eq_ignore_case(subject[text + 1], 'a') &&
            eq_ignore_case(subject[text + 2], 's') && (subject[text + 3] == ':' || subject[text + 3] == ' '))
        {
            subject.erase(text == open + 1 ? open : text - 1);
            break;
        }
        const bool re_colon = text + 3 < subject.size() && subject[text + 2] == ':' && subject[text + 3] == ' ';
        const bool re_power = text + 4 < subject.size() && subject[text + 2] == '^' && subject[text + 4] == ':';
        if (text + 1 < subject.size() && eq_ignore_case(subject[text], 'r') && eq_ignore_case(subject[text + 1], 'e') &&
            (re_colon || re_power))
        {
            subject.erase(text == open + 1 ? open : text - 1);
            break;
        }
    }
    int len = static_cast<int>(subject.size());
    if (!g_unbroken_subjects && len > max)
    {
        // Try to include the last two words on the line while trimming
        const std::size_t last_word = subject.rfind(' ');
        if (last_word != std::string::npos)
        {
            const std::size_t next_to_last = last_word == 0 ? std::string::npos : subject.rfind(' ', last_word - 1);
            std::size_t       keep_from = last_word;
            const int         keep_threshold = len - max + 3 + 10 - 1;
            if (next_to_last != std::string::npos)
            {
                if (static_cast<int>(next_to_last) >= keep_threshold)
                {
                    keep_from = next_to_last;
                }
            }
            if (static_cast<int>(keep_from) >= keep_threshold)
            {
                const std::size_t prefix_len = static_cast<std::size_t>(max - (len - static_cast<int>(keep_from) + 3));
                subject = subject.substr(0, prefix_len) + "..." + subject.substr(keep_from + 1);
                len = max;
            }
        }
    }
    if (len > max)
    {
        subject.resize(static_cast<std::size_t>(max));
    }
    return subject;
}

// Modified version of a spinner originally found in Clifford Adams' strn.

static std::string_view s_spin_chars;
static int             s_spin_level{}; // used to allow non-interfering nested spins
static SpinMode        s_spin_mode{};
static int             s_spin_place{}; // represents place in s_spinchars array
static int             s_spin_pos{};   // the last spinbar position we drew
static ArticleNum      s_spin_art{};
static ArticlePosition s_spin_tell{};

void set_spin(SpinMode mode)
{
    switch (mode)
    {
    case SPIN_FOREGROUND:
    case SPIN_BACKGROUND:
    case SPIN_BAR_GRAPH:
        if (!s_spin_level++)
        {
            s_spin_art = g_open_art;
            if (s_spin_art != 0 && g_art_fp)
            {
                s_spin_tell = tell_art();
            }
            g_spin_count = 0;
            s_spin_place = 0;
        }
        if (s_spin_mode == SPIN_BAR_GRAPH)
        {
            mode = SPIN_BAR_GRAPH;
        }
        if (mode == SPIN_BAR_GRAPH)
        {
            if (s_spin_mode != SPIN_BAR_GRAPH)
            {
                s_spin_marks = (g_verbose ? 25 : 10);
                fmt::print(" [{:>{}}]", "", s_spin_marks);
                for (int i = s_spin_marks + 1; i--;)
                {
                    backspace();
                }
                std::fflush(stdout);
            }
            s_spin_pos = 0;
        }
        s_spin_chars = "|/-\\";
        s_spin_mode = mode;
        break;

    case SPIN_POP:
    case SPIN_OFF:
        if (s_spin_mode == SPIN_BAR_GRAPH)
        {
            s_spin_level = 1;
            spin(10000);
            if (g_spin_count >= g_spin_todo)
            {
                g_spin_char = ']';
            }
            g_spin_count--;
            s_spin_mode = SPIN_FOREGROUND;
        }
        if (mode == SPIN_POP && --s_spin_level > 0)
        {
            break;
        }
        s_spin_level = 0;
        if (s_spin_place)       // we have spun at least once
        {
            std::putchar(g_spin_char); // get rid of spin character
            backspace();
            std::fflush(stdout);
            s_spin_place = 0;
        }
        if (s_spin_art)
        {
            art_open(s_spin_art,s_spin_tell);   // do not screw up the pager
            s_spin_art = ArticleNum{};
        }
        s_spin_mode = SPIN_OFF;
        g_spin_char = ' ';
        break;
    }
}

// modulus for the spin...
void spin(int count)
{
    if (!s_spin_level)
    {
        return;
    }
    switch (s_spin_mode)
    {
    case SPIN_BACKGROUND:
        if (!g_bkgnd_spinner)
        {
            return;
        }
        if (!(++g_spin_count % count))
        {
            std::putchar(s_spin_chars[++s_spin_place % s_spin_chars.size()]);
            backspace();
            std::fflush(stdout);
        }
        break;

    case SPIN_FOREGROUND:
        if (!(++g_spin_count % count))
        {
            std::putchar('.');
            std::fflush(stdout);
        }
        break;

    case SPIN_BAR_GRAPH:
    {
        if (g_spin_todo == 0)
        {
            break;              // bail out rather than crash
        }
        int new_pos = (int)((long)s_spin_marks * ++g_spin_count / g_spin_todo);
        if (s_spin_pos < new_pos && g_spin_count <= g_spin_todo+1)
        {
            do
            {
                std::putchar('*');
            } while (++s_spin_pos < new_pos);
            s_spin_place = 0;
            std::fflush(stdout);
        }
        else if (!(g_spin_count % count))
        {
            std::putchar(s_spin_chars[++s_spin_place % s_spin_chars.size()]);
            backspace();
            std::fflush(stdout);
        }
        break;
    }
    }
}

bool in_background()
{
    return s_spin_mode == SPIN_BACKGROUND;
}

static int         s_prior_perform_cnt{};
static std::time_t s_prior_now{};
static long        s_ps_sel{};
static long        s_ps_cnt{};
static long        s_ps_missing{};

void perform_status_init(int cnt)
{
    g_perform_count = 0;
    g_error_occurred = false;
    g_subj_line = std::nullopt;
    g_page_line = 1;
    g_performed_article_loop = true;

    s_prior_perform_cnt = 0;
    s_prior_now = 0;
    s_ps_sel = g_selected_count;
    s_ps_cnt = cnt;
    s_ps_missing = g_missing_count;

    g_spin_count = 0;
    s_spin_place = 0;
    s_spin_chars = "v>^<";
}

void perform_status(int cnt, int spin)
{
    if (!(++g_spin_count % spin))
    {
        std::putchar(s_spin_chars[++s_spin_place % s_spin_chars.size()]);
        backspace();
        std::fflush(stdout);
    }

    if (g_perform_count == s_prior_perform_cnt)
    {
        return;
    }

    std::time_t now = std::time(nullptr);
    if (now - s_prior_now < 2)
    {
        return;
    }

    s_prior_now = now;
    s_prior_perform_cnt = g_perform_count;

    long missing = g_missing_count - s_ps_missing;
    long kills = s_ps_cnt - cnt - missing;
    long sels = g_selected_count - s_ps_sel;

    if (!(kills | sels))
    {
        return;
    }

    carriage_return();
    if (g_perform_count != sels && g_perform_count != -sels && g_perform_count != kills && g_perform_count != -kills)
    {
        fmt::print("M:{} ", g_perform_count);
    }
    if (kills)
    {
        fmt::print("K:{} ", kills);
    }
    if (sels)
    {
        fmt::print("S:{} ", sels);
    }
    erase_eol();
    std::fflush(stdout);
}

static void skip_past_pipe(std::string_view &text)
{
    const std::size_t pipe = text.find('|');
    text.remove_prefix(pipe == std::string_view::npos ? text.size() : pipe + 1);
}

static void append_until_pipe(std::string &out, std::string_view &text)
{
    const std::size_t pipe = text.find('|');
    const std::size_t count = pipe == std::string_view::npos ? text.size() : pipe;
    out += text.substr(0, count);
    text.remove_prefix(count);
}

static void skip_separator(std::string_view &text)
{
    if (!text.empty())
    {
        text.remove_prefix(1);
    }
}

static void output_change(std::string &out, long num, std::string_view obj_type, std::string_view modifier,
                          std::string_view action)
{
    bool neg;

    if (num < 0)
    {
        num *= -1;
        neg = true;
    }
    else
    {
        neg = false;
    }

    if (!out.empty())
    {
        out += ", ";
    }
    out += std::to_string(num);
    out += ' ';
    if (!obj_type.empty())
    {
        out += obj_type;
        out += plural(num);
        out += ' ';
    }
    std::string_view text = modifier;
    if (!text.empty())
    {
        out += ' ';
        if (num != 1)
        {
            skip_past_pipe(text);
        }
        append_until_pipe(out, text);
        out += ' ';
    }
    text = action;
    if (!neg)
    {
        skip_past_pipe(text);
    }
    append_until_pipe(out, text);
    skip_separator(text);
    if (neg)
    {
        skip_past_pipe(text);
    }
    out += text;
}

int perform_status_end(long cnt, std::string_view obj_type)
{
    bool article_status = !obj_type.empty() && obj_type.front() == 'a';

    g_msg.clear();
    if (g_perform_count == 0)
    {
        g_msg = "No ";
        g_msg += obj_type;
        g_msg += "s affected.";
        return 0;
    }

    long missing = g_missing_count - s_ps_missing;
    long kills = s_ps_cnt - cnt - missing;
    long sels = g_selected_count - s_ps_sel;

    if (!g_performed_article_loop)
    {
        output_change(g_msg, (long)g_perform_count,
                           g_sel_mode == SM_THREAD? "thread" : "subject",
                           {}, "ERR|match|ed");
    }
    else if (g_perform_count != sels && g_perform_count != -sels //
             && g_perform_count != kills && g_perform_count != -kills)
    {
        output_change(g_msg, (long)g_perform_count, obj_type, {},
                           "ERR|match|ed");
        obj_type = {};
    }
    if (kills)
    {
        output_change(g_msg, kills, obj_type, {},
                           article_status? "un||killed" : "more|less|");
        obj_type = {};
    }
    if (sels)
    {
        output_change(g_msg, sels, obj_type, {}, "de||selected");
        obj_type = {};
    }
    if (article_status && missing > 0)
    {
        g_msg += '(';
        output_change(g_msg, missing, obj_type, "was|were", "ERR|missing|");
        g_msg += ')';
    }

    g_msg += ".";

    // If we only selected/deselected things, return 1, else 2
    return (kills | missing) == 0? 1 : 2;
}
