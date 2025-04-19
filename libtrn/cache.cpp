/* cache.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/cache.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/bits.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/hash.h"
#include "trn/head.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/mime.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/rt-ov.h"
#include "trn/rt-page.h"
#include "trn/rt-process.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rthread.h"
#include "trn/score.h"
#include "trn/search.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/utf.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

List      *g_article_list{};         // a list of ARTICLEs
Article  **g_art_ptr_list{};         // the article-selector creates this
Article  **g_art_ptr{};              // ditto -- used for article order
ArticleNum g_art_ptr_list_size{};    //
ArticleNum g_search_ahead{};         // are we in subject scan mode? (if so, contains art # found or -1)
ArticleNum g_first_cached{};         //
ArticleNum g_last_cached{};          //
bool       g_cached_all_in_range{};  //
Article   *g_sentinel_art_ptr{};     //
Subject   *g_first_subject{};        //
Subject   *g_last_subject{};         //
bool       g_untrim_cache{};         //
int        g_join_subject_len{};     // -J
int        g_olden_days{};           // -o
char       g_auto_select_postings{}; // -p

#ifdef PENDING
static ArticleNum    s_subj_to_get{};
static ArticleNum    s_xref_to_get{};
static CompiledRegex s_search_compex; // compiled regex for search ahead
#endif
static HashTable *s_subj_hash{};
static HashTable *s_short_subj_hash{};

static void init_article_node(List *list, ListNode *node);
static bool clear_article_item(char *cp, int arg);

void cache_init()
{
#ifdef PENDING
    init_compex(&s_search_compex);
#endif
}

static NewsgroupData* s_cached_ng{};
static std::time_t s_cached_time{};

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
        for (ArticleNum an = g_last_cached + 1; an <= g_last_art; an.num++)
        {
            article_ptr(an)->flags |= AF_EXISTS;
        }
        rc_to_bits();
        g_article_list->high = g_last_art.num;
        thread_grow();
        return;
    }

    close_cache();

    s_cached_ng = g_newsgroup_ptr;
    s_cached_time = std::time(nullptr);
    g_article_list = new_list(g_abs_first.num, g_last_art.num, sizeof (Article), 371,
                              LF_SPARSE, init_article_node);
    s_subj_hash = hash_create(991, subject_cmp); // TODO: pick a better size

    set_first_art(g_newsgroup_ptr->rc_line + g_newsgroup_ptr->num_offset);
    g_first_cached = g_thread_always? g_abs_first : g_first_art;
    g_last_cached = g_first_cached-1;
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
    Subject* next;

    nntp_art_name(ArticleNum{}, false);             // clear the tmp file cache

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
        next = sp->next;
        std::free(sp->str);
        std::free(sp);
    }
    g_first_subject = nullptr;
    g_last_subject = nullptr;
    g_subject_count = 0;                // just to be sure
    g_parsed_art = ArticleNum{};

    if (g_art_ptr_list)
    {
        std::free(g_art_ptr_list);
        g_art_ptr_list = nullptr;
    }
    g_art_ptr = nullptr;
    thread_close();

    if (g_article_list)
    {
        walk_list(g_article_list, clear_article_item, 0);
        delete_list(g_article_list);
        g_article_list = nullptr;
    }
    s_cached_ng = nullptr;
}

// Initialize the memory for an entire node's worth of article's
static void init_article_node(List *list, ListNode *node)
{
    std::memset(node->data, 0, list->items_per_node * list->item_size);
    Article *ap = (Article *) node->data;
    for (ArticleNum i = ArticleNum{node->low}; i <= ArticleNum{node->high}; i.num++, ap++)
    {
        ap->num = i;
    }
}

static bool clear_article_item(char *cp, int arg)
{
    clear_article((Article*)cp);
    return false;
}

// The article has all it's data in place, so add it to the list of articles
// with the same subject.
//
void cache_article(Article *ap)
{
    Article* next;
    Article* ap2;

    if (!(next = ap->subj->articles) || ap->date < next->date)
    {
        ap->subj->articles = ap;
    }
    else
    {
        while ((next = (ap2 = next)->subj_next) && next->date <= ap->date)
        {
        }
        ap2->subj_next = ap;
    }
    ap->subj_next = next;
    ap->flags |= AF_CACHED;

    if (!!(ap->flags & AF_UNREAD) ^ g_sel_rereading)
    {
        if (ap->subj->flags & g_sel_mask)
        {
            select_article(ap, AUTO_KILL_NONE);
        }
        else
        {
            if (ap->subj->flags & SF_WAS_SELECTED)
            {
#if 0
                if (g_selected_only)
                {
                    ap->flags |= g_sel_mask;
                }
                else
#endif
                {
                    select_article(ap, AUTO_KILL_NONE);
                }
            }
            ap->subj->flags |= SF_VISIT;
        }
    }

    if (g_join_subject_len != 0)
    {
        check_for_near_subj(ap);
    }
}

void check_for_near_subj(Article *ap)
{
    Subject* sp;
    if (!s_short_subj_hash)
    {
        s_short_subj_hash = hash_create(401, subject_cmp);    // TODO: pick a better size
        sp = g_first_subject;
    }
    else
    {
        sp = ap->subj;
        if (sp->next)
        {
            sp = nullptr;
        }
    }
    while (sp)
    {
        if ((int) std::strlen(sp->str + 4) >= g_join_subject_len && sp->thread)
        {
            Subject* sp2;
            HashDatum data = hash_fetch(s_short_subj_hash, sp->str + 4, g_join_subject_len);
            if (!(sp2 = (Subject *) data.dat_ptr))
            {
                data.dat_ptr = (char*)sp;
                hash_store_last(data);
            }
            else if (sp->thread != sp2->thread)
            {
                merge_threads(sp2, sp);
            }
        }
        sp = sp->next;
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
        if (len && g_first_subject && g_first_subject->articles)
        {
            check_for_near_subj(g_first_subject->articles);
        }
    }
}

void check_poster(Article *ap)
{
    if (g_auto_select_postings && (ap->flags & AF_EXISTS) && ap->from)
    {
        {
            char* s = g_cmd_buf;
            char* u;
            char* h;
            std::strcpy(s,ap->from);
            if ((h=std::strchr(s,'<')) != nullptr)   // grab the good part
            {
                s = h+1;
                if ((h=std::strchr(s,'>')) != nullptr)
                {
                    *h = '\0';
                }
            }
            else if ((h = std::strchr(s, '(')) != nullptr)
            {
                while (h-- != s && *h == ' ')
                {
                }
                h[1] = '\0';            // or strip the comment
            }
            if ((h = std::strchr(s, '%')) != nullptr || (h = std::strchr(s, '@')))
            {
                *h++ = '\0';
                u = s;
            }
            else if ((u = std::strrchr(s, '!')) != nullptr)
            {
                *u++ = '\0';
                h = s;
            }
            else
            {
                h = s;
                u = s;
            }
            if (!std::strcmp(u, g_login_name.c_str()))
            {
                if (in_string(h, g_host_name, false))
                {
                    switch (g_auto_select_postings)
                    {
                    case '.':
                        select_sub_thread(ap,AUTO_SEL_FOL);
                        break;

                    case '+':
                        select_articles_thread(ap,AUTO_SEL_THD);
                        break;

                    case 'p':
                        if (ap->parent)
                        {
                            select_sub_thread(ap->parent,AUTO_SEL_FOL);
                        }
                        else
                        {
                            select_sub_thread(ap,AUTO_SEL_FOL);
                        }
                        break;
                    }
                }
                else
                {
#ifdef REPLYTO_POSTER_CHECKING
                    char* reply_buf = fetchlines(article_num(ap),REPLY_LINE);
                    if (in_string(reply_buf, g_login_name.c_str(), true))
                    {
                        select_subthread(ap,AUTO_SEL_FOL);
                    }
                    std::free(reply_buf);
#endif
                }
            }
        }
    }
}

// The article turned out to be a duplicate, so remove it from the cached
// list and possibly destroy the subject (should only happen if the data
// was corrupt and the duplicate id got a different subject).
//
void uncache_article(Article *ap, bool remove_empties)
{
    if (ap->subj)
    {
        if (all_bits(ap->flags, AF_CACHED | AF_EXISTS))
        {
            Article *next = ap->subj->articles;
            if (next == ap)
            {
                ap->subj->articles = ap->subj_next;
            }
            else
            {
                while (next)
                {
                    Article *ap2 = next->subj_next;
                    if (ap2 == ap)
                    {
                        next->subj_next = ap->subj_next;
                        break;
                    }
                    next = ap2;
                }
            }
        }
        if (remove_empties && !ap->subj->articles)
        {
            Subject* sp = ap->subj;
            if (sp == g_first_subject)
            {
                g_first_subject = sp->next;
            }
            else
            {
                sp->prev->next = sp->next;
            }
            if (sp == g_last_subject)
            {
                g_last_subject = sp->prev;
            }
            else
            {
                sp->next->prev = sp->prev;
            }
            hash_delete(s_subj_hash, sp->str+4, std::strlen(sp->str+4));
            std::free((char*)sp);
            ap->subj = nullptr;
            g_subject_count--;
        }
    }
    ap->flags2 |= AF2_BOGUS;
    one_missing(ap);
}

// get the header line from an article's cache or parse the article trying

char *fetch_cache(ArticleNum art_num, HeaderLineType which_line, bool fill_cache)
{
    char* s;
    Article* ap;
    bool cached = (g_header_type[which_line].flags & HT_CACHED);

    // article_find() returns a nullptr if the article number value is invalid
    if (!(ap = article_find(art_num)) || !(ap->flags & AF_EXISTS))
    {
        return "";
    }
    if (cached && (s=get_cached_line(ap,which_line,g_untrim_cache)) != nullptr)
    {
        return s;
    }
    if (!fill_cache)
    {
        return nullptr;
    }
    if (!parse_header(art_num))
    {
        return "";
    }
    if (cached)
    {
        return get_cached_line(ap, which_line, g_untrim_cache);
    }
    return nullptr;
}

// Return a pointer to a cached header line for the indicated article.
// Truncated headers (e.g. from a .thread file) are optionally ignored.
//
char *get_cached_line(Article *ap, HeaderLineType which_line, bool no_truncs)
{
    char* s;

    switch (which_line)
    {
    case SUBJ_LINE:
        if (!ap->subj || (no_truncs && (ap->subj->flags & SF_SUBJ_TRUNCATED)))
        {
            s = nullptr;
        }
        else
        {
            s = ap->subj->str + ((ap->flags & AF_HAS_RE) ? 0 : 4);
        }
        break;

    case FROM_LINE:
        s = ap->from;
        break;

    case XREF_LINE:
        s = ap->xrefs;
        break;

    case MSG_ID_LINE:
        s = ap->msg_id;
        break;

    case LINES_LINE:
    {
        static char lines_buf[32];
        std::sprintf(lines_buf, "%ld", ap->lines);
        s = lines_buf;
        break;
    }

    case BYTES_LINE:
    {
        static char bytes_buf[32];
        std::sprintf(bytes_buf, "%ld", ap->bytes);
        s = bytes_buf;
        break;
    }

    default:
        s = nullptr;
        break;
    }
    return s;
}

// subj not yet allocated, so we can tweak it first
void set_subj_line(Article *ap, char *subj, int size)
{
    HashDatum data;
    Subject* sp;
    char* subj_start;

    if (subject_has_re(subj, &subj_start))
    {
        ap->flags |= AF_HAS_RE;
    }
    if ((size -= subj_start - subj) < 0)
    {
        size = 0;
    }

    char *new_subj = safe_malloc(size + 4 + 1);
    std::strcpy(new_subj, "Re: ");
    size = decode_header(new_subj + 4, subj_start, size);

    // Do the Re:-stripping over again, just in case it was encoded.
    if (subject_has_re(new_subj + 4, &subj_start))
    {
        ap->flags |= AF_HAS_RE;
    }
    if (subj_start != new_subj + 4)
    {
        safe_copy(new_subj + 4, subj_start, size);
        if ((size -= subj_start - new_subj - 4) < 0)
        {
            size = 0;
        }
    }
    if (ap->subj && !std::strncmp(ap->subj->str + 4, new_subj + 4, size))
    {
        std::free(new_subj);
        return;
    }

    if (ap->subj)
    {
        // This only happens when we freshen truncated subjects
        hash_delete(s_subj_hash, ap->subj->str+4, std::strlen(ap->subj->str+4));
        std::free(ap->subj->str);
        ap->subj->str = new_subj;
        data.dat_ptr = (char*)ap->subj;
        hash_store(s_subj_hash, new_subj + 4, size, data);
    }
    else
    {
        data = hash_fetch(s_subj_hash, new_subj + 4, size);
        if (!(sp = (Subject *) data.dat_ptr))
        {
            sp = (Subject*)safe_malloc(sizeof (Subject));
            std::memset((char*)sp,0,sizeof (Subject));
            g_subject_count++;
            sp->prev = g_last_subject;
            if (sp->prev != nullptr)
            {
                sp->prev->next = sp;
            }
            else
            {
                g_first_subject = sp;
            }
            g_last_subject = sp;
            sp->str = new_subj;
            sp->thread_link = sp;
            sp->flags = SF_NONE;

            data.dat_ptr = (char*)sp;
            hash_store_last(data);
        }
        else
        {
            std::free(new_subj);
        }
        ap->subj = sp;
    }
}

int decode_header(char *to, char *from, int size)
{
    char *s = to; // save for pass 2
    bool pass2_needed = false;

    // Pass 1 to decode coded bytes (which might be character fragments - so 1 pass is wrong)
    for (int i = size; *from && i--;)
    {
        if (*from == '=' && from[1] == '?')
        {
            char* q = std::strchr(from+2,'?');
            char ch = (q && q[2] == '?')? q[1] : 0;
            char* e;

            if (ch == 'q' || ch == 'Q' || ch == 'b' || ch == 'B')
            {
                const char* old_ics = input_charset_name();
                const char* old_ocs = output_charset_name();
#ifdef USE_UTF_HACK
                *q = '\0';
                utf_init(from+2, CHARSET_NAME_UTF8); // FIXME
                *q = '?';
#endif
                e = q+2;
                do
                {
                    e = std::strchr(e + 1, '?');
                } while (e && e[1] != '=');
                if (e)
                {
                    int len = e - from + 2;
#ifdef USE_UTF_HACK
                    char *d;
#endif
                    i -= len-1;
                    size -= len;
                    q += 3;
                    from = e+2;
                    *e = '\0';
                    if (ch == 'q' || ch == 'Q')
                    {
                        len = qp_decode_string(to, q, true);
                    }
                    else
                    {
                        len = b64_decode_string(to, q);
                    }
#ifdef USE_UTF_HACK
                    d = create_utf8_copy(to);
                    if (d)
                    {
                        for (len = 0; d[len];)
                        {
                            to[len] = d[len];
                            len++;
                        }
                        std::free(d);
                    }
#endif
                    *e = '?';
                    to += len;
                    size += len;
                    // If the next character is whitespace we should eat it now
                    from = skip_hor_space(from);
                }
                else
                {
                    *to++ = *from++;
                }
#ifdef USE_UTF_HACK
                utf_init(old_ics, old_ocs);
#endif
            }
            else
            {
                *to++ = *from++;
            }
        }
        else if (*from != '\n')
        {
            *to++ = *from++;
        }
        else
        {
            from++;
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

// s already allocated, ready to save
void set_cached_line(Article *ap, int which_line, char *s)
{
    char* cp;
    // SUBJ_LINE is handled specially above
    switch (which_line)
    {
    case FROM_LINE:
        if (ap->from)
        {
            std::free(ap->from);
        }
        decode_header(s, s, std::strlen(s));
        ap->from = s;
        break;

    case XREF_LINE:
        if (!empty(ap->xrefs))
        {
            std::free(ap->xrefs);
        }
        // Exclude an xref for just this group or "(none)".
        cp = std::strchr(s, ':');
        if (!cp || !std::strchr(cp + 1, ':'))
        {
            std::free(s);
            s = save_str("");
        }
        ap->xrefs = s;
        break;

    case MSG_ID_LINE:
        if (ap->msg_id)
        {
            std::free(ap->msg_id);
        }
        ap->msg_id = s;
        break;

    case LINES_LINE:
        ap->lines = std::atol(s);
        break;

    case BYTES_LINE:
        ap->bytes = std::atol(s);
        break;
    }
}

int subject_cmp(const char *key, int key_len, HashDatum data)
{
    // We already know that the lengths are equal, just compare the strings
    return std::memcmp(key, ((Subject*)data.dat_ptr)->str+4, key_len);
}

// see what we can do while they are reading

#ifdef PENDING
void look_ahead()
{
#ifdef ARTSEARCH
    char* h;
    char* s;

#ifdef DEBUG
    if (debug && g_srchahead)
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
        if (debug & DEB_SEARCH_AHEAD)
        {
            std::fputs("(hit CR)",stdout);
            std::fflush(stdout);
            std::fgets(g_buf+128, sizeof g_buf-128, stdin);
            std::printf("\npattern = %s\n",pattern);
            term_down(2);
        }
#endif
        s = compile(&s_srchcompex, pattern, true, true);
        if (s != nullptr)
        {
                                    // compile regular expression
            std::printf("\n%s\n",s);
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
                    if (debug)
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
                    if (debug)
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

    if ((g_data_source->flags & DF_REMOTE) && nntp_finish_body(FB_BACKGROUND))
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
            fetch_subj(an, false);
        }
    }
    s_subj_to_get = an;
    return s_subj_to_get > g_last_art;
}

bool cache_xrefs()
{
    ArticleNum an;

    if (g_olden_days || (g_data_source->flags & DF_NO_XREFS) || s_xref_to_get > g_last_art)
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
            fetch_xref(an, false);
        }
    }
    s_xref_to_get = an;
    return s_xref_to_get > g_last_art;
}

bool cache_all_arts()
{
    ArticleNum old_last_cached = g_last_cached;
    if (!g_cached_all_in_range)
    {
        g_last_cached = g_first_cached - 1;
    }
    if (g_last_cached >= g_last_art && g_first_cached <= g_abs_first)
    {
        return true;
    }

    // turn it on as late as possible to avoid fseek()ing open art
    set_spin(SPIN_BACKGROUND);
    if (g_last_cached < g_last_art)
    {
        if (g_data_source->ov_opened)
        {
            ov_data(g_last_cached + 1, g_last_art, true);
        }
        if (!art_data(g_last_cached + 1, g_last_art, true, true))
        {
            g_last_cached = old_last_cached;
            return false;
        }
        g_cached_all_in_range = true;
    }
    if (g_first_cached > g_abs_first)
    {
        if (g_data_source->ov_opened)
        {
            ov_data(g_abs_first, g_first_cached - 1, true);
        }
        else
        {
            art_data(g_abs_first, g_first_cached - 1, true, true);
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
    if (g_curr_artp && !(g_curr_artp->flags & AF_CACHED) && !input_pending())
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

bool cache_unread_arts()
{
    if (g_last_cached >= g_last_art)
    {
        return true;
    }
    set_spin(SPIN_BACKGROUND);
    return art_data(g_last_cached+1, g_last_art, true, false);
}
#endif

bool art_data(ArticleNum first, ArticleNum last, bool cheating, bool all_articles)
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
        int lots_to_do = ((g_data_source->flags & DF_REMOTE)? g_net_speed : 20) * 25;
        set_spin(g_spin_estimate > lots_to_do? SPIN_BAR_GRAPH : SPIN_FOREGROUND);
    }
    // TRN_ASSERT(first >= g_abs_first && last <= g_last_art);
    for (i = article_first(first); i <= last; i = article_next(i))
    {
        if ((article_ptr(i)->flags & cache_mask) ^ cache_mask2)
        {
            continue;
        }

        g_spin_todo -= i.num - expected_i.num;
        expected_i = i + 1;

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
        g_last_cached = first-1;
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
    g_spin_todo = count.num;

    if (g_first_cached > g_last_cached)
    {
        if (g_sel_rereading)
        {
            if (g_first_subject)
            {
                count.num -= g_newsgroup_ptr->to_read;
            }
        }
        else if (first == g_first_art && last == g_last_art && !all_arts)
        {
            count.num = g_newsgroup_ptr->to_read;
        }
    }
    g_spin_estimate = count.num;

    std::printf("\n%sing %ld article%s.", g_threaded_group? "Thread" : "Cach",
           (long)count.num, plural(count.num));
    term_down(1);

    set_spin(SPIN_FOREGROUND);

    if (first < g_first_cached)
    {
        if (g_data_source->ov_opened)
        {
            ov_data(g_abs_first,g_first_cached-1,false);
            success = (g_first_cached == g_abs_first);
        }
        else
        {
            success = art_data(first, g_first_cached-1, false, all_arts);
            g_cached_all_in_range = (all_arts && success);
        }
    }
    if (success && g_last_cached < last)
    {
        if (g_data_source->ov_opened)
        {
            ov_data(g_last_cached + 1, last, false);
        }
        success = art_data(g_last_cached+1, last, false, all_arts);
        g_cached_all_in_range = (all_arts && success);
    }
    set_spin(SPIN_POP);
    return success;
}

void clear_article(Article *ap)
{
    if (ap->from)
    {
        std::free(ap->from);
    }
    if (ap->msg_id)
    {
        std::free(ap->msg_id);
    }
    if (!empty(ap->xrefs))
    {
        std::free(ap->xrefs);
    }
}
