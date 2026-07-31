/* sadesc.cpp
 *
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/sadesc.h>

#include <config/common.h>
#include <trn/cache.h>
#include <trn/head.h>    // currently used for fast author fetch when group is threaded
#include <trn/rt-util.h> // compress_from()
#include <trn/samain.h>
#include <trn/sathread.h>
#include <trn/scan.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/terminal.h> // for standout

#include <fmt/format.h>

#include <iterator>
#include <string>

// returns statchars...
// int line;            // which status line (1 = first)
std::string sa_get_stat_chars(long a, int line)
{
// Debug
#if 0
    fmt::print("entry: sa_get_statchars({},{})\n", static_cast<int>(a), line);
#endif

    switch (line)
    {
    case 1:
    {
        std::string status{"..."};
        if (sa_marked(a))
        {
            status[2] = 'x';
        }
        if (sa_selected1(a))
        {
            status[1] = '*';
        }
        if (was_read(g_sa_ents[a].artnum))
        {
            status[0] = '-';
        }
        else
        {
            status[0] = '+';
        }
        return status;
    }

    default:
        return "   ";
    } // switch
}

std::string sa_desc_subject(long e)
{
    std::string subject = fetch_lines(g_sa_ents[e].artnum, SUBJ_LINE);

    if (subject.empty())
    {
        return "(no subject)";
    }
    if ((subject[0] == 'r' || subject[0] == 'R') && subject.size() >= 3)
    {
        if ((subject[1] == 'e' || subject[1] == 'E') && subject[2] == ':')
        {
            subject[2] = '>'; // more cosmetic "Re:"
            subject.erase(0, 2);
        }
    }
    return subject;
}

// NOTE: should redesign later for the "menu" style...
// long e;              // entry number
// bool trunc;          // should it be truncated?
std::string sa_get_desc(long e, int line, bool trunc)
{
    ArticleNum artnum = g_sa_ents[e].artnum;
    bool       use_standout = false;
    std::string desc;
    std::string s;
    switch (line)
    {
    case 1:
        if (g_sa_mode_desc_art_num)
        {
            fmt::format_to(std::back_inserter(desc), "{:6d} ", static_cast<int>(artnum.value_of()));
        }
        if (g_sc_initialized && g_sa_mode_desc_score)
        {
            // we'd like the score now
            fmt::format_to(std::back_inserter(desc), "[{:4d}] ", sc_score_art(artnum, true));
        }
        if (g_sa_mode_desc_thread_count)
        {
            fmt::format_to(std::back_inserter(desc), "({:3d}) ", sa_subj_thread_count(e));
        }
        if (g_sa_mode_desc_author)
        {
            desc += compress_from(article_ptr(artnum)->from_view(), trunc ? 16 : 200);
            desc += ' ';
        }
        if (g_sa_mode_desc_subject)
        {
            desc += sa_desc_subject(e);
        }
        break;

    case 2:   // summary line (test)
        s = fetch_lines(artnum,SUMMARY_LINE);
        if (!s.empty())   // we really have one
        {
            int i;              // number of spaces to indent

            i = 0;
            // if variable widths used later, use them
            if (g_sa_mode_desc_art_num)
            {
                i += 7;
            }
            if (g_sc_initialized && g_sa_mode_desc_score)
            {
                i += 7;
            }
            if (g_sa_mode_desc_thread_count)
            {
                i += 6;
            }
            desc.assign(static_cast<std::size_t>(i), ' ');
#ifdef HAS_TERMLIB
            if (use_standout)
            {
                fmt::format_to(std::back_inserter(desc), "Summary: {}{}", standout_start(), s);
            }
            else
#endif
            {
                fmt::format_to(std::back_inserter(desc), "Summary: {}", s);
            }
            break;
        }
        // otherwise, we might have had a keyword
        // FALL THROUGH

    case 3:   // Keywords (test)
        s = fetch_lines(artnum,KEYW_LINE);
        if (!s.empty())   // we really have one
        {
            int i;              // number of spaces to indent
            i = 0;
            // if variable widths used later, use them
            if (g_sa_mode_desc_art_num)
            {
                i += 7;
            }
            if (g_sc_initialized && g_sa_mode_desc_score)
            {
                i += 7;
            }
            if (g_sa_mode_desc_thread_count)
            {
                i += 6;
            }
            desc.assign(static_cast<std::size_t>(i), ' ');
#ifdef HAS_TERMLIB
            if (use_standout)
            {
                fmt::format_to(std::back_inserter(desc), "Keys: {}{}", standout_start(), s);
            }
            else
#endif
            {
                fmt::format_to(std::back_inserter(desc), "Keys: {}", s);
            }
            break;
        }
        // FALL THROUGH

    default:  // no line I know of
        // later return nullptr
        desc = fmt::format("Entry {}: Nonimplemented Description LINE", e);
        break;
    } // switch (line)
    if (trunc && desc.size() > static_cast<std::size_t>(g_s_desc_cols))
    {
        desc.resize(static_cast<std::size_t>(g_s_desc_cols)); // make sure it's not too long
    }
#ifdef HAS_TERMLIB
    if (use_standout)
    {
        desc += standout_end(); // end standout mode
    }
#endif
    // take out bad characters (replace with one space)
    for (char &ch : desc)
    {
        switch (ch)
        {
        case Ctl('h'):
        case '\t':
        case '\n':
        case '\r':
            ch = ' ';
        }
    }
    return desc;
}

// returns # of lines the article occupies in total...
// long e;                      // the entry number
int sa_ent_lines(long e)
{
    std::string s;
    int  num = 1;

    ArticleNum artnum = g_sa_ents[e].artnum;
    if (g_sa_mode_desc_summary)
    {
        s = fetch_lines(artnum,SUMMARY_LINE);
        if (!s.empty())
        {
            num++;      // just a test
        }
    }
    if (g_sa_mode_desc_keyw)
    {
        s = fetch_lines(artnum,KEYW_LINE);
        if (!s.empty())
        {
            num++;      // just a test
        }
    }
    return num;
}
