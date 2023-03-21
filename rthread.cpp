/* rthread.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "intrp.h"
#include "trn.h"
#include "hash.h"
#include "cache.h"
#include "bits.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "ng.h"
#include "rcln.h"
#include "search.h"
#include "artstate.h"
#include "rcstuff.h"
#include "final.h"
#include "kfile.h"
#include "head.h"
#include "util.h"
#include "util2.h"
#include "rt-mt.h"
#include "rt-ov.h"
#include "score.h"
#include "rt-page.h"
#include "rt-process.h"
#include "rt-select.h"
#include "rt-util.h"
#include "rt-wumpus.h"
#include "INTERN.h"
#include "rthread.h"

ART_NUM obj_count{};
int subject_count{};
bool output_chase_phrase{};
HASHTABLE *msgid_hash{};

static int cleanup_msgid_hash(int keylen, HASHDATUM *data, int extra);
static ARTICLE *first_sib(ARTICLE *ta, int depth);
static ARTICLE *last_sib(ARTICLE *ta, int depth, ARTICLE *limit);
static int subjorder_subject(const SUBJECT **spp1, const SUBJECT **spp2);
static int subjorder_date(const SUBJECT **spp1, const SUBJECT **spp2);
static int subjorder_count(const SUBJECT **spp1, const SUBJECT **spp2);
static int subjorder_lines(const SUBJECT **spp1, const SUBJECT **spp2);
static int subject_score_high(const SUBJECT *);
static int subjorder_score(const SUBJECT **spp1, const SUBJECT **spp2);
static int threadorder_subject(const SUBJECT **spp1, const SUBJECT **spp2);
static int threadorder_date(const SUBJECT **spp1, const SUBJECT **spp2);
static int threadorder_count(const SUBJECT **spp1, const SUBJECT **spp2);
static int threadorder_lines(const SUBJECT **spp1, const SUBJECT **spp2);
static int thread_score_high(const SUBJECT *tp);
static int threadorder_score(const SUBJECT **spp1, const SUBJECT **spp2);
static int artorder_date(const ARTICLE **art1, const ARTICLE **art2);
static int artorder_subject(const ARTICLE **art1, const ARTICLE **art2);
static int artorder_author(const ARTICLE **art1, const ARTICLE **art2);
static int artorder_number(const ARTICLE **art1, const ARTICLE **art2);
static int artorder_groups(const ARTICLE **art1, const ARTICLE **art2);
static int artorder_lines(const ARTICLE **art1, const ARTICLE **art2);
static int artorder_score(const ARTICLE **art1, const ARTICLE **art2);
static void build_artptrs();

void thread_init()
{
}

/* Generate the thread data we need for this group.  We must call
** thread_close() before calling this again.
*/
void thread_open()
{
    bool find_existing = !g_first_subject;
    time_t save_ov_opened;
    ARTICLE* ap;

    if (!msgid_hash)
	msgid_hash = hashcreate(1999, msgid_cmp); /*TODO: pick a better size */
    if (g_threaded_group) {
	/* Parse input and use msgid_hash for quick article lookups. */
	/* If cached but not threaded articles exist, set up to thread them. */
	if (g_first_subject) {
	    g_first_cached = g_firstart;
	    g_last_cached = g_firstart - 1;
	    g_parsed_art = 0;
	}
    }

    if (g_sel_mode == SM_ARTICLE)
	set_selector(g_sel_mode, g_sel_artsort);
    else
	set_selector(g_sel_threadmode, g_sel_threadsort);

    if ((g_datasrc->flags & DF_TRY_THREAD) && !g_first_subject) {
	if (mt_data() < 0)
	    return;
    }
    if ((g_datasrc->flags & DF_TRY_OVERVIEW) && !g_cached_all_in_range) {
	if (thread_always) {
	    spin_todo = spin_estimate = g_lastart - g_absfirst + 1;
	    (void) ov_data(g_absfirst, g_lastart, false);
	    if (g_datasrc->ov_opened && find_existing && g_datasrc->over_dir == nullptr) {
		mark_missing_articles();
		rc_to_bits();
		find_existing = false;
	    }
	}
	else {
	    spin_todo = g_lastart - g_firstart + 1;
	    spin_estimate = g_ngptr->toread;
	    if (g_firstart > g_lastart) {
		/* If no unread articles, see if ov. exists as fast as possible */
		(void) ov_data(g_absfirst, g_absfirst, false);
		g_cached_all_in_range = false;
	    } else
		(void) ov_data(g_firstart, g_lastart, false);
	}
    }
    if (find_existing) {
	find_existing_articles();
	mark_missing_articles();
	rc_to_bits();
    }

    for (ap = article_ptr(article_first(g_firstart));
	 ap && article_num(ap) <= g_last_cached;
	 ap = article_nextp(ap))
    {
	if ((ap->flags & (AF_EXISTS|AF_CACHED)) == AF_EXISTS)
	    (void) parseheader(article_num(ap));
    }

    if (g_last_cached > g_lastart) {
	g_ngptr->toread += (ART_UNREAD)(g_last_cached-g_lastart);
	/* ensure getngsize() knows the new maximum */
	g_ngptr->ngmax = g_lastart = g_last_cached;
    }
    g_article_list->high = g_lastart;

    for (ap = article_ptr(article_first(g_absfirst));
	 ap && article_num(ap) <= g_lastart;
	 ap = article_nextp(ap))
    {
	if (ap->flags & AF_CACHED)
	    check_poster(ap);
    }

    save_ov_opened = g_datasrc->ov_opened;
    g_datasrc->ov_opened = 0; /* avoid trying to call ov_data twice for high arts */
    thread_grow();	    /* thread any new articles not yet in the database */
    g_datasrc->ov_opened = save_ov_opened;
    g_added_articles = 0;
    g_sel_page_sp = 0;
    g_sel_page_app = 0;
}

/* Update the group's thread info.
*/
void thread_grow()
{
    g_added_articles += g_lastart - g_last_cached;
    if (g_added_articles && thread_always)
	cache_range(g_last_cached + 1, g_lastart);
    count_subjects(CS_NORM);
    if (g_artptr_list)
	sort_articles();
    else
	sort_subjects();
}

