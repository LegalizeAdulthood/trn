/* cache.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/cache.h>

#include <config/common.h>
#include <trn/bits.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/mime.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/rt-ov.h>
#include <trn/rt-page.h>
#include <trn/rt-process.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/score.h>
#include <trn/search.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

std::map<ArticleNum, Article> g_article_list;
std::vector<Article *>        g_art_ptr_list;        // the article-selector creates this
Article                     **g_art_ptr{};           // ditto -- used for article order
ArticleNum                    g_search_ahead{};      // are we in subject scan mode? (if so, contains art # found or -1)
ArticleNum                    g_first_cached{};      //
ArticleNum                    g_last_cached{};       //
bool                          g_cached_all_in_range{};  //
Article                      *g_sentinel_art_ptr{};     //
Subject                      *g_first_subject{};        //
Subject                      *g_last_subject{};         //
bool                          g_untrim_cache{};         //
int                           g_join_subject_len{};     // -J
int                           g_olden_days{};           // -o
char                          g_auto_select_postings{}; // -p

#ifdef PENDING
static ArticleNum    s_subj_to_get{};
static ArticleNum    s_xref_to_get{};
static CompiledRegex s_search_compex; // compiled regex for search ahead
#endif
static HashTable *s_subj_hash{};
static HashTable *s_short_subj_hash{};

static int subject_cmp(std::string_view key, HashDatum data);
#ifdef PENDING
static bool cache_xrefs();
static bool cache_all_arts();
static bool cache_unread_arts();
#endif
static bool art_data(ArticleNum first, ArticleNum last, bool cheating, bool all_articles);

Article *article_ptr(ArticleNum an)
{
    auto [it, inserted] = g_article_list.try_emplace(an, Article{});
    if (inserted)
    {
        it->second.m_num = an;
    }
    return &it->second;
}

bool article_hasdata(ArticleNum an)
{
    return g_article_list.find(an) != g_article_list.end();
}

Article *article_find(ArticleNum an)
{
    if (an > g_last_art)
    {
        return nullptr;
    }
    const auto it = g_article_list.find(an);
    return it == g_article_list.end() ? nullptr : &it->second;
}

bool article_walk(bool (*callback)(char *, int), int arg)
{
    for (auto &[num, article] : g_article_list)
    {
        if (callback(reinterpret_cast<char *>(&article), arg))
        {
            return true;
        }
    }
    return false;
}

ArticleNum article_first(ArticleNum an)
{
    const auto it = g_article_list.lower_bound(an);
    return it == g_article_list.end() ? article_after(g_last_art) : it->first;
}

ArticleNum article_next(ArticleNum an)
{
    const auto it = g_article_list.upper_bound(an);
    return it == g_article_list.end() ? article_after(g_last_art) : it->first;
}

ArticleNum article_last(ArticleNum an)
{
    auto it = g_article_list.upper_bound(an);
    if (it == g_article_list.begin())
    {
        return article_before(g_abs_first);
    }
    --it;
    return it->first;
}

ArticleNum article_prev(ArticleNum an)
{
    auto it = g_article_list.lower_bound(an);
    if (it == g_article_list.begin())
    {
        return article_before(g_abs_first);
    }
    --it;
    return it->first;
}

Article *article_nextp(Article *ap)
{
    if (ap == nullptr)
    {
        return nullptr;
    }
    const auto it = g_article_list.upper_bound(ap->article_num());
    return it == g_article_list.end() ? nullptr : &it->second;
}

void cache_init()
{
#ifdef PENDING
    s_search_compex.init_compex();
#endif
}

static NewsgroupData *s_cached_ng{};
static std::time_t    s_cached_time{};

void build_cache()
{
    if (s_cached_ng == g_newsgroup_ptr && std::time(nullptr) < s_cached_time + 6 * 60 * 60L)
    {
        s_cached_time = std::time(nullptr);
        if (g_sel_mode == SM_ARTICLE)
        {
            set_selector(g_sel_mode, g_sel_art_sort);
        }
        else
        {
            set_selector(g_sel_thread_mode, g_sel_thread_sort);
        }
        for (ArticleNum an{article_after(g_last_cached)}; an <= g_last_art; ++an)
        {
            article_ptr(an)->m_flags |= AF_EXISTS;
        }
        rc_to_bits();
        thread_grow();
        return;
    }

    close_cache();

    s_cached_ng = g_newsgroup_ptr;
    s_cached_time = std::time(nullptr);
    g_article_list.clear();
    s_subj_hash = hash_create(991, subject_cmp); // TODO: pick a better size

    set_first_art(g_newsgroup_ptr->rc_numbers_c_str());
    g_first_cached = g_thread_always ? g_abs_first : g_first_art;
    g_last_cached = article_before(g_first_cached);
    g_cached_all_in_range = false;
#ifdef PENDING
    s_subj_to_get = g_first_art;
    s_xref_to_get = g_first_art;
#endif

    // Cache as much data in advance as possible, possibly threading
    // articles as we go.
    thread_open();
}

void close_cache()
{
    Subject *next;

    nntp_art_name(ArticleNum{}, false); // clear the tmp file cache

    if (s_subj_hash)
    {
        hash_destroy(s_subj_hash);
        s_subj_hash = nullptr;
    }
    if (s_short_subj_hash)
    {
        hash_destroy(s_short_subj_hash);
        s_short_subj_hash = nullptr;
    }
    // Free all the subjects.
    for (Subject *sp = g_first_subject; sp; sp = next)
    {
        next = sp->m_next;
        delete sp;
    }
    g_first_subject = nullptr;
    g_last_subject = nullptr;
    g_subject_count = 0; // just to be sure
    g_parsed_art = ArticleNum{};

    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_sel_page_app = nullptr;
    g_sel_next_app = nullptr;
    thread_close();

    article_walk(
        [](char *cp, int)
        {
            reinterpret_cast<Article *>(cp)->clear_article();
            return false;
        },
        0);
    g_article_list.clear();
    s_cached_ng = nullptr;
}

// TODO: decouple this from s_short_subj_hash
//
void Article::check_for_near_subj()
{
    Subject* sp;
    if (!s_short_subj_hash)
    {
        s_short_subj_hash = hash_create(401, subject_cmp);    // TODO: pick a better size
        sp = g_first_subject;
    }
    else
    {
        sp = m_subj;
        if (sp->m_next)
        {
            sp = nullptr;
        }
    }
    while (sp)
    {
        const std::string_view subject_text = sp->stripped_view();
        if ((int) subject_text.size() >= g_join_subject_len && sp->m_thread)
        {
            Subject* sp2;
            HashDatum data = hash_fetch(
                    s_short_subj_hash,
                    subject_text.substr(0, static_cast<std::size_t>(g_join_subject_len)));
            if (!(sp2 = (Subject *) data.dat_ptr))
            {
                data.dat_ptr = (char*)sp;
                hash_store_last(data);
            }
            else if (sp->m_thread != sp2->m_thread)
            {
                merge_threads(sp2, sp);
            }
        }
        sp = sp->m_next;
    }
}

void change_join_subject_len(int len)
{
    if (g_join_subject_len != len)
    {
        if (s_short_subj_hash)
        {
            hash_destroy(s_short_subj_hash);
            s_short_subj_hash = nullptr;
        }
        g_join_subject_len = len;
        if (len && g_first_subject && g_first_subject->m_articles)
        {
            g_first_subject->m_articles->check_for_near_subj();
        }
    }
}

// The article turned out to be a duplicate, so remove it from the cached
// list and possibly destroy the subject (should only happen if the data
// was corrupt and the duplicate id got a different subject).
//
// TODO: decouple this from s_subj_hash
//
void Article::uncache_article(bool remove_empties)
{
    if (m_subj)
    {
        if (all_bits(m_flags, AF_CACHED | AF_EXISTS))
        {
            Article *next = m_subj->m_articles;
            if (next == this)
            {
                m_subj->m_articles = m_subj_next;
            }
            else
            {
                while (next)
                {
                    Article *ap2 = next->m_subj_next;
                    if (ap2 == this)
                    {
                        next->m_subj_next = m_subj_next;
                        break;
                    }
                    next = ap2;
                }
            }
        }
        if (remove_empties && !m_subj->m_articles)
        {
            Subject* sp = m_subj;
            if (sp == g_first_subject)
            {
                g_first_subject = sp->m_next;
            }
            else
            {
                sp->m_prev->m_next = sp->m_next;
            }
            if (sp == g_last_subject)
            {
                g_last_subject = sp->m_prev;
            }
            else
            {
                sp->m_next->m_prev = sp->m_prev;
            }
            hash_delete(s_subj_hash, sp->stripped_view());
            delete sp;
            m_subj = nullptr;
            g_subject_count--;
        }
    }
    m_flags2 |= AF2_BOGUS;
    one_missing();
}

// get the header line from an article's cache or parse the article trying

std::string fetch_cache(ArticleNum art_num, HeaderLineType which_line, bool fill_cache)
{
    Article *ap;
    bool     cached = (g_header_type[which_line].flags & HT_CACHED);

    // article_find() returns a nullptr if the article number value is invalid
    if (!(ap = article_find(art_num)) || !(ap->m_flags & AF_EXISTS))
    {
        return {};
    }
    if (cached)
    {
        std::string line = ap->get_cached_line_text(which_line, g_untrim_cache);
        if (!line.empty())
        {
            return line;
        }
    }
    if (!fill_cache)
    {
        return {};
    }
    if (!parse_header(art_num))
    {
        return {};
    }
    if (cached)
    {
        return ap->get_cached_line_text(which_line, g_untrim_cache);
    }
    return {};
}

// subj not yet allocated, so we can tweak it first
//
// TODO: decouple from s_subj_hash
//
void Article::set_subj_line(std::string_view subj)
{
    HashDatum        data;
    Subject         *sp;
    std::string_view subj_start;

    if (subject_has_re(subj, subj_start))
    {
        m_flags |= AF_HAS_RE;
    }
    int size = static_cast<int>(subj_start.size());

    std::string new_subj(static_cast<std::size_t>(size) + 4 + 1, '\0');
    std::copy_n("Re: ", 4, new_subj.data());
    size = decode_header(new_subj.data() + 4, subj_start);

    // Do the Re:-stripping over again, just in case it was encoded.
    const std::string_view decoded_subject{new_subj.data() + 4, static_cast<std::size_t>(size)};
    if (subject_has_re(decoded_subject, subj_start))
    {
        m_flags |= AF_HAS_RE;
    }
    const std::size_t prefix_size = decoded_subject.size() - subj_start.size();
    size = static_cast<int>(subj_start.size());
    if (prefix_size != 0)
    {
        new_subj.erase(4, prefix_size);
    }
    new_subj.resize(static_cast<std::size_t>(size) + 4);

    if (m_subj && !std::strncmp(m_subj->stripped_text(), new_subj.c_str() + 4, size))
    {
        return;
    }

    if (m_subj)
    {
        // This only happens when we freshen truncated subjects
        hash_delete(s_subj_hash, m_subj->stripped_view());
        m_subj->m_str = std::move(new_subj);
        data.dat_ptr = (char*)m_subj;
        hash_store(s_subj_hash, m_subj->stripped_view(), data);
    }
    else
    {
        const std::string_view new_key{new_subj.c_str() + 4, static_cast<std::size_t>(size)};
        data = hash_fetch(s_subj_hash, new_key);
        if (!(sp = (Subject *) data.dat_ptr))
        {
            sp = new Subject{};
            g_subject_count++;
            sp->m_prev = g_last_subject;
            if (sp->m_prev != nullptr)
            {
                sp->m_prev->m_next = sp;
            }
            else
            {
                g_first_subject = sp;
            }
            g_last_subject = sp;
            sp->m_str = std::move(new_subj);
            sp->m_thread_link = sp;
            sp->m_flags = SF_NONE;

            data.dat_ptr = (char*)sp;
            hash_store_last(data);
        }
        m_subj = sp;
    }
}

int decode_header(char *to, std::string_view from)
{
    char      *s = to; // save for pass 2
    bool       pass2_needed = false;
    int        size = static_cast<int>(from.size());
    const char *cursor = from.data();

    if (from.empty())
    {
        *to = '\0';
        return 0;
    }

    const char *const end = cursor + from.size();
    const auto        find_char = [end](const char *start, char ch)
    {
        while (start < end)
        {
            if (*start == ch)
            {
                return start;
            }
            ++start;
        }
        return static_cast<const char *>(nullptr);
    };
    const auto skip_hor_space_in_view = [end](const char *start)
    {
        while (start < end && is_hor_space(*start))
        {
            ++start;
        }
        return start;
    };

    // Pass 1 to decode coded bytes (which might be character fragments - so 1 pass is wrong)
    while (cursor < end && *cursor)
    {
        if (*cursor == '=' && cursor + 1 < end && cursor[1] == '?')
        {
            const char *q = find_char(cursor + 2, '?');
            char        ch = (q && q + 2 < end && q[2] == '?') ? q[1] : 0;
            const char *e;

            if (ch == 'q' || ch == 'Q' || ch == 'b' || ch == 'B')
            {
                std::string_view old_ics = input_charset_name();
                std::string_view old_ocs = output_charset_name();
#ifdef USE_UTF_HACK
                std::string charset{cursor + 2, q};
                utf_init(charset, CHARSET_NAME_UTF8); // FIXME
#endif
                e = q + 2;
                do
                {
                    e = find_char(e + 1, '?');
                } while (e && e + 1 < end && e[1] != '=');
                if (e && e + 1 < end)
                {
                    int         len = static_cast<int>(e - cursor + 2);
                    std::string encoded{q + 3, e};
                    size -= len;
                    cursor = e + 2;
                    if (ch == 'q' || ch == 'Q')
                    {
                        len = qp_decode_string(to, encoded.c_str(), true);
                    }
                    else
                    {
                        len = b64_decode_string(to, encoded.c_str());
                    }
#ifdef USE_UTF_HACK
                    std::string utf8_copy = create_utf8_copy(to);
                    len = static_cast<int>(utf8_copy.size());
                    std::memcpy(to, utf8_copy.c_str(), utf8_copy.size());
#endif
                    to += len;
                    size += len;
                    // If the next character is whitespace we should eat it now
                    cursor = skip_hor_space_in_view(cursor);
                }
                else
                {
                    *to++ = *cursor++;
                }
#ifdef USE_UTF_HACK
                utf_init(old_ics, old_ocs);
#endif
            }
            else
            {
                *to++ = *cursor++;
            }
        }
        else if (*cursor != '\n')
        {
            *to++ = *cursor++;
        }
        else
        {
            cursor++;
            size--;
        }
        pass2_needed = true;
    }
    while (size > 1 && to[-1] == ' ')
    {
        to--;
        size--;
    }
    *to = '\0';

    // Pass 2 to clear out "control" characters
    if (pass2_needed)
    {
        dectrl(s);
    }
    return size;
}

void dectrl(char *str)
{
    if (str == nullptr)
    {
        return;
    }

    while (*str)
    {
        int w = byte_length_at(str);
        if (at_grey_space(str))
        {
            for (int i = 0; i < w; i += 1)
            {
                str[i] = ' ';
            }
        }
        str += w;
    }
}

static int subject_cmp(std::string_view key, HashDatum data)
{
    const auto            *subject = (Subject *) data.dat_ptr;
    const std::string_view subject_text = subject->stripped_view().substr(0, key.size());

    return key.compare(subject_text);
}

// see what we can do while they are reading

#ifdef PENDING
void look_ahead()
{
#ifdef ARTSEARCH
    char* h;
    char* s;

#ifdef DEBUG
    if (g_debug && g_srchahead)
    {
        std::printf("(%ld)",(long)g_srchahead);
        std::fflush(stdout);
    }
#endif
#endif

    if (g_threaded_group)
    {
        g_artp = g_curr_artp;
        inc_article(g_selected_only,false);
        if (g_artp)
        {
            parse_header(g_art);
        }
    }
    else
#ifdef ARTSEARCH
    if (g_srchahead && g_srchahead < g_art)     // in ^N mode?
    {
        char* pattern;

        pattern = g_buf+1;
        std::strcpy(pattern,": *");
        h = pattern + std::strlen(pattern);
        interp(h,(sizeof g_buf) - (h-g_buf),"%\\s");
        {                       // compensate for notes files
            for (int i = 24; *h && i--; h++)
            {
                if (*h == '\\')
                {
                    h++;
                }
            }
            *h = '\0';
        }
#ifdef DEBUG
        if (g_debug & DEB_SEARCH_AHEAD)
        {
            std::fputs("(hit CR)",stdout);
            std::fflush(stdout);
            std::fgets(g_buf+128, sizeof g_buf-128, stdin);
            std::printf("\npattern = %s\n",pattern);
            term_down(2);
        }
#endif
        const char *compile_error = s_srchcompex.compile(pattern, true, true);
        if (compile_error != nullptr)
        {
                                    // compile regular expression
            std::printf("\n%s\n",compile_error);
            term_down(2);
            g_srchahead = 0;
        }
        if (g_srchahead)
        {
            g_srchahead = g_art;
            while (true)
            {
                g_srchahead++;  // go forward one article
                if (g_srchahead > g_lastart)   // out of articles?
                {
#ifdef DEBUG
                    if (g_debug)
                    {
                        std::fputs("(not found)",stdout);
                    }
#endif
                    break;
                }
                if (!was_read(g_srchahead) && //
                    wanted(&s_srchcompex, g_srchahead, 0))
                {
                                    // does the shoe fit?
#ifdef DEBUG
                    if (g_debug)
                    {
                        std::printf("(%ld)",(long)g_srchahead);
                    }
#endif
                    parseheader(g_srchahead);
                    break;
                }
                if (input_pending())
                {
                    break;
                }
            }
            std::fflush(stdout);
        }
    }
    else
#endif // ARTSEARCH
    {
        if (article_next(g_art) <= g_last_art)   // how about a pre-fetch?
        {
            parse_header(article_next(g_art));   // look for the next article
        }
    }
}
#endif // PENDING

// see what else we can do while they are reading

void cache_until_key()
{
    if (!g_in_ng)
    {
        return;
    }
#ifdef PENDING
    if (input_pending())
    {
        return;
    }

    if ((g_data_source->m_flags & DF_REMOTE) && nntp_finish_body(FB_BACKGROUND))
    {
        return;
    }

    g_untrim_cache = true;
    g_sentinel_art_ptr = g_curr_artp;

    // Prioritize our caching based on what mode we're in
    if (g_general_mode == GM_SELECTOR)
    {
        if (cache_subjects())
        {
            if (cache_xrefs())
            {
                if (chase_xrefs(true))
                {
                    if (g_threaded_group)
                    {
                        cache_all_arts();
                    }
                    else
                    {
                        cache_unread_arts();
                    }
                }
            }
        }
    }
    else
    {
        if (!g_threaded_group || cache_all_arts())
        {
            if (cache_subjects())
            {
                if (cache_unread_arts())
                {
                    if (cache_xrefs())
                    {
                        chase_xrefs(true);
                    }
                }
            }
        }
    }

    if (!input_pending() && g_sc_initialized)
    {
        sc_look_ahead(true, true);
    }

    set_spin(SPIN_OFF);
    g_untrim_cache = false;
#endif
    check_data_sources();
}

#ifdef PENDING
bool cache_subjects()
{
    ArticleNum an;

    if (s_subj_to_get > g_last_art)
    {
        return true;
    }
    set_spin(SPIN_BACKGROUND);
    for (an = article_first(s_subj_to_get); an <= g_last_art; an = article_next(an))
    {
        if (input_pending())
        {
            break;
        }

        if (article_unread(an))
        {
            prefetch_subj(an);
        }
    }
    s_subj_to_get = an;
    return s_subj_to_get > g_last_art;
}

static bool cache_xrefs()
{
    ArticleNum an;

    if (g_olden_days || (g_data_source->m_flags & DF_NO_XREFS) || s_xref_to_get > g_last_art)
    {
        return true;
    }
    set_spin(SPIN_BACKGROUND);
    for (an = article_first(s_xref_to_get); an <= g_last_art; an = article_next(an))
    {
        if (input_pending())
        {
            break;
        }
        if (article_unread(an))
        {
            prefetch_xref(an);
        }
    }
    s_xref_to_get = an;
    return s_xref_to_get > g_last_art;
}

static bool cache_all_arts()
{
    ArticleNum old_last_cached = g_last_cached;
    if (!g_cached_all_in_range)
    {
        g_last_cached = article_before(g_first_cached);
    }
    if (g_last_cached >= g_last_art && g_first_cached <= g_abs_first)
    {
        return true;
    }

    // turn it on as late as possible to avoid fseek()ing open art
    set_spin(SPIN_BACKGROUND);
    if (g_last_cached < g_last_art)
    {
        if (g_data_source->m_ov_opened)
        {
            ov_data(article_after(g_last_cached), g_last_art, true);
        }
        if (!art_data(article_after(g_last_cached), g_last_art, true, true))
        {
            g_last_cached = old_last_cached;
            return false;
        }
        g_cached_all_in_range = true;
    }
    if (g_first_cached > g_abs_first)
    {
        if (g_data_source->m_ov_opened)
        {
            ov_data(g_abs_first, article_before(g_first_cached), true);
        }
        else
        {
            art_data(g_abs_first, article_before(g_first_cached), true, true);
        }
        // If we got interrupted, make a quick exit
        if (g_first_cached > g_abs_first)
        {
            g_last_cached = old_last_cached;
            return false;
        }
    }
    // We're all done threading the group, so if the current article is
    // still in doubt, tell them it's missing.
    if (g_curr_artp && !(g_curr_artp->m_flags & AF_CACHED) && !input_pending())
    {
        push_char('\f' | 0200);
    }
    // A completely empty group needs a count & a sort
    if (g_general_mode != GM_SELECTOR && !g_obj_count && !g_selected_only)
    {
        thread_grow();
    }
    return true;
}

static bool cache_unread_arts()
{
    if (g_last_cached >= g_last_art)
    {
        return true;
    }
    set_spin(SPIN_BACKGROUND);
    return art_data(article_after(g_last_cached), g_last_art, true, false);
}
#endif

static bool art_data(ArticleNum first, ArticleNum last, bool cheating, bool all_articles)
{
    ArticleNum i;
    ArticleNum expected_i = first;

    int cache_mask = (g_threaded_group ? AF_THREADED : AF_CACHED)
                  + (all_articles? 0 : AF_UNREAD);
    int cache_mask2 = (all_articles? 0 : AF_UNREAD);

    if (cheating)
    {
        set_spin(SPIN_BACKGROUND);
    }
    else
    {
        int lots_to_do = ((g_data_source->m_flags & DF_REMOTE)? g_net_speed : 20) * 25;
        set_spin(g_spin_estimate > lots_to_do? SPIN_BAR_GRAPH : SPIN_FOREGROUND);
    }
    // TRN_ASSERT(first >= g_abs_first && last <= g_last_art);
    for (i = article_first(first); i <= last; i = article_next(i))
    {
        if ((article_ptr(i)->m_flags & cache_mask) ^ cache_mask2)
        {
            continue;
        }

        g_spin_todo -= value_of(i - expected_i);
        expected_i = article_after(i);

        // This parses the header which will cache/thread the article
        (void) parse_header(i);

        if (g_int_count)
        {
            g_int_count = 0;
            break;
        }
        if (cheating)
        {
            if (input_pending())
            {
                break;
            }
            // If the current article is no longer a '?', let them know.
            if (g_curr_artp != g_sentinel_art_ptr)
            {
                push_char('\f' | 0200);
                break;
            }
        }
    }
    set_spin(SPIN_POP);
    i = std::min(i, last);
    g_last_cached = std::max(i, g_last_cached);
    if (i == last)
    {
        g_first_cached = std::min(first, g_first_cached);
        return true;
    }
    return false;
}

bool cache_range(ArticleNum first, ArticleNum last)
{
    bool success = true;
    bool all_arts = (g_sel_rereading || g_thread_always);
    ArticleNum count{};

    if (g_sel_rereading && !g_cached_all_in_range)
    {
        g_first_cached = first;
        g_last_cached = article_before(first);
    }
    if (first < g_first_cached)
    {
        count = g_first_cached - first;
    }
    if (last > g_last_cached)
    {
        count += last - g_last_cached;
    }
    if (!count)
    {
        return true;
    }
    g_spin_todo = count.value_of();

    if (g_first_cached > g_last_cached)
    {
        if (g_sel_rereading)
        {
            if (g_first_subject)
            {
                count -= ArticleNum{g_newsgroup_ptr->m_to_read};
            }
        }
        else if (first == g_first_art && last == g_last_art && !all_arts)
        {
            count = ArticleNum{g_newsgroup_ptr->m_to_read};
        }
    }
    g_spin_estimate = count.value_of();

    std::printf("\n%sing %ld article%s.", g_threaded_group? "Thread" : "Cach",
           count.value_of(), plural(count.value_of()));
    term_down(1);

    set_spin(SPIN_FOREGROUND);

    if (first < g_first_cached)
    {
        if (g_data_source->m_ov_opened)
        {
            ov_data(g_abs_first, article_before(g_first_cached), false);
            success = (g_first_cached == g_abs_first);
        }
        else
        {
            success = art_data(first, article_before(g_first_cached), false, all_arts);
            g_cached_all_in_range = (all_arts && success);
        }
    }
    if (success && g_last_cached < last)
    {
        if (g_data_source->m_ov_opened)
        {
            ov_data(article_after(g_last_cached), last, false);
        }
        success = art_data(article_after(g_last_cached), last, false, all_arts);
        g_cached_all_in_range = (all_arts && success);
    }
    set_spin(SPIN_POP);
    return success;
}

void Article::set_cached_line(int which_line, std::string_view line)
{
    // SUBJ_LINE is handled specially above
    switch (which_line)
    {
    case FROM_LINE:
    {
        std::string decoded(line.size() + 1, '\0');
        const int   size = decode_header(decoded.data(), line);
        decoded.resize(static_cast<std::size_t>(size));
        m_from = decoded;
        break;
    }

    case XREF_LINE:
    {
        // Exclude an xref for just this group or "(none)".
        const std::size_t first_colon = line.find(':');
        if (first_colon == std::string_view::npos || line.find(':', first_colon + 1) == std::string_view::npos)
        {
            m_xrefs = "";
        }
        else
        {
            m_xrefs = std::string{line};
        }
        break;
    }

    case MSG_ID_LINE:
        m_msg_id = std::string{line};
        break;

    case LINES_LINE:
        m_lines = std::atol(std::string{line}.c_str());
        break;

    case BYTES_LINE:
        m_bytes = std::atol(std::string{line}.c_str());
        break;
    }
}
