/* bits.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/bits.h>

#include <config/common.h>
#include <nntp/nntpclient.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

int g_dm_count{};

static long s_chase_count{};
#ifdef VALIDATE_XREF_SITE
static std::optional<std::string> s_inews_site;
#endif

static bool yank_article(char *ptr, int arg);
static bool check_chase(char *ptr, int until_key);
static int chase_xref(ArticleNum art_num, bool mark_read);
#ifdef VALIDATE_XREF_SITE
static bool valid_xref_site(ArticleNum art_num, const char *site);
#endif

void bits_init()
{
}

void rc_to_bits()
{
    char*   my_buf = g_buf; // place to decode rc line
    char*   c;
    char*   h;
    ArticleNum unread;
    Article*ap;

    // modify the article flags to reflect what has already been read

    const char *numbers = skip_eq(g_newsgroup_ptr->rc_numbers_c_str(), ' ');
                                        // find numbers in rc line
    long i = std::strlen(numbers);
#ifndef lint
    if (i >= LINE_BUF_LEN-2)                 // bigger than g_buf?
    {
        my_buf = safe_malloc((MemorySize) (i + 2));
    }
#endif
    std::strcpy(my_buf,numbers);                    // make scratch copy of line
    if (my_buf[0])
    {
        my_buf[i++] = ',';               // put extra comma on the end
    }
    my_buf[i] = '\0';
    char *s = my_buf;                    // initialize the for loop below
    if (set_first_art(s))
    {
        s = std::strchr(s,',') + 1;
        ArticleNum n;
        for (n = article_first(g_abs_first); n < g_first_art; n = article_next(n))
        {
            article_ptr(n)->m_flags &= ~AF_UNREAD;
        }
        g_first_art = n;
    }
    else
    {
        g_first_art = article_first(g_first_art);
    }
    unread = ArticleNum{};
#ifdef DEBUG
    if (g_debug & DEB_CTLAREA_BITMAP)
    {
        std::printf("\n%s\n",my_buf);
        term_down(2);
        for (ArticleNum i = article_first(g_abs_first); i < g_first_art; i = article_next(i))
        {
            if (article_unread(i))
            {
                std::printf("%ld ", i.value_of());
            }
        }
    }
#endif
    ArticleNum n = g_first_art;
    for ( ; (c = std::strchr(s,',')) != nullptr; s = ++c)    // for each range
    {
        ArticleNum max;
        *c = '\0';                      // do not let index see past comma
        h = std::strchr(s,'-');
        ArticleNum min{std::atol(s)};
        min = std::max(min, g_first_art);    // make sure range is in range
        if (min > g_last_art)
        {
            min = article_after(g_last_art);
        }
        for (; n < min; n = article_next(n))
        {
            ap = article_ptr(n);
            if (ap->m_flags & AF_EXISTS)
            {
                if (ap->m_auto_flags & AUTO_KILL_MASK)
                {
                    ap->m_flags &= ~AF_UNREAD;
                }
                else
                {
                    ap->m_flags |= AF_UNREAD;
                    ++unread;
                    if (ap->m_auto_flags & AUTO_SEL_MASK)
                    {
                        ap->select_article(ap->m_auto_flags);
                    }
                }
            }
        }
        if (!h)
        {
            max = min;
        }
        else if ((max = ArticleNum{std::atol(h + 1)}) < min)
        {
            max = article_before(min);
        }
        max = std::min(max, g_last_art);
        // mark all arts in range as read
        for ( ; n <= max; n = article_next(n))
        {
            article_ptr(n)->m_flags &= ~AF_UNREAD;
        }
#ifdef DEBUG
        if (g_debug & DEB_CTLAREA_BITMAP)
        {
            std::printf("\n%s\n",s);
            term_down(2);
            for (ArticleNum a = g_abs_first; a <= g_last_art; a++)
            {
                if (!was_read(a))
                {
                    std::printf("%ld ",a.value_of());
                }
            }
        }
#endif
        n = article_next(max);
    }
    for (; n <= g_last_art; n = article_next(n))
    {
        ap = article_ptr(n);
        if (ap->m_flags & AF_EXISTS)
        {
            if (ap->m_auto_flags & AUTO_KILL_MASK)
            {
                ap->m_flags &= ~AF_UNREAD;
            }
            else
            {
                ap->m_flags |= AF_UNREAD;
                ++unread;
                if (ap->m_auto_flags & AUTO_SEL_MASK)
                {
                    ap->select_article(ap->m_auto_flags);
                }
            }
        }
    }
#ifdef DEBUG
    if (g_debug & DEB_CTLAREA_BITMAP)
    {
        std::fputs("\n(hit CR)",stdout);
        term_down(1);
        std::fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    if (my_buf != g_buf)
    {
        std::free(my_buf);
    }
    g_newsgroup_ptr->m_to_read = unread.value_of();
}

bool set_first_art(const char *s)
{
    s = skip_eq(s, ' ');
    if (!std::strncmp(s,"1-",2))                     // can we save some time here?
    {
        g_first_art = ArticleNum{std::atol(s+2)+1};               // process first range thusly
        g_first_art = std::max(g_first_art, g_abs_first);
        return true;
    }

    g_first_art = g_abs_first;
    return false;
}

// reconstruct the .newsrc line in a human readable form

void bits_to_rc()
{
    ArticleNum  i;
    ArticleNum  count{};
    std::string rc_line;
    rc_line.reserve(LINE_BUF_LEN);
    rc_line = g_newsgroup_ptr->rc_name();         // start with the newsgroup name
    rc_line += g_newsgroup_ptr->m_subscribe_char; // put the requisite : or !

    for (i = article_first(g_abs_first); i <= g_last_art; i = article_next(i))
    {
        if (article_unread(i))
        {
            break;
        }
    }
    fmt::format_to(std::back_inserter(rc_line), " 1-{},", i.value_of() - 1);
    for (; i <= g_last_art; ++i) // for each article in newsgroup
    {
        if (!was_read(i)) // still unread?
        {
            ++count; // then count it
        }
        else // article was read
        {
            fmt::format_to(std::back_inserter(rc_line), "{}", i.value_of());
            ArticleNum old_i = i; // remember this spot
            do
            {
                ++i;
            } while (i <= g_last_art && was_read(i));
            // find 1st unread article or end
            --i;           // backup to last read article
            if (i > old_i) // range of more than 1?
            {
                fmt::format_to(std::back_inserter(rc_line), "-{},", i.value_of());
            }
            else
            {
                rc_line.push_back(','); // otherwise, just a comma will do
            }
        }
    }
    if (rc_line.back() == ',') // is there a final ','?
    {
        rc_line.pop_back(); // take it back
    }
#ifdef DEBUG
    if ((g_debug & DEB_NEWSRC_LINE) && !g_panic)
    {
        fmt::print("{}: {}\n", g_newsgroup_ptr->rc_line_c_str(), g_newsgroup_ptr->rc_numbers_c_str());
        fmt::print("{}\n", rc_line);
        term_down(2);
    }
#endif
    g_newsgroup_ptr->m_rc_line = std::move(rc_line);
    g_newsgroup_ptr->hide_subscribe_char();
    if (g_newsgroup_ptr->m_subscribe_char == UNSUBSCRIBED_CHAR) // did they unsubscribe?
    {
        g_newsgroup_ptr->m_to_read = TR_UNSUB; // make line invisible
    }
    else
    {
        g_newsgroup_ptr->m_to_read = (ArticleUnread) count.value_of(); // otherwise, remember the count
    }
    g_newsgroup_ptr->m_rc->flags |= RF_RC_CHANGED;
}

void find_existing_articles()
{
    ArticleNum an;
    Article* ap;

    if (g_data_source->m_flags & DF_REMOTE)
    {
        // Parse the LISTGROUP output and remember everything we find
        if (nntp_art_nums())
        {
            for (ap = article_ptr(article_first(g_abs_first));
                 ap && ap->article_num() <= g_last_art;
                 ap = article_nextp(ap))
            {
                ap->m_flags &= ~AF_EXISTS;
            }
            while (true)
            {
                if (nntp_gets(g_ser_line, sizeof g_ser_line) == NGSR_ERROR)
                {
                    break;
                }
                if (nntp_at_list_end(g_ser_line))
                {
                    break;
                }
                an = ArticleNum{std::atol(g_ser_line)};
                if (an < g_abs_first)
                {
                    continue;   // Ignore some whacked-out NNTP servers
                }
                ap = article_ptr(an);
                if (!(ap->m_flags2 & AF2_BOGUS))
                {
                    ap->m_flags |= AF_EXISTS;
                }
            }
        }
        else if (g_first_subject && g_cached_all_in_range)
        {
            if (!g_data_source->m_ov_opened || !g_data_source->m_over_dir.empty())
            {
                for (ap = article_ptr(article_first(g_first_cached));
                     ap && ap->article_num() <= g_last_cached;
                     ap = article_nextp(ap))
                {
                    if (ap->m_flags & AF_CACHED)
                    {
                        ap->m_flags |= AF_EXISTS;
                    }
                }
            }
            for (an = g_abs_first; an < g_first_cached; ++an)
            {
                ap = article_ptr(an);
                if (!(ap->m_flags2 & AF2_BOGUS))
                {
                    ap->m_flags |= AF_EXISTS;
                }
            }
            for (an = article_after(g_last_cached); an <= g_last_art; ++an)
            {
                ap = article_ptr(an);
                if (!(ap->m_flags2 & AF2_BOGUS))
                {
                    ap->m_flags |= AF_EXISTS;
                }
            }
        }
        else
        {
            for (an = g_abs_first; an <= g_last_art; ++an)
            {
                ap = article_ptr(an);
                if (!(ap->m_flags2 & AF2_BOGUS))
                {
                    ap->m_flags |= AF_EXISTS;
                }
            }
        }
    }
    else
    {
        namespace fs = std::filesystem;
        ArticleNum first{article_after(g_last_art)};
        ArticleNum last{};
        fs::path cwd(".");
        char ch;
        long l_num;

        fs::directory_iterator entries(cwd);
        if (fs::directory_iterator() == entries)
        {
            return;
        }

        // Scan the directory to find which articles are present.
        for (ap = article_ptr(article_first(g_abs_first));
             ap && ap->article_num() <= g_last_art;
             ap = article_nextp(ap))
        {
            ap->m_flags &= ~AF_EXISTS;
        }

        for (const fs::directory_entry &entry : entries)
        {
            std::string filename{entry.path().filename().string()};
            if (std::sscanf(filename.c_str(), "%ld%c", &l_num, &ch) == 1)
            {
                an = ArticleNum{l_num};
                if (an <= g_last_art && an >= g_abs_first)
                {
                    first = std::min(an, first);
                    last = std::max(an, last);
                    ap = article_ptr(an);
                    if (!(ap->m_flags2 & AF2_BOGUS))
                    {
                        ap->m_flags |= AF_EXISTS;
                    }
                }
            }
        }

        g_newsgroup_ptr->m_abs_first = first;
        g_newsgroup_ptr->m_ng_max = last;

        if (first > g_abs_first)
        {
            g_newsgroup_ptr->check_expired(first);
            for (g_abs_first = article_first(g_abs_first);
                 g_abs_first < first;
                 g_abs_first = article_next(g_abs_first))
            {
                article_ptr(g_abs_first)->one_missing();
            }
            g_abs_first = first;
        }
        g_last_art = last;
    }

    g_first_art = std::max(g_first_art, g_abs_first);
    if (g_first_art > g_last_art)
    {
        g_first_art = article_after(g_last_art);
    }
    g_first_cached = std::max(g_first_cached, g_abs_first);
    if (g_last_cached < g_abs_first)
    {
        g_last_cached = article_before(g_abs_first);
    }
}

void one_less_art_num(ArticleNum art_num)
{
    Article* ap = article_find(art_num);
    if (ap)
    {
        ap->one_less();
    }
}

// Mark an article as read in this newsgroup and possibly chase xrefs.
// Don't call this on missing articles.
//
// TODO: decouple from s_chase_count
//
void Article::set_read()
{
    one_less();
    if (!g_olden_days && has_xrefs() && !(m_flags & AF_K_CHASE))
    {
        m_flags |= AF_K_CHASE;
        s_chase_count++;
    }
}

// mark article as read.  If article is cross-referenced to other
// newsgroups, mark them read there also.
//
// TODO: decouple from s_chase_count
//
void Article::mark_as_read()
{
    one_less();
    if (has_xrefs() && !(m_flags & AF_K_CHASE))
    {
        m_flags |= AF_K_CHASE;
        s_chase_count++;
    }
    g_check_count++;             // get more worried about crashes
}

#ifdef MCHASE
void note_chase_xref()
{
    s_chase_count++;
}
#endif

void mark_missing_articles()
{
    for (Article *ap = article_ptr(article_first(g_abs_first));
         ap && ap->article_num() <= g_last_art;
         ap = article_nextp(ap))
    {
        if (!(ap->m_flags & AF_EXISTS))
        {
            ap->one_missing();
        }
    }
}

// keep g_first_art pointing at the first unread article

void check_first(ArticleNum min)
{
    min = std::max(min, g_abs_first);
    g_first_art = std::min(min, g_first_art);
}

// bring back articles marked with M
void yank_back()
{
    if (g_dm_count)                      // delayed unmarks pending?
    {
        if (g_panic)
        {
        }
        else if (g_general_mode == GM_SELECTOR)
        {
            std::sprintf(g_msg, "Returned %ld Marked article%s.", (long) g_dm_count, plural(g_dm_count));
        }
        else
        {
            std::printf("\nReturning %ld Marked article%s...\n",(long)g_dm_count,
                plural(g_dm_count));
            term_down(2);
        }
        article_walk(yank_article, 0);
        g_dm_count = 0;
    }
}

static bool yank_article(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if (ap->m_flags & AF_YANK_BACK)
    {
        ap->unmark_as_read();
        if (g_selected_only)
        {
            ap->select_article(AUTO_KILL_NONE);
        }
        ap->m_flags &= ~AF_YANK_BACK;
    }
    return false;
}

bool chase_xrefs(bool until_key)
{
    if (!s_chase_count)
    {
        return true;
    }
    if (until_key)
    {
        set_spin(SPIN_BACKGROUND);
    }

    article_walk(check_chase, until_key);
    s_chase_count = 0;
    return true;
}

static bool check_chase(char *ptr, int until_key)
{
    Article* ap = (Article*)ptr;

    if (ap->m_flags & AF_K_CHASE)
    {
        chase_xref(ap->article_num(), true);
        ap->m_flags &= ~AF_K_CHASE;
        if (!--s_chase_count)
        {
            return true;
        }
    }
#ifdef MCHASE
    if (ap->m_flags & AF_M_CHASE)
    {
        chase_xref(ap->article_num(), true);
        ap->m_flags &= ~AF_M_CHASE;
        if (!--s_chase_count)
        {
            return true;
        }
    }
#endif
    if (until_key && input_pending())
    {
        return true;
    }
    return false;
}

// run down xref list and mark as read or unread

// The Xref-line-using version
static int chase_xref(ArticleNum art_num, bool mark_read)
{
    ArticleNum x;

    if (g_data_source->m_flags & DF_NO_XREFS)
    {
        return 0;
    }

    if (in_background())
    {
        spin(10);
    }
    else
    {
        if (g_output_chase_phrase)
        {
            if (g_verbose)
            {
                std::fputs("\nChasing xrefs", stdout);
            }
            else
            {
                std::fputs("\nXrefs", stdout);
            }
            term_down(1);
            g_output_chase_phrase = false;
        }
        std::putchar('.');
        std::fflush(stdout);
    }

    const std::string xref_text = fetch_cache(art_num, XREF_LINE, FILL_CACHE);
    if (xref_text.empty())
    {
        return 0;
    }

    std::string xref_buf{xref_text};
# ifdef DEBUG
    if (g_debug & DEB_XREF_MARKER)
    {
        std::printf("Xref: %s\n",xref_buf.c_str());
        term_down(1);
    }
# endif
    char *cur_xref = std::strchr(xref_buf.data(), ' ');
    if (cur_xref == nullptr)
    {
        return 0;
    }
    *cur_xref++ = '\0';
# ifdef VALIDATE_XREF_SITE
    if (valid_xref_site(art_num, xref_buf.data()))
# endif
    {
        while (*cur_xref)            // for each newsgroup
        {
            char *next_xref = std::strchr(cur_xref, ' ');
            if (next_xref != nullptr)
            {
                *next_xref++ = '\0';
            }
            else
            {
                next_xref = cur_xref + std::strlen(cur_xref);
            }
            char *group_name = cur_xref;
            cur_xref = skip_space(next_xref);
            char *xartnum = std::strchr(group_name, ':');
            if (!xartnum)
            {
                break;
            }
            *xartnum++ = '\0';
            if (!(value_of(x) = std::atol(xartnum)))
            {
                continue;
            }
            if (!std::strcmp(group_name,g_newsgroup_name.c_str()))  // is this the current newsgroup?
            {
                if (x < g_abs_first || x > g_last_art)
                {
                    continue;
                }
                if (mark_read)
                {
                    article_ptr(x)->one_less(); // take care of old C newses
                }
# ifdef MCHASE
                else
                {
                    onemore(article_ptr(x));
                }
# endif
            }
            else
            {
                if (mark_read)
                {
                    if (add_art_num(g_data_source,x,group_name))
                    {
                        break;
                    }
                }
# ifdef MCHASE
                else
                {
                    sub_art_num(g_data_source,x,group_name);
                }
# endif
            }
        }
    }
    return 0;
}

// Make sure the site name on Xref matches what inews thinks the site
// is.  Check first against last inews_site.  If it matches, fine.
// If not, fetch inews_site from current Path or Relay-Version line and
// check again.  This is so that if the new administrator decides
// to change the system name as known to inews, rn will still do
// Xrefs correctly--each article need only match itself to be valid.
//
# ifdef VALIDATE_XREF_SITE
static bool valid_xref_site(ArticleNum art_num, const char *site)
{
    std::string sitebuf;
    char* s;

    if (s_inews_site && *s_inews_site == site)
        return true;

#ifndef ANCIENT_NEWS
    // Grab the site from the first component of the Path line
    sitebuf = fetch_lines(art_num,PATH_LINE);
    s = std::strchr(sitebuf.data(), '!');
    if (s != nullptr)
    {
        *s = '\0';
        s_inews_site = sitebuf;
    }
#else // ANCIENT_NEWS
    // Grab the site from the Posting-Version line
    sitebuf = fetch_lines(art_num,RVER_LINE);
    s = in_string(sitebuf.data(), "; site ", true);
    if (s != nullptr)
    {
        char* t = std::strchr(s+7, '.');
        if (t)
        {
            *t = '\0';
        }
        s_inews_site = s+7;
    }
#endif // ANCIENT_NEWS
    else
    {
        s_inews_site = "";
    }

    if (*s_inews_site == site)
    {
        return true;
    }

#ifdef DEBUG
    if (g_debug)
    {
        std::printf("Xref not from %s -- ignoring\n",s_inews_site->c_str());
        term_down(1);
    }
#endif
    return false;
}
# endif // VALIDATE_XREF_SITE