void thread_close()
{
    g_curr_artp = g_artp = nullptr;
    init_tree();			/* free any tree lines */

    update_thread_kfile();
    if (msgid_hash)
	hashwalk(msgid_hash, cleanup_msgid_hash, 0);
    g_sel_page_sp = 0;
    g_sel_page_app = 0;
    g_sel_last_ap = 0;
    g_sel_last_sp = 0;
    g_selected_only = false;
    g_sel_exclusive = false;
    ov_close();
}

static int cleanup_msgid_hash(int keylen, HASHDATUM *data, int extra)
{
    ARTICLE* ap = (ARTICLE*)data->dat_ptr;
    int ret = -1;

    if (ap) {
	if (data->dat_len)
	    return 0;
	if ((g_kf_state & KFS_GLOBAL_THREADFILE) && ap->autofl) {
	    data->dat_ptr = ap->msgid;
	    data->dat_len = ap->autofl;
	    ret = 0;
	    ap->msgid = nullptr;
	}
	if (ap->flags & AF_TMPMEM) {
	    clear_article(ap);
	    free((char*)ap);
	}
    }
    return ret;
}

void top_article()
{
    g_art = g_lastart+1;
    g_artp = nullptr;
    inc_art(g_selected_only, false);
    if (g_art > g_lastart && g_last_cached < g_lastart)
	g_art = g_firstart;
}

ARTICLE *first_art(SUBJECT *sp)
{
    ARTICLE* ap = (g_threaded_group? sp->thread : sp->articles);
    if (ap && !(ap->flags & AF_EXISTS)) {
	oneless(ap);
	ap = next_art(ap);
    }
    return ap;
}

ARTICLE *last_art(SUBJECT *sp)
{
    ARTICLE* ap;

    if (!g_threaded_group) {
	ap = sp->articles;
	while (ap->subj_next)
	    ap = ap->subj_next;
	return ap;
    }

    ap = sp->thread;
    if (ap) {
	for (;;) {
	    if (ap->sibling)
		ap = ap->sibling;
	    else if (ap->child1)
		ap = ap->child1;
	    else
		break;
	}
	if (!(ap->flags & AF_EXISTS)) {
	    oneless(ap);
	    ap = prev_art(ap);
	}
    }
    return ap;
}

/* Bump g_art/g_artp to the next article, wrapping from thread to thread.
** If sel_flag is true, only stops at selected articles.
** If rereading is false, only stops at unread articles.
*/
void inc_art(bool sel_flag, bool rereading)
{
    ARTICLE* ap = g_artp;
    int subj_mask = (rereading? 0 : SF_VISIT);

    /* Use the explicit article-order if it exists */
    if (g_artptr_list) {
	ARTICLE** limit = g_artptr_list + g_artptr_list_size;
	if (!ap)
	    g_artptr = g_artptr_list-1;
	else if (!g_artptr || *g_artptr != ap) {
	    for (g_artptr = g_artptr_list; g_artptr < limit; g_artptr++) {
		if (*g_artptr == ap)
		    break;
	    }
	}
	do {
	    if (++g_artptr >= limit)
		break;
	    ap = *g_artptr;
	} while ((!rereading && !(ap->flags & AF_UNREAD))
	      || (sel_flag && !(ap->flags & AF_SEL)));
	if (g_artptr < limit) {
	    g_artp = *g_artptr;
	    g_art = article_num(g_artp);
	} else {
	    g_artp = nullptr;
	    g_art = g_lastart+1;
	    g_artptr = g_artptr_list;
	}
	return;
    }

    /* Use subject- or thread-order when possible */
    if (g_threaded_group || g_srchahead) {
	SUBJECT* sp;
	if (ap)
	    sp = ap->subj;
	else
	    sp = next_subj((SUBJECT*)nullptr, subj_mask);
	if (!sp)
	    goto num_inc;
	do {
	    if (ap)
		ap = next_art(ap);
	    else
		ap = first_art(sp);
	    while (!ap) {
		sp = next_subj(sp, subj_mask);
		if (!sp)
		    break;
		ap = first_art(sp);
	    }
	} while (ap && ((!rereading && !(ap->flags & AF_UNREAD))
		     || (sel_flag && !(ap->flags & AF_SEL))));
	if ((g_artp = ap) != nullptr)
	    g_art = article_num(ap);
	else {
	    if (g_art <= g_last_cached)
		g_art = g_last_cached+1;
	    else
		g_art++;
	    if (g_art <= g_lastart)
		g_artp = article_ptr(g_art);
	    else
		g_art = g_lastart+1;
	}
	return;
    }

    /* Otherwise, just increment through the art numbers */
  num_inc:
    if (!ap)
      g_art = g_firstart-1;
    for (;;) {
	g_art = article_next(g_art);
	if (g_art > g_lastart) {
	    g_art = g_lastart+1;
	    ap = nullptr;
	    break;
	}
	ap = article_ptr(g_art);
	if (!(ap->flags & AF_EXISTS))
	    oneless(ap);
	else if ((rereading || (ap->flags & AF_UNREAD))
	      && (!sel_flag || (ap->flags & AF_SEL)))
	    break;
    } 
    g_artp = ap;
}

/* Bump g_art/g_artp to the previous article, wrapping from thread to thread.
** If sel_flag is true, only stops at selected articles.
** If rereading is false, only stops at unread articles.
*/
void dec_art(bool sel_flag, bool rereading)
{
    ARTICLE* ap = g_artp;
    int subj_mask = (rereading? 0 : SF_VISIT);

    /* Use the explicit article-order if it exists */
    if (g_artptr_list) {
	ARTICLE** limit = g_artptr_list + g_artptr_list_size;
	if (!ap)
	    g_artptr = limit;
	else if (!g_artptr || *g_artptr != ap) {
	    for (g_artptr = g_artptr_list; g_artptr < limit; g_artptr++) {
		if (*g_artptr == ap)
		    break;
	    }
	}
	do {
	    if (g_artptr == g_artptr_list)
		break;
	    ap = *--g_artptr;
	} while ((!rereading && !(ap->flags & AF_UNREAD))
	      || (sel_flag && !(ap->flags & AF_SEL)));
	g_artp = *g_artptr;
	g_art = article_num(g_artp);
	return;
    }

    /* Use subject- or thread-order when possible */
    if (g_threaded_group || g_srchahead) {
	SUBJECT* sp;
	if (ap)
	    sp = ap->subj;
	else
	    sp = prev_subj((SUBJECT*)nullptr, subj_mask);
	if (!sp)
	    goto num_dec;
	do {
	    if (ap)
		ap = prev_art(ap);
	    else
		ap = last_art(sp);
	    while (!ap) {
		sp = prev_subj(sp, subj_mask);
		if (!sp)
		    break;
		ap = last_art(sp);
	    }
	} while (ap && ((!rereading && !(ap->flags & AF_UNREAD))
		     || (sel_flag && !(ap->flags & AF_SEL))));
	if ((g_artp = ap) != nullptr)
	    g_art = article_num(ap);
	else
	    g_art = g_absfirst-1;
	return;
    }

    /* Otherwise, just decrement through the art numbers */
  num_dec:
    for (;;) {
	g_art = article_prev(g_art);
	if (g_art < g_absfirst) {
	    g_art = g_absfirst-1;
	    ap = nullptr;
	    break;
	}
	ap = article_ptr(g_art);
	if (!(ap->flags & AF_EXISTS))
	    oneless(ap);
	else if ((rereading || (ap->flags & AF_UNREAD))
	      && (!sel_flag || (ap->flags & AF_SEL)))
	    break;
    }
    g_artp = ap;
}

