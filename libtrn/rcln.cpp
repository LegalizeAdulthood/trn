/* rcln.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rcln.h>

#include <config/common.h>
#include <trn/datasrc.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

enum
{
    MAX_DIGITS = 7
};

bool g_to_read_quiet{};

void rcln_init()
{
}

void NewsgroupData::catch_up(int leave_count, int output_level)
{
    if (leave_count)
    {
        if (output_level)
        {
            if (g_verbose)
            {
                std::printf("\nMarking all but %d articles in %s as read.\n",
                      leave_count,rc_line_c_str());
            }
            else
            {
                std::printf("\nAll but %d marked as read.\n", leave_count);
            }
        }
        check_expired(get_newsgroup_size() - ArticleNum{leave_count + 1});
        set_to_read(ST_STRICT);
    }
    else
    {
        if (output_level)
        {
            if (g_verbose)
            {
                std::printf("\nMarking %s as all read.\n", rc_line_c_str());
            }
            else
            {
                std::fputs("\nMarked read\n", stdout);
            }
        }
        std::string rc_line{rc_line_c_str()};
        rc_line += ": 1-";
        rc_line += std::to_string(get_newsgroup_size().value_of());
        m_rc_line = rc_line;
        hide_subscribe_char();
        if (g_newsgroup_min_to_read > TR_NONE && m_to_read > TR_NONE)
        {
            --g_newsgroup_to_read;
        }
        m_to_read = TR_NONE;
    }
    m_rc->flags |= RF_RC_CHANGED;
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
}

// add an article number to a newsgroup, if it isn't already read

int add_art_num(DataSource *dp, ArticleNum art_num, std::string_view newsgroup_name)
{
    ArticleNum min{};
    ArticleNum max{-1};
    ArticleNum lastnum{};

    if (!art_num)
    {
        return 0;
    }
    NewsgroupData *np = find_newsgroup(newsgroup_name);
    if (np == nullptr)                  // not found in newsrc?
    {
        return 0;
    }
    if (dp != np->m_rc->data_source)          // punt on cross-host xrefs
    {
#ifdef DEBUG
        if (g_debug & DEB_XREF_MARKER)
        {
            const std::string group_name{newsgroup_name};
            std::printf("Cross-host xref to group %s ignored.\n", group_name.c_str());
        }
#endif
        return 0;
    }
    if (!np->m_num_offset)
    {
        return 0;
    }
#ifndef ANCIENT_NEWS
    if (!np->m_abs_first)
    {
        // Trim down the list due to expires if we haven't done so yet.
        np->set_to_read(ST_LAX);
    }
#endif

    if (np->m_to_read == TR_BOGUS)
    {
        return 0;
    }
    if (art_num > np->m_ng_max)
    {
        if (np->m_to_read > TR_NONE)
        {
            np->m_to_read += (ArticleUnread)(art_num - np->m_ng_max).value_of();
        }
        np->m_ng_max = art_num;
    }
#ifdef DEBUG
    if (g_debug & DEB_XREF_MARKER)
    {
        std::printf("%ld->\n%s%c%s\n",art_num.value_of(),np->rc_line_c_str(), np->m_subscribe_char,
          np->rc_numbers_c_str());
    }
#endif
    std::string_view rc_line = np->m_rc_line;
    const auto       is_digit_at = [rc_line](std::size_t offset)
    { return offset < rc_line.size() && std::isdigit(static_cast<unsigned char>(rc_line[offset])); };
    const auto digits_end = [rc_line](std::size_t offset)
    {
        const std::size_t end = rc_line.find_first_not_of("0123456789", offset);
        return end == std::string_view::npos ? rc_line.size() : end;
    };
    const auto next_digit = [rc_line](std::size_t offset)
    {
        const std::size_t next = rc_line.find_first_of("0123456789", offset);
        return next == std::string_view::npos ? rc_line.size() : next;
    };
    const auto parse_article_num = [rc_line](std::size_t offset)
    {
        long                   value{};
        const std::string_view text = rc_line.substr(offset);
        std::from_chars(text.data(), text.data() + text.size(), value);
        return ArticleNum{value};
    };

    const std::size_t numbers_offset = static_cast<std::size_t>(np->m_num_offset);
    std::size_t       s_offset = rc_line.find_first_not_of(' ', numbers_offset);
    if (s_offset == std::string_view::npos)
    {
        s_offset = rc_line.size();
    }
    std::size_t t_offset = s_offset;
    std::size_t max_offset = std::string_view::npos;
    while (is_digit_at(s_offset) && art_num >= (min = parse_article_num(s_offset)))
    {
        // while it might have been read
        t_offset = digits_end(s_offset);                           // skip number
        if (t_offset < rc_line.size() && rc_line[t_offset] == '-') // is it a range?
        {
            ++t_offset; // skip to next number
            if (art_num <= (max = parse_article_num(t_offset)))
            {
                return 0; // it is in range => already read
            }
            lastnum = max;                   // remember it
            max_offset = t_offset;           // remember position in case we
                                             // want to overwrite the max
            t_offset = digits_end(t_offset); // skip second number
        }
        else
        {
            if (art_num == min) // explicitly a read article?
            {
                return 0;
            }
            lastnum = min;                       // remember what the number was
            max_offset = std::string_view::npos; // last one was not a range
        }
        t_offset = next_digit(t_offset); // skip comma and any spaces
        s_offset = t_offset;
    }

    // we have not read it, so insert the article number before s

    const bool morenum = is_digit_at(s_offset); // will it need a comma after?
    np->show_subscribe_char();
    std::string new_rc_line;
    new_rc_line.reserve(rc_line.size() + MAX_DIGITS + 2);
    new_rc_line = np->m_rc_line; // make new rc line
    std::size_t write_offset{};
    std::string insert_text;
    // Can we just extend last range?
    if (max_offset != std::string_view::npos && lastnum && art_num == article_after(lastnum))
    {
        // then overwrite previous max
        write_offset = max_offset;
    }
    else
    {
        // point t into new line instead
        write_offset = t_offset;
        if (lastnum) // have we parsed any line?
        {
            if (!morenum) // are we adding to the tail?
            {
                insert_text = ","; // supply comma before
            }
            if (max_offset == std::string_view::npos && art_num == article_after(lastnum)) // adjacent singletons?
            {
                if (morenum && write_offset > 0 && new_rc_line[write_offset - 1] == ',')
                {
                    new_rc_line[write_offset - 1] = '-';
                }
                else if (!morenum)
                {
                    insert_text = "-";
                }
            }
        }
    }
    if (morenum) // is there more to life?
    {
        if (min == article_after(art_num)) // can we consolidate further?
        {
            bool        range_before = (write_offset > 0 && new_rc_line[write_offset - 1] == '-');
            std::size_t nextmax = digits_end(s_offset);
            bool        range_after = (nextmax < rc_line.size() && rc_line[nextmax] == '-');

            if (!range_before)
            {
                insert_text += fmt::format("{}-", art_num.value_of());
                // artnum will be new min
            }

            if (range_after)
            {
                s_offset = nextmax + 1; // current range min is redundant
            }
        }
        else
        {
            insert_text += fmt::format("{},", art_num.value_of());
            // put the number and comma
        }
    }
    else
    {
        insert_text += fmt::format("{}", art_num.value_of());
        // put the number there (wherever)
    }
    new_rc_line.erase(write_offset);
    new_rc_line += insert_text;
    new_rc_line += rc_line.substr(s_offset); // copy remainder of line
#ifdef DEBUG
    if (g_debug & DEB_XREF_MARKER)
    {
        fmt::print("{}\n", new_rc_line);
    }
#endif
    np->m_rc_line = std::move(new_rc_line);
    // pull the switcheroo
    np->hide_subscribe_char();
    // wipe out : or !
    if (np->m_to_read > TR_NONE)   // lest we turn unsub into bogus
    {
        np->m_to_read--;
    }
    return 0;
}

// delete an article number from a newsgroup, if it is there

#ifdef MCHASE
void sub_art_num(DataSource *dp, ArticleNum art_num, std::string_view newsgroup_name)
{
    if (!art_num)
    {
        return;
    }
    NewsgroupData *np = find_newsgroup(newsgroup_name);
    if (np == nullptr) // not found in newsrc?
    {
        return;
    }
    if (dp != np->m_rc->data_source) // punt on cross-host xrefs
    {
        return;
    }
    if (!np->m_num_offset)
    {
        return;
    }
#ifdef DEBUG
    if (g_debug & DEB_XREF_MARKER)
    {
        std::printf("%ld<-\n%s%c%s\n", art_num.value_of(), np->rc_line_c_str(), np->m_subscribe_char,
                    np->rc_numbers_c_str());
    }
#endif
    np->show_subscribe_char();
    std::string_view rc_line = np->m_rc_line;
    const auto       is_digit_at = [rc_line](std::size_t offset)
    { return offset < rc_line.size() && std::isdigit(static_cast<unsigned char>(rc_line[offset])); };
    const auto digits_end = [rc_line](std::size_t offset)
    {
        const std::size_t end = rc_line.find_first_not_of("0123456789", offset);
        return end == std::string_view::npos ? rc_line.size() : end;
    };
    const auto next_digit = [rc_line](std::size_t offset)
    {
        const std::size_t next = rc_line.find_first_of("0123456789", offset);
        return next == std::string_view::npos ? rc_line.size() : next;
    };
    const auto parse_article_num = [rc_line](std::size_t offset)
    {
        long                   value{};
        const std::string_view text = rc_line.substr(offset);
        std::from_chars(text.data(), text.data() + text.size(), value);
        return ArticleNum{value};
    };

    const std::size_t numbers_offset = static_cast<std::size_t>(np->m_num_offset);
    std::size_t       s_offset = rc_line.find_first_not_of(' ', numbers_offset);
    if (s_offset == std::string_view::npos)
    {
        s_offset = rc_line.size();
    }

    // a little optimization, since it is almost always the last number

    std::size_t t_offset = rc_line.size(); // find end of string
    std::size_t last_number = t_offset;
    while (last_number != s_offset && std::isdigit(static_cast<unsigned char>(rc_line[last_number - 1])))
    {
        --last_number;
    }
    if (last_number != s_offset && rc_line[last_number - 1] == ',' && parse_article_num(last_number) == art_num)
    {
        std::string new_rc_line;
        new_rc_line.reserve(rc_line.size());
        new_rc_line = rc_line.substr(0, last_number - 1);
#ifdef DEBUG
        if (g_debug & DEB_XREF_MARKER)
        {
            fmt::print("{}\n", new_rc_line);
        }
#endif
        np->m_rc_line = std::move(new_rc_line);
        np->hide_subscribe_char();
        if (np->m_to_read >= TR_NONE)
        {
            ++np->m_to_read;
        }
        return;
    }

    // not the last number, oh well, we may need the length anyway

    ArticleNum min{};
    while (is_digit_at(s_offset) && art_num >= (min = parse_article_num(s_offset)))
    {
        // while it might have been read
        t_offset = digits_end(s_offset);                           // skip number
        if (t_offset < rc_line.size() && rc_line[t_offset] == '-') // is it a range?
        {
            ++t_offset; // skip to next number
            ArticleNum max = parse_article_num(t_offset);
            t_offset = digits_end(t_offset); // skip second number
            if (art_num <= max)
            {
                // it is in range => already read
                ArticleNum split_num = art_num;
                if (art_num == min)
                {
                    min = article_after(min);
                    split_num = ArticleNum{};
                }
                else if (art_num == max)
                {
                    max = article_before(max);
                    split_num = ArticleNum{};
                }

                std::string new_rc_line;
                new_rc_line.reserve(rc_line.size() + (split_num ? (MAX_DIGITS + 1) * 2 + 1 : 2));
                new_rc_line = rc_line.substr(0, s_offset);
                if (split_num) // split into two ranges?
                {
                    new_rc_line += fmt::format("{}-{},{}-{}", min.value_of(), article_before(split_num).value_of(),
                                               article_after(split_num).value_of(), max.value_of());
                }
                else if (min == max)
                {
                    new_rc_line += fmt::format("{}", min.value_of());
                }
                else // only one range
                {
                    new_rc_line += fmt::format("{}-{}", min.value_of(), max.value_of());
                }
                new_rc_line += rc_line.substr(t_offset); // copy remainder over
#ifdef DEBUG
                if (g_debug & DEB_XREF_MARKER)
                {
                    fmt::print("{}\n", new_rc_line);
                }
#endif
                np->m_rc_line = std::move(new_rc_line);
                np->hide_subscribe_char();
                if (np->m_to_read >= TR_NONE)
                {
                    ++np->m_to_read;
                }
                return;
            }
        }
        else if (art_num == min) // explicitly a read article?
        {
            std::size_t remove_start = s_offset;
            std::size_t remove_end = t_offset;
            if (remove_end < rc_line.size() && rc_line[remove_end] == ',') // pick a comma, any comma
            {
                ++remove_end;
            }
            else if (remove_start > 0 && rc_line[remove_start - 1] == ',')
            {
                --remove_start;
            }
            else if (remove_start >= 2 && rc_line[remove_start - 2] == ',') // (in case of space)
            {
                remove_start -= 2;
            }

            std::string new_rc_line;
            new_rc_line.reserve(rc_line.size());
            new_rc_line = rc_line.substr(0, remove_start);
            new_rc_line += rc_line.substr(remove_end);
#ifdef DEBUG
            if (g_debug & DEB_XREF_MARKER)
            {
                fmt::print("{}\n", new_rc_line);
            }
#endif
            np->m_rc_line = std::move(new_rc_line);
            np->hide_subscribe_char();
            if (np->m_to_read >= TR_NONE)
            {
                ++np->m_to_read;
            }
            return;
        }
        t_offset = next_digit(t_offset); // skip comma and any spaces
        s_offset = t_offset;
    }
    np->hide_subscribe_char();
}
#endif

// calculate the number of unread articles for a newsgroup
//
void NewsgroupData::set_to_read(bool lax_high_check)
{
    bool       virgin_ng = (!m_abs_first);
    ArticleNum ngsize = get_newsgroup_size();
    ArticleNum unread = ngsize;
    ArticleNum newmax;

    if (ngsize.value_of() == TR_BOGUS)
    {
        if (!g_to_read_quiet)
        {
            std::printf("\nInvalid (bogus) newsgroup found: %s\n", rc_line_c_str());
        }
        g_paranoid = true;
        if (virgin_ng || m_to_read >= g_newsgroup_min_to_read)
        {
            --g_newsgroup_to_read;
            g_missing_count++;
        }
        m_to_read = TR_BOGUS;
        return;
    }
    if (virgin_ng)
    {
        const std::string all_read = fmt::format(" 1-{}", ngsize.value_of());
        if (all_read != rc_numbers_c_str())
        {
            check_expired(m_abs_first); // this might realloc rcline
        }
    }
    auto parse_article_num = [](std::string_view text)
    {
        const char *first = text.data();
        const char *last = first + text.size();
        while (first != last && std::isspace(static_cast<unsigned char>(*first)))
        {
            ++first;
        }
        long value{};
        std::from_chars(first, last, value);
        return ArticleNum{value};
    };
    const std::string_view nums = rc_numbers_c_str();
    std::string            ranges;
    ranges.reserve(std::max<std::size_t>(64, nums.size() + MAX_DIGITS + 1));
    ranges.append(nums.data(), nums.size());
    ranges += ',';
    std::string_view  s{ranges};
    const std::size_t first_range = s.find_first_not_of(" \f\n\r\t\v");
    if (first_range > 0 && first_range != std::string_view::npos)
    {
        s.remove_prefix(first_range);
    }
    for (std::size_t comma = s.find(','); comma != std::string_view::npos; comma = s.find(',')) // for each range
    {
        const std::string_view range = s.substr(0, comma);
        const std::size_t      hyphen = range.find('-');
        if (hyphen != std::string_view::npos) // find - in range, if any
        {
            newmax = parse_article_num(range.substr(hyphen + 1));
            unread -= newmax - article_after(parse_article_num(range));
        }
        else
        {
            newmax = parse_article_num(range);
        }
        if (newmax != 0)
        {
            --unread; // recalculate length
        }
        if (newmax > ngsize) // paranoia check
        {
            if (!lax_high_check && newmax > ngsize)
            {
                unread = ArticleNum{-1};
                break;
            }
            unread += newmax - ngsize;
            m_ng_max = newmax;
            ngsize = newmax;
        }
        s.remove_prefix(comma + 1);
    }
    if (unread < 0) // SOMEONE RESET THE NEWSGROUP!!!
    {
        unread = ngsize; // assume nothing carried over
        if (!g_to_read_quiet)
        {
            std::printf("\nSomebody reset %s -- assuming nothing read.\n", rc_line_c_str());
        }
        *rc_numbers_data() = '\0';
        g_paranoid = true; // enough to make a guy paranoid
        m_rc->flags |= RF_RC_CHANGED;
    }
    if (m_subscribe_char == UNSUBSCRIBED_CHAR)
    {
        unread = ArticleNum{TR_UNSUB};
    }

    if (unread.value_of() >= g_newsgroup_min_to_read)
    {
        if (!virgin_ng && m_to_read < g_newsgroup_min_to_read)
        {
            ++g_newsgroup_to_read;
        }
    }
    else if (unread <= 0)
    {
        if (m_to_read > g_newsgroup_min_to_read)
        {
            --g_newsgroup_to_read;
            if (virgin_ng)
            {
                g_missing_count++;
            }
        }
    }
    m_to_read = (ArticleUnread)unread.value_of();    // remember how many are left
}

// make sure expired articles are marked as read
//
void NewsgroupData::check_expired(ArticleNum first)
{
    ArticleNum num;
    ArticleNum lastnum{};

    if (first <= 1)
    {
        return;
    }
#ifdef DEBUG
    if (g_debug & DEB_XREF_MARKER)
    {
        std::printf("1-%ld->\n%s%c%s\n",first.value_of()-1,rc_line_c_str(),m_subscribe_char,
          rc_numbers_c_str());
    }
#endif
    std::string_view       rc_line = m_rc_line;
    const std::size_t      numbers_offset = static_cast<std::size_t>(m_num_offset);
    const std::string_view numbers = rc_line.substr(numbers_offset);
    const auto first_non_space = std::find_if_not(numbers.begin(), numbers.end(),
                                                  [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
    const auto digits_end = [rc_line](std::size_t offset)
    {
        const std::size_t end = rc_line.find_first_not_of("0123456789", offset);
        return end == std::string_view::npos ? rc_line.size() : end;
    };
    const auto next_digit = [rc_line](std::size_t offset)
    {
        const std::size_t next = rc_line.find_first_of("0123456789", offset);
        return next == std::string_view::npos ? rc_line.size() : next;
    };
    const auto parse_article_num = [rc_line](std::size_t offset)
    {
        long                   value{};
        const std::string_view text = rc_line.substr(offset);
        std::from_chars(text.data(), text.data() + text.size(), value);
        return ArticleNum{value};
    };

    std::size_t s_offset = numbers_offset + static_cast<std::size_t>(first_non_space - numbers.begin());
    while (s_offset < rc_line.size() && (num = parse_article_num(s_offset)) <= first)
    {
        s_offset = next_digit(digits_end(s_offset));
        lastnum = num;
    }

    const std::string_view prefix{rc_line.data(), numbers_offset};
    const std::string_view remainder = rc_line.substr(s_offset);
    if (!remainder.empty() && s_offset > 0 && rc_line[s_offset - 1] == '-') // landed in a range?
    {
        if (lastnum != 1)
        {
            m_rc_line = fmt::format("{} 1-{}", prefix, remainder);
            m_rc->flags |= RF_RC_CHANGED;
        }
    }
    else
    {
        // s now points to what should follow the first range
        m_rc_line =
            fmt::format("{} 1-{}{}{}", prefix, first.value_of() - (lastnum != first),
                        !remainder.empty() ? "," : "", remainder);
        m_rc->flags |= RF_RC_CHANGED;
    }

#ifdef DEBUG
    if (g_debug & DEB_XREF_MARKER)
    {
        std::printf("%s%c%s\n",rc_line_c_str(),m_subscribe_char,
          rc_numbers_c_str());
    }
#endif
}

// Returns true if article is marked as read or does not exist
// could use a better name
bool was_read_group(ArticleNum artnum, std::string_view ngnam)
{
    const char* s;
    const char* t;
    ArticleNum min{};
    ArticleNum max{-1};

    if (!artnum)
    {
        return true;
    }
    NewsgroupData *np = find_newsgroup(ngnam);
    if (np == nullptr)          // not found in newsrc?
    {
        return true;
    }
    if (!np->m_num_offset)         // no numbers on line
    {
        return false;
    }

    if (np->m_to_read == TR_BOGUS)
    {
        return true;
    }
    if (artnum > np->m_ng_max)
    {
        return false;           // probably doesn't exist, however
    }
    s = skip_eq(np->rc_numbers_c_str(), ' '); // skip spaces
    t = s;
    while (std::isdigit(*s) && artnum >= (min = ArticleNum{std::atol(s)}))
    {
        const char* maxt = nullptr;
        ArticleNum lastnum{};
        // while it might have been read
        t = skip_digits(s);             // skip number
        if (*t == '-')                  // is it a range?
        {
            t++;                        // skip to next number
            if (artnum <= (max = ArticleNum{std::atol(t)}))
            {
                return true;            // it is in range => already read
            }
            lastnum = max;              // remember it
            maxt = t;                   // remember position in case we
                                        // want to overwrite the max
            t = skip_digits(t);         // skip second number
        }
        else
        {
            if (artnum == min)          // explicitly a read article?
            {
                return true;
            }
            lastnum = min;              // remember what the number was
            maxt = nullptr;             // last one was not a range
        }
        while (*t && !std::isdigit(*t))
        {
            t++;                        // skip comma and any spaces
        }
        s = t;
    }

    // we have not read it, so return false
    return false;
}