/* Bump the param to the next article in depth-first order.
*/
ARTICLE *bump_art(ARTICLE *ap)
{
    if (ap->child1)
	return ap->child1;
    while (!ap->sibling) {
	if (!(ap = ap->parent))
	    return nullptr;
    }
    return ap->sibling;
}

/* Bump the param to the next REAL article.  Uses subject order in a
** non-threaded group; honors the breadth_first flag in a threaded one.
*/
ARTICLE *next_art(ARTICLE *ap)
{
try_again:
    if (!g_threaded_group) {
	ap = ap->subj_next;
	goto done;
    }
    if (breadth_first) {
	if (ap->sibling) {
	    ap = ap->sibling;
	    goto done;
	}
	if (ap->parent)
	    ap = ap->parent->child1;
	else
	    ap = ap->subj->thread;
    }
    do {
	if (ap->child1) {
	    ap = ap->child1;
	    goto done;
	}
	while (!ap->sibling) {
	    if (!(ap = ap->parent))
		return nullptr;
	}
	ap = ap->sibling;
    } while (breadth_first);
done:
    if (ap && !(ap->flags & AF_EXISTS)) {
	oneless(ap);
	goto try_again;
    }
    return ap;
}

/* Bump the param to the previous REAL article.  Uses subject order in a
** non-threaded group.
*/
ARTICLE *prev_art(ARTICLE *ap)
{
    ARTICLE* initial_ap;

try_again:
    initial_ap = ap;
    if (!g_threaded_group) {
	if ((ap = ap->subj->articles) == initial_ap)
	    ap = nullptr;
	else
	    while (ap->subj_next != initial_ap)
		ap = ap->subj_next;
	goto done;
    }
    ap = (ap->parent ? ap->parent->child1 : ap->subj->thread);
    if (ap == initial_ap) {
	ap = ap->parent;
	goto done;
    }
    while (ap->sibling != initial_ap)
	ap = ap->sibling;
    while (ap->child1) {
	ap = ap->child1;
	while (ap->sibling)
	    ap = ap->sibling;
    }
done:
    if (ap && !(ap->flags & AF_EXISTS)) {
	oneless(ap);
	goto try_again;
    }
    return ap;
}

/* Find the next g_art/g_artp with the same subject as this one.  Returns
** false if no such article exists.
*/
bool next_art_with_subj()
{
    ARTICLE* ap = g_artp;

    if (!ap)
	return false;

    do {
	ap = ap->subj_next;
	if (!ap) {
	    if (!g_art)
		g_art = g_firstart;
	    return false;
	}
    } while (!ALLBITS(ap->flags, AF_EXISTS | AF_UNREAD)
	  || (g_selected_only && !(ap->flags & AF_SEL)));
    g_artp = ap;
    g_art = article_num(ap);
    g_srchahead = -1;
    return true;
}

/* Find the previous g_art/g_artp with the same subject as this one.  Returns
** false if no such article exists.
*/
bool prev_art_with_subj()
{
    ARTICLE* ap = g_artp;
    ARTICLE* ap2;

    if (!ap)
	return false;

    do {
	ap2 = ap->subj->articles;
	if (ap2 == ap)
	    ap = nullptr;
	else {
	    while (ap2 && ap2->subj_next != ap)
		ap2 = ap2->subj_next;
	    ap = ap2;
	}
	if (!ap) {
	    if (!g_art)
		g_art = g_lastart;
	    return false;
	}
    } while (!(ap->flags & AF_EXISTS)
	  || (g_selected_only && !(ap->flags & AF_SEL)));
    g_artp = ap;
    g_art = article_num(ap);
    return true;
}

SUBJECT *next_subj(SUBJECT *sp, int subj_mask)
{
    if (!sp)
	sp = g_first_subject;
    else if (g_sel_mode == SM_THREAD) {
	ARTICLE* ap = sp->thread;
	do {
	    sp = sp->next;
	} while (sp && sp->thread == ap);
    }
    else
	sp = sp->next;

    while (sp && (sp->flags & subj_mask) != subj_mask) {
	sp = sp->next;
    }
    return sp;
}

SUBJECT *prev_subj(SUBJECT *sp, int subj_mask)
{
    if (!sp)
	sp = g_last_subject;
    else if (g_sel_mode == SM_THREAD) {
	ARTICLE* ap = sp->thread;
	do {
	    sp = sp->prev;
	} while (sp && sp->thread == ap);
    }
    else
	sp = sp->prev;

    while (sp && (sp->flags & subj_mask) != subj_mask) {
	sp = sp->prev;
    }
    return sp;
}

/* Select a single article.
*/
void select_article(ARTICLE *ap, int auto_flags)
{
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    bool echo = (auto_flags & ALSO_ECHO) != 0;
    auto_flags &= AUTO_SELS;
    if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == desired_flags) {
	if (!(ap->flags & g_sel_mask)) {
	    g_selected_count++;
	    if (verbose && echo && gmode != 's')
		fputs("\tSelected",stdout);
	}
	ap->flags = (ap->flags & ~AF_DEL) | g_sel_mask;
    }
    if (auto_flags)
	change_auto_flags(ap, auto_flags);
    if (ap->subj) {
	if (!(ap->subj->flags & g_sel_mask))
	    g_selected_subj_cnt++;
	ap->subj->flags = (ap->subj->flags&~SF_DEL) | g_sel_mask | SF_VISIT;
    }
    g_selected_only = (g_selected_only || g_selected_count != 0);
}

/* Select this article's subject.
*/
void select_arts_subject(ARTICLE *ap, int auto_flags)
{
    if (ap->subj && ap->subj->articles)
	select_subject(ap->subj, auto_flags);
    else
	select_article(ap, auto_flags);
}

/* Select all the articles in a subject.
*/
void select_subject(SUBJECT *subj, int auto_flags)
{
    ARTICLE* ap;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int old_count = g_selected_count;

    auto_flags &= AUTO_SELS;
    for (ap = subj->articles; ap; ap = ap->subj_next) {
	if ((ap->flags & (AF_EXISTS|AF_UNREAD|g_sel_mask)) == desired_flags) {
	    ap->flags |= g_sel_mask;
	    g_selected_count++;
	}
	if (auto_flags)
	    change_auto_flags(ap, auto_flags);
    }
    if (g_selected_count > old_count) {
	if (!(subj->flags & g_sel_mask))
	    g_selected_subj_cnt++;
	subj->flags = (subj->flags & ~SF_DEL)
		    | g_sel_mask | SF_VISIT | SF_WASSELECTED;
	g_selected_only = true;
    } else
	subj->flags |= SF_WASSELECTED;
}

/* Select this article's thread.
*/
void select_arts_thread(ARTICLE *ap, int auto_flags)
{
    if (ap->subj && ap->subj->thread)
	select_thread(ap->subj->thread, auto_flags);
    else
	select_arts_subject(ap, auto_flags);
}

/* Select all the articles in a thread.
*/
void select_thread(ARTICLE *thread, int auto_flags)
{
    SUBJECT* sp;

    sp = thread->subj;
    do {
	select_subject(sp, auto_flags);
	sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Select the subthread attached to this article.
*/
void select_subthread(ARTICLE *ap, int auto_flags)
{
    ARTICLE* limit;
    SUBJECT* subj;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int old_count = g_selected_count;

    if (!ap)
	return;
    subj = ap->subj;
    for (limit = ap; limit; limit = limit->parent) {
	if (limit->sibling) {
	    limit = limit->sibling;
	    break;
	}
    }

    auto_flags &= AUTO_SELS;
    for (; ap != limit; ap = bump_art(ap)) {
	if ((ap->flags & (AF_EXISTS|AF_UNREAD|g_sel_mask)) == desired_flags) {
	    ap->flags |= g_sel_mask;
	    g_selected_count++;
	}
	if (auto_flags)
	    change_auto_flags(ap, auto_flags);
    }
    if (subj && g_selected_count > old_count) {
	if (!(subj->flags & g_sel_mask))
	    g_selected_subj_cnt++;
	subj->flags = (subj->flags & ~SF_DEL) | g_sel_mask | SF_VISIT;
	g_selected_only = true;
    }
}

/* Deselect a single article.
*/
void deselect_article(ARTICLE *ap, int auto_flags)
{
    bool echo = (auto_flags & ALSO_ECHO) != 0;
    auto_flags &= AUTO_SELS;
    if (ap->flags & g_sel_mask) {
	ap->flags &= ~g_sel_mask;
	if (!g_selected_count--)
	    g_selected_count = 0;
	if (verbose && echo && gmode != 's')
	    fputs("\tDeselected",stdout);
    }
    if (g_sel_rereading && g_sel_mode == SM_ARTICLE)
	ap->flags |= AF_DEL;
}

/* Deselect this article's subject.
*/
void deselect_arts_subject(ARTICLE *ap)
{
    if (ap->subj && ap->subj->articles)
	deselect_subject(ap->subj);
    else
	deselect_article(ap, 0);
}

/* Deselect all the articles in a subject.
*/
void deselect_subject(SUBJECT *subj)
{
    ARTICLE* ap;

    for (ap = subj->articles; ap; ap = ap->subj_next) {
	if (ap->flags & g_sel_mask) {
	    ap->flags &= ~g_sel_mask;
	    if (!g_selected_count--)
		g_selected_count = 0;
	}
    }
    if (subj->flags & g_sel_mask) {
	subj->flags &= ~g_sel_mask;
	g_selected_subj_cnt--;
    }
    subj->flags &= ~(SF_VISIT | SF_WASSELECTED);
    if (g_sel_rereading)
	subj->flags |= SF_DEL;
    else
	subj->flags &= ~SF_DEL;
}

/* Deselect this article's thread.
*/
void deselect_arts_thread(ARTICLE *ap)
{
    if (ap->subj && ap->subj->thread)
	deselect_thread(ap->subj->thread);
    else
	deselect_arts_subject(ap);
}

/* Deselect all the articles in a thread.
*/
void deselect_thread(ARTICLE *thread)
{
    SUBJECT* sp;

    sp = thread->subj;
    do {
	deselect_subject(sp);
	sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Deselect everything.
*/
void deselect_all()
{
    SUBJECT* sp;

    for (sp = g_first_subject; sp; sp = sp->next)
	deselect_subject(sp);
    g_selected_count = g_selected_subj_cnt = 0;
    g_sel_page_sp = 0;
    g_sel_page_app = 0;
    g_sel_last_ap = 0;
    g_sel_last_sp = 0;
    g_selected_only = false;
}

/* Kill all unread articles attached to this article's subject.
*/
void kill_arts_subject(ARTICLE *ap, int auto_flags)
{
    if (ap->subj && ap->subj->articles)
	kill_subject(ap->subj, auto_flags);
    else {
	if (auto_flags & SET_TORETURN)
	    delay_unmark(ap);
	set_read(ap);
	auto_flags &= AUTO_KILLS;
	if (auto_flags)
	    change_auto_flags(ap, auto_flags);
    }
}

/* Kill all unread articles attached to the given subject.
*/
void kill_subject(SUBJECT *subj, int auto_flags)
{
    ARTICLE* ap;
    int killmask = (auto_flags & AFFECT_ALL)? 0 : g_sel_mask;
    char toreturn = (auto_flags & SET_TORETURN) != 0;

    auto_flags &= AUTO_KILLS;
    for (ap = subj->articles; ap; ap = ap->subj_next) {
	if (toreturn)
	    delay_unmark(ap);
	if ((ap->flags & (AF_UNREAD|killmask)) == AF_UNREAD)
	    set_read(ap);
	if (auto_flags)
	    change_auto_flags(ap, auto_flags);
    }
    subj->flags &= ~(SF_VISIT | SF_WASSELECTED);
}

/* Kill all unread articles attached to this article's thread.
*/
void kill_arts_thread(ARTICLE *ap, int auto_flags)
{
    if (ap->subj && ap->subj->thread)
	kill_thread(ap->subj->thread, auto_flags);
    else
	kill_arts_subject(ap, auto_flags);
}

/* Kill all unread articles attached to the given thread.
*/
void kill_thread(ARTICLE *thread, int auto_flags)
{
    SUBJECT* sp;

    sp = thread->subj;
    do {
	kill_subject(sp, auto_flags);
	sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Kill the subthread attached to this article.
*/
void kill_subthread(ARTICLE *ap, int auto_flags)
{
    ARTICLE* limit;
    char toreturn = (auto_flags & SET_TORETURN) != 0;

    if (!ap)
	return;
    for (limit = ap; limit; limit = limit->parent) {
	if (limit->sibling) {
	    limit = limit->sibling;
	    break;
	}
    }

    auto_flags &= AUTO_KILLS;
    for (; ap != limit; ap = bump_art(ap)) {
	if (toreturn)
	    delay_unmark(ap);
	if (ALLBITS(ap->flags, AF_EXISTS | AF_UNREAD))
	    set_read(ap);
	if (auto_flags)
	    change_auto_flags(ap, auto_flags);
    }
}

/* Unkill all the articles attached to the given subject.
*/
void unkill_subject(SUBJECT *subj)
{
    ARTICLE* ap;
    int save_sel_count = g_selected_count;

    for (ap = subj->articles; ap; ap = ap->subj_next) {
	if (g_sel_rereading) {
	    if (ALLBITS(ap->flags, AF_DELSEL | AF_EXISTS)) {
		if (!(ap->flags & AF_UNREAD))
		    g_ngptr->toread++;
		if (g_selected_only && !(ap->flags & AF_SEL))
		    g_selected_count++;
		ap->flags = (ap->flags & ~AF_DELSEL) | AF_SEL|AF_UNREAD;
	    } else
		ap->flags &= ~(AF_DEL|AF_DELSEL);
	} else {
	    if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == AF_EXISTS)
		onemore(ap);
	    if (g_selected_only && (ap->flags & (AF_SEL|AF_UNREAD)) == AF_UNREAD) {
		ap->flags = (ap->flags & ~AF_DEL) | AF_SEL;
		g_selected_count++;
	    }
	}
    }
    if (g_selected_count != save_sel_count
     && g_selected_only && !(subj->flags & SF_SEL)) {
	subj->flags |= SF_SEL | SF_VISIT | SF_WASSELECTED;
	g_selected_subj_cnt++;
    }
    subj->flags &= ~(SF_DEL|SF_DELSEL);
}

/* Unkill all the articles attached to the given thread.
*/
void unkill_thread(ARTICLE *thread)
{
    SUBJECT* sp;

    sp = thread->subj;
    do {
	unkill_subject(sp);
	sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Unkill the subthread attached to this article.
*/
void unkill_subthread(ARTICLE *ap)
{
    ARTICLE* limit;
    SUBJECT* sp;

    if (!ap)
	return;
    for (limit = ap; limit; limit = limit->parent) {
	if (limit->sibling) {
	    limit = limit->sibling;
	    break;
	}
    }

    sp = ap->subj;
    for (; ap != limit; ap = bump_art(ap)) {
	if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == AF_EXISTS)
	    onemore(ap);
	if (g_selected_only && !(ap->flags & AF_SEL)) {
	    ap->flags |= AF_SEL;
	    g_selected_count++;
	}
    }
    if (!(sp->flags & g_sel_mask))
	g_selected_subj_cnt++;
    sp->flags = (sp->flags & ~SF_DEL) | SF_SEL | SF_VISIT;
    g_selected_only = (g_selected_only || g_selected_count != 0);
}

/* Clear the auto flags in all unread articles attached to the given subject.
*/
void clear_subject(SUBJECT *subj)
{
    ARTICLE* ap;

    for (ap = subj->articles; ap; ap = ap->subj_next)
	clear_auto_flags(ap);
}

/* Clear the auto flags in all unread articles attached to the given thread.
*/
void clear_thread(ARTICLE *thread)
{
    SUBJECT* sp;

    sp = thread->subj;
    do {
	clear_subject(sp);
	sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Clear the auto flags in the subthread attached to this article.
*/
void clear_subthread(ARTICLE *ap)
{
    ARTICLE* limit;

    if (!ap)
	return;
    for (limit = ap; limit; limit = limit->parent) {
	if (limit->sibling) {
	    limit = limit->sibling;
	    break;
	}
    }

    for (; ap != limit; ap = bump_art(ap))
	clear_auto_flags(ap);
}

ARTICLE *subj_art(SUBJECT *sp)
{
    ARTICLE* ap = nullptr;
    int art_mask = (g_selected_only? (AF_SEL|AF_UNREAD) : AF_UNREAD);
    bool TG_save = g_threaded_group;

    g_threaded_group = (g_sel_mode == SM_THREAD);
    ap = first_art(sp);
    while (ap && (ap->flags & art_mask) != art_mask)
	ap = next_art(ap);
    if (!ap) {
	g_reread = true;
	ap = first_art(sp);
	if (g_selected_only) {
	    while (ap && !(ap->flags & AF_SEL))
		ap = next_art(ap);
	    if (!ap)
		ap = first_art(sp);
	}
    }
    g_threaded_group = TG_save;
    return ap;
}

/* Find the next thread (first if g_art > g_lastart).  If articles are selected,
** only choose from threads with selected articles.
*/
void visit_next_thread()
{
    SUBJECT* sp;
    ARTICLE* ap = g_artp;

    sp = (ap? ap->subj : nullptr);
    while ((sp = next_subj(sp, SF_VISIT)) != nullptr) {
	if ((ap = subj_art(sp)) != nullptr) {
	    g_art = article_num(ap);
	    g_artp = ap;
	    return;
	}
	g_reread = false;
    }
    g_artp = nullptr;
    g_art = g_lastart+1;
    g_forcelast = true;
}

/* Find previous thread (or last if g_artp == nullptr).  If articles are selected,
** only choose from threads with selected articles.
*/
void visit_prev_thread()
{
    SUBJECT* sp;
    ARTICLE* ap = g_artp;

    sp = (ap? ap->subj : nullptr);
    while ((sp = prev_subj(sp, SF_VISIT)) != nullptr) {
	if ((ap = subj_art(sp)) != nullptr) {
	    g_art = article_num(ap);
	    g_artp = ap;
	    return;
	}
	g_reread = false;
    }
    g_artp = nullptr;
    g_art = g_lastart+1;
    g_forcelast = true;
}

/* Find g_artp's parent or oldest ancestor.  Returns false if no such
** article.  Sets g_art and g_artp otherwise.
*/
bool find_parent(bool keep_going)
{
    ARTICLE* ap = g_artp;

    if (!ap->parent)
	return false;

    do {
	ap = ap->parent;
    } while (keep_going && ap->parent);

    g_artp = ap;
    g_art = article_num(ap);
    return true;
}

/* Find g_artp's first child or youngest decendent.  Returns false if no
** such article.  Sets g_art and g_artp otherwise.
*/
bool find_leaf(bool keep_going)
{
    ARTICLE* ap = g_artp;

    if (!ap->child1)
	return false;

    do {
	ap = ap->child1;
    } while (keep_going && ap->child1);

    g_artp = ap;
    g_art = article_num(ap);
    return true;
}

/* Find the next "sibling" of g_artp, including cousins that are the
** same distance down the thread as we are.  Returns false if no such
** article.  Sets g_art and g_artp otherwise.
*/
bool find_next_sib()
{
    ARTICLE* ta;
    ARTICLE* tb;
    int ascent;

    ascent = 0;
    ta = g_artp;
    for (;;) {
	while (ta->sibling) {
	    ta = ta->sibling;
	    if ((tb = first_sib(ta, ascent)) != nullptr) {
		g_artp = tb;
		g_art = article_num(tb);
		return true;
	    }
	}
	if (!(ta = ta->parent))
	    break;
	ascent++;
    }
    return false;
}

/* A recursive routine to find the first node at the proper depth.  This
** article is at depth 0.
*/
static ARTICLE *first_sib(ARTICLE *ta, int depth)
{
    ARTICLE* tb;

    if (!depth)
	return ta;

    for (;;) {
	if (ta->child1 && (tb = first_sib(ta->child1, depth-1)))
	    return tb;

	if (!ta->sibling)
	    return nullptr;

	ta = ta->sibling;
    }
}

/* Find the previous "sibling" of g_artp, including cousins that are
** the same distance down the thread as we are.  Returns false if no
** such article.  Sets g_art and g_artp otherwise.
*/
bool find_prev_sib()
{
    ARTICLE* ta;
    ARTICLE* tb;
    int ascent;

    ascent = 0;
    ta = g_artp;
    for (;;) {
	tb = ta;
	if (ta->parent)
	    ta = ta->parent->child1;
	else
	    ta = ta->subj->thread;
	if ((tb = last_sib(ta, ascent, tb)) != nullptr) {
	    g_artp = tb;
	    g_art = article_num(tb);
	    return true;
	}
	if (!(ta = ta->parent))
	    break;
	ascent++;
    }
    return false;
}

/* A recursive routine to find the last node at the proper depth.  This
** article is at depth 0.
*/
static ARTICLE *last_sib(ARTICLE *ta, int depth, ARTICLE *limit)
{
    ARTICLE* tb;
    ARTICLE* tc;

    if (ta == limit)
	return nullptr;

    if (ta->sibling) {
	tc = ta->sibling;
	if (tc != limit && (tb = last_sib(tc,depth,limit)))
	    return tb;
    }
    if (!depth)
	return ta;
    if (ta->child1)
	return last_sib(ta->child1, depth-1, limit);
    return nullptr;
}

/* Get each subject's article count; count total articles and selected
** articles (use g_sel_rereading to determine whether to count read or
** unread articles); deselect any subjects we find that are empty if
** CS_UNSELECT or CS_UNSEL_STORE is specified.  If mode is CS_RESELECT
** is specified, the selections from the last CS_UNSEL_STORE are
** reselected.
*/
void count_subjects(int cmode)
{
    int count, sel_count;
    ARTICLE* ap;
    SUBJECT* sp;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    time_t subjdate;

    obj_count = g_selected_count = g_selected_subj_cnt = 0;
    if (g_last_cached >= g_lastart)
	g_firstart = g_lastart+1;

    switch (cmode) {
    case CS_RETAIN:
	break;
    case CS_UNSEL_STORE:
	for (sp = g_first_subject; sp; sp = sp->next) {
	    if (sp->flags & SF_VISIT)
		sp->flags = (sp->flags & ~SF_VISIT) | SF_OLDVISIT;
	    else
		sp->flags &= ~SF_OLDVISIT;
	}
	break;
    case CS_RESELECT:
	for (sp = g_first_subject; sp; sp = sp->next) {
	    if (sp->flags & SF_OLDVISIT)
		sp->flags |= SF_VISIT;
	    else
		sp->flags &= ~SF_VISIT;
	}
	break;
    default:
	for (sp = g_first_subject; sp; sp = sp->next)
	    sp->flags &= ~SF_VISIT;
    }

    for (sp = g_first_subject; sp; sp = sp->next) {
	subjdate = 0;
	count = sel_count = 0;
	for (ap = sp->articles; ap; ap = ap->subj_next) {
	    if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == desired_flags) {
		count++;
		if (ap->flags & g_sel_mask)
		    sel_count++;
		if (!subjdate)
		    subjdate = ap->date;
		if (article_num(ap) < g_firstart)
		    g_firstart = article_num(ap);
	    }
	}
	sp->misc = count;
	if (subjdate)
	    sp->date = subjdate;
	else if (!sp->date && sp->articles)
	    sp->date = sp->articles->date;
	obj_count += count;
	if (cmode == CS_RESELECT) {
	    if (sp->flags & SF_VISIT) {
		sp->flags = (sp->flags & ~(SF_SEL|SF_DEL)) | g_sel_mask;
		g_selected_count += sel_count;
		g_selected_subj_cnt++;
	    } else
		sp->flags &= ~g_sel_mask;
	} else {
	    if (sel_count
	     && (cmode >= CS_UNSEL_STORE || (sp->flags & g_sel_mask))) {
		sp->flags = (sp->flags & ~(SF_SEL|SF_DEL)) | g_sel_mask;
		g_selected_count += sel_count;
		g_selected_subj_cnt++;
	    } else if (cmode >= CS_UNSELECT)
		sp->flags &= ~g_sel_mask;
	    else if (sp->flags & g_sel_mask) {
		sp->flags &= ~SF_DEL;
		g_selected_subj_cnt++;
	    }
	    if (count && (!g_selected_only || (sp->flags & g_sel_mask))) {
		sp->flags |= SF_VISIT;
	    }
	}
    }
    if (cmode != CS_RETAIN && cmode != CS_RESELECT
     && !obj_count && !g_selected_only) {
	for (sp = g_first_subject; sp; sp = sp->next)
	    sp->flags |= SF_VISIT;
    }
}

static int subjorder_subject(const SUBJECT **spp1, const SUBJECT**spp2)
{
    return strcasecmp((*spp1)->str+4, (*spp2)->str+4) * g_sel_direction;
}

static int subjorder_date(const SUBJECT **spp1, const SUBJECT**spp2)
{
    time_t eq = (*spp1)->date - (*spp2)->date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int subjorder_count(const SUBJECT **spp1, const SUBJECT**spp2)
{
    short eq = (*spp1)->misc - (*spp2)->misc;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : subjorder_date(spp1,spp2);
}

static int subjorder_lines(const SUBJECT **spp1, const SUBJECT**spp2)
{
    long eq, l1, l2;
    l1 = (*spp1)->articles? (*spp1)->articles->lines : 0;
    l2 = (*spp2)->articles? (*spp2)->articles->lines : 0;
    eq = l1 - l2;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : subjorder_date(spp1,spp2);
}

/* for now, highest eligible article score */
static int subject_score_high(const SUBJECT *sp)
{
    ARTICLE* ap;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int hiscore = 0;
    int hiscore_found = 0;
    int sc;

    /* find highest score of desired articles */
    for (ap = sp->articles; ap; ap = ap->subj_next) {
	if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == desired_flags) {
	    sc = sc_score_art(article_num(ap),false);
	    if ((!hiscore_found) || (sc>hiscore)) {
		hiscore_found = 1;
		hiscore = sc;
	    }
	}
    }
    return hiscore;
}

/* later make a subject-thread score routine */
static int subjorder_score(const SUBJECT** spp1, const SUBJECT** spp2)
{
    int sc1, sc2;
    sc1 = subject_score_high(*spp1);
    sc2 = subject_score_high(*spp2);

    if (sc1 != sc2)
	return (sc2 - sc1) * g_sel_direction;
    return subjorder_date(spp1, spp2);
}

static int threadorder_subject(const SUBJECT **spp1, const SUBJECT**spp2)
{
    ARTICLE* t1 = (*spp1)->thread;
    ARTICLE* t2 = (*spp2)->thread;
    if (t1 != t2 && t1 && t2)
	return strcasecmp(t1->subj->str+4, t2->subj->str+4) * g_sel_direction;
    return subjorder_date(spp1, spp2);
}

static int threadorder_date(const SUBJECT **spp1, const SUBJECT**spp2)
{
    ARTICLE* t1 = (*spp1)->thread;
    ARTICLE* t2 = (*spp2)->thread;
    if (t1 != t2 && t1 && t2) {
	SUBJECT* sp1;
	SUBJECT* sp2;
	long eq;
	if (!(sp1 = t1->subj)->misc)
	    for (sp1=sp1->thread_link; sp1 != t1->subj; sp1=sp1->thread_link)
		if (sp1->misc)
		    break;
	if (!(sp2 = t2->subj)->misc)
	    for (sp2=sp2->thread_link; sp2 != t2->subj; sp2=sp2->thread_link)
		if (sp2->misc)
		    break;
	if (!(eq = sp1->date - sp2->date))
	    return strcasecmp(sp1->str+4, sp2->str+4);
	return eq > 0? g_sel_direction : -g_sel_direction;
    }
    return subjorder_date(spp1, spp2);
}

static int threadorder_count(const SUBJECT **spp1, const SUBJECT**spp2)
{
    int size1 = (*spp1)->misc;
    int size2 = (*spp2)->misc;
    if ((*spp1)->thread != (*spp2)->thread) {
	SUBJECT* sp;
	for (sp = (*spp1)->thread_link; sp != *spp1; sp = sp->thread_link)
	    size1 += sp->misc;
	for (sp = (*spp2)->thread_link; sp != *spp2; sp = sp->thread_link)
	    size2 += sp->misc;
    }
    if (size1 != size2)
	return (size1 - size2) * g_sel_direction;
    return threadorder_date(spp1, spp2);
}

static int threadorder_lines(const SUBJECT **spp1, const SUBJECT**spp2)
{
    ARTICLE* t1 = (*spp1)->thread;
    ARTICLE* t2 = (*spp2)->thread;
    if (t1 != t2 && t1 && t2) {
	SUBJECT* sp1;
	SUBJECT* sp2;
	long eq, l1, l2;
	if (!(sp1 = t1->subj)->misc) {
	    for (sp1=sp1->thread_link; sp1 != t1->subj; sp1=sp1->thread_link) {
		if (sp1->misc)
		    break;
	    }
	}
	if (!(sp2 = t2->subj)->misc) {
	    for (sp2=sp2->thread_link; sp2 != t2->subj; sp2=sp2->thread_link) {
		if (sp2->misc)
		    break;
	    }
	}
	l1 = sp1->articles? sp1->articles->lines : 0;
	l2 = sp2->articles? sp2->articles->lines : 0;
	if ((eq = l1 - l2) != 0)
	    return eq > 0? g_sel_direction : -g_sel_direction;
	eq = sp1->date - sp2->date;
	return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
    }
    return subjorder_date(spp1, spp2);
}

static int thread_score_high(const SUBJECT *tp)
{ 
    const SUBJECT* sp;
    int hiscore = 0;
    int hiscore_found = 0;
    int sc;

    for (sp = tp->thread_link; ; sp = sp->thread_link) {
	sc = subject_score_high(sp);
	if ((!hiscore_found) || (sc>hiscore)) {
	    hiscore_found = 1;
	    hiscore = sc;
	}
	/* break *after* doing the last item */
	if (tp == sp)
	    break;
    }
    return hiscore;
}

static int threadorder_score(const SUBJECT** spp1, const SUBJECT** spp2)
{
    int sc1, sc2;

    sc1 = sc2 = 0;

    if ((*spp1)->thread != (*spp2)->thread) {
	sc1 = thread_score_high(*spp1);
	sc2 = thread_score_high(*spp2);
    }
    if (sc1 != sc2)
	return (sc2 - sc1) * g_sel_direction;
    return threadorder_date(spp1, spp2);
}

/* Sort the subjects according to the chosen order.
*/
void sort_subjects()
{
    SUBJECT* sp;
    int i;
    SUBJECT** lp;
    SUBJECT** subj_list;
    int (*sort_procedure)(const SUBJECT **spp1, const SUBJECT**spp2);

    /* If we don't have at least two subjects, we're done! */
    if (!g_first_subject || !g_first_subject->next)
	return;

    switch (g_sel_sort) {
      case SS_DATE:
      default:
	sort_procedure = (g_sel_mode == SM_THREAD?
			  threadorder_date : subjorder_date);
	break;
      case SS_STRING:
	sort_procedure = (g_sel_mode == SM_THREAD?
			  threadorder_subject : subjorder_subject);
	break;
      case SS_COUNT:
	sort_procedure = (g_sel_mode == SM_THREAD?
			  threadorder_count : subjorder_count);
	break;
      case SS_LINES:
	sort_procedure = (g_sel_mode == SM_THREAD?
			  threadorder_lines : subjorder_lines);
	break;
      /* if SCORE is undefined, use the default above */
      case SS_SCORE:
	sort_procedure = (g_sel_mode == SM_THREAD?
			  threadorder_score : subjorder_score);
	break;
    }

    subj_list = (SUBJECT**)safemalloc(subject_count * sizeof (SUBJECT*));
    for (lp = subj_list, sp = g_first_subject; sp; sp = sp->next)
	*lp++ = sp;
    assert(lp - subj_list == subject_count);

    qsort(subj_list, subject_count, sizeof (SUBJECT*), ((int(*)(void const *, void const *))sort_procedure));

    g_first_subject = sp = subj_list[0];
    sp->prev = nullptr;
    for (i = subject_count, lp = subj_list; --i; lp++) {
	lp[0]->next = lp[1];
	lp[1]->prev = lp[0];
	if (g_sel_mode == SM_THREAD) {
	    if (lp[0]->thread && lp[0]->thread == lp[1]->thread)
		lp[0]->thread_link = lp[1];
	    else {
		lp[0]->thread_link = sp;
		sp = lp[1];
	    }
	}
    }
    g_last_subject = lp[0];
    g_last_subject->next = nullptr;
    if (g_sel_mode == SM_THREAD)
	g_last_subject->thread_link = sp;
    free((char*)subj_list);
}

static int artorder_date(const ARTICLE **art1, const ARTICLE **art2)
{
    long eq = (*art1)->date - (*art2)->date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int artorder_subject(const ARTICLE **art1, const ARTICLE **art2)
{
    if ((*art1)->subj == (*art2)->subj)
	return artorder_date(art1, art2);
    return strcasecmp((*art1)->subj->str + 4, (*art2)->subj->str + 4)
	* g_sel_direction;
}

static int artorder_author(const ARTICLE **art1, const ARTICLE **art2)
{
    int eq = strcasecmp((*art1)->from, (*art2)->from);
    return eq? eq * g_sel_direction : artorder_date(art1, art2);
}

static int artorder_number(const ARTICLE **art1, const ARTICLE **art2)
{
    ART_NUM eq = article_num(*art1) - article_num(*art2);
    return eq > 0? g_sel_direction : -g_sel_direction;
}

static int artorder_groups(const ARTICLE **art1, const ARTICLE **art2)
{
    long eq;
#ifdef DEBUG
    assert((*art1)->subj != nullptr);
    assert((*art2)->subj != nullptr);
#endif
    if ((*art1)->subj == (*art2)->subj)
	return artorder_date(art1, art2);
    eq = (*art1)->subj->date - (*art2)->subj->date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int artorder_lines(const ARTICLE **art1, const ARTICLE **art2)
{
    long eq = (*art1)->lines - (*art2)->lines;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : artorder_date(art1,art2);
}

static int artorder_score(const ARTICLE **art1, const ARTICLE **art2)
{
    int eq = sc_score_art(article_num(*art2),false)
	   - sc_score_art(article_num(*art1),false);
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

/* Sort the articles according to the chosen order.
*/
void sort_articles()
{
    int (*sort_procedure)(const ARTICLE **art1, const ARTICLE **art2);

    build_artptrs();

    /* If we don't have at least two articles, we're done! */
    if (g_artptr_list_size < 2)
	return;

    switch (g_sel_sort) {
      case SS_DATE:
      default:
	sort_procedure = artorder_date;
	break;
      case SS_STRING:
	sort_procedure = artorder_subject;
	break;
      case SS_AUTHOR:
	sort_procedure = artorder_author;
	break;
      case SS_NATURAL:
	sort_procedure = artorder_number;
	break;
      case SS_GROUPS:
	sort_procedure = artorder_groups;
	break;
      case SS_LINES:
	sort_procedure = artorder_lines;
	break;
      case SS_SCORE:
	sort_procedure = artorder_score;
	break;
    }
    g_sel_page_app = 0;
    qsort(g_artptr_list, g_artptr_list_size, sizeof (ARTICLE*), ((int(*)(void const *, void const *))sort_procedure));
}

static void build_artptrs()
{
    ARTICLE** app;
    ARTICLE* ap;
    ART_NUM an;
    ART_NUM count = obj_count;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));

    if (!g_artptr_list || g_artptr_list_size != count) {
	g_artptr_list = (ARTICLE**)saferealloc((char*)g_artptr_list,
		(MEM_SIZE)count * sizeof (ARTICLE*));
	g_artptr_list_size = count;
    }
    app = g_artptr_list;
    for (an = article_first(g_absfirst); count; an = article_next(an)) {
	ap = article_ptr(an);
	if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == desired_flags) {
	    *app++ = ap;
	    count--;
	}
    }
}
