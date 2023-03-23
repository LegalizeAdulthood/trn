/* cache.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "common.h"
#include "list.h"
#include "intrp.h"
#include "search.h"
#include "ng.h"
#include "hash.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "term.h"
#include "final.h"
#include "head.h"
#include "mime.h"
#include "bits.h"
#include "kfile.h"
#include "rthread.h"
#include "rt-ov.h"
#include "rt-page.h"
#include "rt-process.h"
#include "rt-select.h"
#include "rt-util.h"
#include "score.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "utf.h"
#include "cache.h"

LIST *g_article_list{nullptr};    /* a list of ARTICLEs */
ARTICLE **g_artptr_list{nullptr}; /* the article-selector creates this */
ARTICLE **g_artptr{};             /* ditto -- used for article order */
ART_NUM g_artptr_list_size{0};
ART_NUM g_srchahead{0}; /* are we in subject scan mode? (if so, contains art # found or -1) */
ART_NUM g_first_cached{};
ART_NUM g_last_cached{};
bool g_cached_all_in_range{};
ARTICLE *g_sentinel_artp{};
SUBJECT *g_first_subject{nullptr};
SUBJECT *g_last_subject{nullptr};
bool g_untrim_cache{false};
#ifdef PENDING
static ART_NUM s_subj_to_get{};
static ART_NUM s_xref_to_get{};
static COMPEX s_srchcompex; /* compiled regex for searchahead */
#endif
static HASHTABLE *s_subj_hash = nullptr;
static HASHTABLE *s_shortsubj_hash = nullptr;

static void init_artnode(LIST *list, LISTNODE *node);
static bool clear_artitem(char *cp, int arg);

void cache_init()
{
#ifdef PENDING
    init_compex(&s_srchcompex);
#endif
}

static NGDATA* cached_ng = nullptr;
static time_t cached_time = 0;

void build_cache()
{
    if (cached_ng == g_ngptr && time((time_t*)nullptr) < cached_time + 6*60*60L) {
	ART_NUM an;
	cached_time = time((time_t*)nullptr);
	if (g_sel_mode == SM_ARTICLE)
	    set_selector(g_sel_mode, g_sel_artsort);
	else
	    set_selector(g_sel_threadmode, g_sel_threadsort);
	for (an = g_last_cached+1; an <= g_lastart; an++)
	    article_ptr(an)->flags |= AF_EXISTS;
	rc_to_bits();
	g_article_list->high = g_lastart;
	thread_grow();
	return;
    }

    close_cache();

    cached_ng = g_ngptr;
    cached_time = time((time_t*)nullptr);
    g_article_list = new_list(g_absfirst, g_lastart, sizeof (ARTICLE), 371,
			    LF_SPARSE, init_artnode);
    s_subj_hash = hashcreate(991, subject_cmp);	/*TODO: pick a better size */

    set_firstart(g_ngptr->rcline + g_ngptr->numoffset);
    g_first_cached = g_thread_always? g_absfirst : g_firstart;
    g_last_cached = g_first_cached-1;
    g_cached_all_in_range = false;
#ifdef PENDING
    s_subj_to_get = s_xref_to_get = g_firstart;
#endif

    /* Cache as much data in advance as possible, possibly threading
    ** articles as we go. */
    thread_open();
}

void close_cache()
{
    SUBJECT* sp;
    SUBJECT* next;

    nntp_artname(0, false);		/* clear the tmpfile cache */

    if (s_subj_hash) {
	hashdestroy(s_subj_hash);
	s_subj_hash = nullptr;
    }
    if (s_shortsubj_hash) {
	hashdestroy(s_shortsubj_hash);
	s_shortsubj_hash = nullptr;
    }
    /* Free all the subjects. */
    for (sp = g_first_subject; sp; sp = next) {
	next = sp->next;
	free(sp->str);
	free((char*)sp);
    }
    g_first_subject = g_last_subject = nullptr;
    g_subject_count = 0;			/* just to be sure */
    g_parsed_art = 0;

    if (g_artptr_list) {
	free((char*)g_artptr_list);
	g_artptr_list = nullptr;
    }
    g_artptr = nullptr;
    thread_close();

    if (g_article_list) {
	walk_list(g_article_list, clear_artitem, 0);
	delete_list(g_article_list);
	g_article_list = nullptr;
    }
    cached_ng = nullptr;
}

/* Initialize the memory for an entire node's worth of article's */
static void init_artnode(LIST *list, LISTNODE *node)
{
    ART_NUM i;
    ARTICLE* ap;
    memset(node->data,0,list->items_per_node * list->item_size);
    for (i = node->low, ap = (ARTICLE*)node->data; i <= node->high; i++, ap++)
	ap->num = i;
}

static bool clear_artitem(char *cp, int arg)
{
    clear_article((ARTICLE*)cp);
    return false;
}

/* The article has all it's data in place, so add it to the list of articles
** with the same subject.
*/
void cache_article(ARTICLE *ap)
{
    ARTICLE* next;
    ARTICLE* ap2;

    if (!(next = ap->subj->articles) || ap->date < next->date)
	ap->subj->articles = ap;
    else {
	while ((next = (ap2 = next)->subj_next) && next->date <= ap->date)
	    ;
	ap2->subj_next = ap;
    }
    ap->subj_next = next;
    ap->flags |= AF_CACHED;

    if (!!(ap->flags & AF_UNREAD) ^ g_sel_rereading) {
	if (ap->subj->flags & g_sel_mask)
	    select_article(ap, 0);
	else {
	    if (ap->subj->flags & SF_WASSELECTED) {
#if 0
		if (g_selected_only)
		    ap->flags |= g_sel_mask;
		else
#endif
		    select_article(ap, 0);
	    }
	    ap->subj->flags |= SF_VISIT;
	}
    }

    if (g_join_subject_len != 0)
	check_for_near_subj(ap);
}

void check_for_near_subj(ARTICLE *ap)
{
    SUBJECT* sp;
    if (!s_shortsubj_hash) {
	s_shortsubj_hash = hashcreate(401, subject_cmp);	/*TODO: pick a better size */
	sp = g_first_subject;
    }
    else {
	sp = ap->subj;
	if (sp->next)
	    sp = nullptr;
    }
    while (sp) {
	if ((int)strlen(sp->str+4) >= g_join_subject_len && sp->thread) {
	    SUBJECT* sp2;
	    HASHDATUM data;
	    data = hashfetch(s_shortsubj_hash, sp->str+4, g_join_subject_len);
	    if (!(sp2 = (SUBJECT*)data.dat_ptr)) {
		data.dat_ptr = (char*)sp;
		hashstorelast(data);
	    }
	    else if (sp->thread != sp2->thread) {
		merge_threads(sp2, sp);
	    }
	}
	sp = sp->next;
    }
}

void change_join_subject_len(int len)
{
    if (g_join_subject_len != len) {
	if (s_shortsubj_hash) {
	    hashdestroy(s_shortsubj_hash);
	    s_shortsubj_hash = nullptr;
	}
	g_join_subject_len = len;
	if (len && g_first_subject && g_first_subject->articles)
	    check_for_near_subj(g_first_subject->articles);
    }
}

void check_poster(ARTICLE *ap)
{
    if (g_auto_select_postings && (ap->flags & AF_EXISTS) && ap->from) {
	if (ap->flags & AF_FROMTRUNCED) {
	    strcpy(g_cmd_buf,g_real_name);
	    if (!strcmp(ap->from,compress_name(g_cmd_buf,16))) {
		g_untrim_cache = true;
		fetchfrom(article_num(ap),false);
		g_untrim_cache = false;
	    }
	}
	if (!(ap->flags & AF_FROMTRUNCED)) {
	    char* s = g_cmd_buf;
	    char* u;
	    char* h;
	    strcpy(s,ap->from);
	    if ((h=strchr(s,'<')) != nullptr) { /* grab the good part */
		s = h+1;
		if ((h=strchr(s,'>')) != nullptr)
		    *h = '\0';
	    } else if ((h=strchr(s,'(')) != nullptr) {
		while (h-- != s && *h == ' ')
		    ;
		h[1] = '\0';		/* or strip the comment */
	    }
	    if ((h = strchr(s,'%')) != nullptr || (h = strchr(s,'@')) != nullptr) {
		*h++ = '\0';
		u = s;
	    } else if ((u = strrchr(s,'!')) != nullptr) {
		*u++ = '\0';
		h = s;
	    } else
		h = u = s;
	    if (!strcmp(u,g_login_name)) {
		if (in_string(h,g_hostname, false)) {
		    switch (g_auto_select_postings) {
		      case '.':
			select_subthread(ap,AUTO_SEL_FOL);
			break;
		      case '+':
			select_arts_thread(ap,AUTO_SEL_THD);
			break;
		      case 'p':
			if (ap->parent)
			    select_subthread(ap->parent,AUTO_SEL_FOL);
			else
			    select_subthread(ap,AUTO_SEL_FOL);
			break;
		    }
		} else {
#ifdef REPLYTO_POSTER_CHECKING
		    char* reply_buf = fetchlines(article_num(ap),REPLY_LINE);
		    if (instr(reply_buf,g_login_name,true))
			select_subthread(ap,AUTO_SEL_FOL);
		    free(reply_buf);
#endif
		}
	    }
	}
    }
}

/* The article turned out to be a duplicate, so remove it from the cached
** list and possibly destroy the subject (should only happen if the data
** was corrupt and the duplicate id got a different subject).
*/
void uncache_article(ARTICLE *ap, bool remove_empties)
{
    ARTICLE* next;

    if (ap->subj) {
	if (ALLBITS(ap->flags, AF_CACHED | AF_EXISTS)) {
	    if ((next = ap->subj->articles) == ap)
		ap->subj->articles = ap->subj_next;
	    else {
		ARTICLE* ap2;
		while (next) {
		    if ((ap2 = next->subj_next) == ap) {
			next->subj_next = ap->subj_next;
			break;
		    }
		    next = ap2;
		}
	    }
	}
	if (remove_empties && !ap->subj->articles) {
	    SUBJECT* sp = ap->subj;
	    if (sp == g_first_subject)
		g_first_subject = sp->next;
	    else
		sp->prev->next = sp->next;
	    if (sp == g_last_subject)
		g_last_subject = sp->prev;
	    else
		sp->next->prev = sp->prev;
	    hashdelete(s_subj_hash, sp->str+4, strlen(sp->str+4));
	    free((char*)sp);
	    ap->subj = nullptr;
	    g_subject_count--;
	}
    }
    ap->flags2 |= AF2_BOGUS;
    onemissing(ap);
}

/* get the header line from an article's cache or parse the article trying */

char *fetchcache(ART_NUM artnum, int which_line, bool fill_cache)
{
    char* s;
    ARTICLE* ap;
    bool cached = (g_htype[which_line].flags & HT_CACHED);

    /* article_find() returns a nullptr if the artnum value is invalid */
    if (!(ap = article_find(artnum)) || !(ap->flags & AF_EXISTS))
	return "";
    if (cached && (s=get_cached_line(ap,which_line,g_untrim_cache)) != nullptr)
	return s;
    if (!fill_cache)
	return nullptr;
    if (!parseheader(artnum))
	return "";
    if (cached)
	return get_cached_line(ap,which_line,g_untrim_cache);
    return nullptr;
}

/* Return a pointer to a cached header line for the indicated article.
** Truncated headers (e.g. from a .thread file) are optionally ignored.
*/
char *get_cached_line(ARTICLE *ap, int which_line, bool no_truncs)
{
    char* s;

    switch (which_line) {
      case SUBJ_LINE:
	if (!ap->subj || (no_truncs && (ap->subj->flags & SF_SUBJTRUNCED)))
	    s = nullptr;
	else
	    s = ap->subj->str + ((ap->flags & AF_HAS_RE) ? 0 : 4);
	break;
      case FROM_LINE:
	if (no_truncs && (ap->flags & AF_FROMTRUNCED))
	    s = nullptr;
	else
	    s = ap->from;
	break;
      case XREF_LINE:
	s = ap->xrefs;
	break;
      case MSGID_LINE:
	s = ap->msgid;
	break;
      case LINES_LINE: {
	static char linesbuf[32];
	sprintf(linesbuf, "%ld", ap->lines);
	s = linesbuf;
	break;
      }
      case BYTES_LINE: {
	static char bytesbuf[32];
	sprintf(bytesbuf, "%ld", ap->bytes);
	s = bytesbuf;
	break;
      }
      default:
	s = nullptr;
	break;
    }
    return s;
}

/* subj not yet allocated, so we can tweak it first */
void set_subj_line(ARTICLE *ap, char *subj, int size)
{
    HASHDATUM data;
    SUBJECT* sp;
    char* newsubj;
    char* subj_start;
    short def_flags = 0;

    if (subject_has_Re(subj, &subj_start))
	ap->flags |= AF_HAS_RE;
    if ((size -= subj_start - subj) < 0)
	size = 0;

    newsubj = safemalloc(size + 4 + 1);
    strcpy(newsubj, "Re: ");
    size = decode_header(newsubj + 4, subj_start, size);

    /* Do the Re:-stripping over again, just in case it was encoded. */
    if (subject_has_Re(newsubj + 4, &subj_start))
	ap->flags |= AF_HAS_RE;
    if (subj_start != newsubj + 4) {
	safecpy(newsubj + 4, subj_start, size);
	if ((size -= subj_start - newsubj - 4) < 0)
	    size = 0;
    }
    if (ap->subj && !strncmp(ap->subj->str+4, newsubj+4, size)) {
	free(newsubj);
	return;
    }

    if (ap->subj) {
	/* This only happens when we freshen truncated subjects */
	hashdelete(s_subj_hash, ap->subj->str+4, strlen(ap->subj->str+4));
	free(ap->subj->str);
	ap->subj->str = newsubj;
	ap->subj->flags |= def_flags;
	data.dat_ptr = (char*)ap->subj;
	hashstore(s_subj_hash, newsubj + 4, size, data);
    } else {
	data = hashfetch(s_subj_hash, newsubj + 4, size);
	if (!(sp = (SUBJECT*)data.dat_ptr)) {
	    sp = (SUBJECT*)safemalloc(sizeof (SUBJECT));
	    memset((char*)sp,0,sizeof (SUBJECT));
	    g_subject_count++;
	    if ((sp->prev = g_last_subject) != nullptr)
		sp->prev->next = sp;
	    else
		g_first_subject = sp;
	    g_last_subject = sp;
	    sp->str = newsubj;
	    sp->thread_link = sp;
	    sp->flags = def_flags;

	    data.dat_ptr = (char*)sp;
	    hashstorelast(data);
	} else
	    free(newsubj);
	ap->subj = sp;
    }
}

int decode_header(char *to, char *from, int size)
{
    int i;
    char *s = to; /* save for pass 2 */
    bool pass2needed = false;

    /* Pass 1 to decode coded bytes (which might be character fragments - so 1 pass is wrong) */
    for (i = size; *from && i--; ) {
	if (*from == '=' && from[1] == '?') {
	    char* q = strchr(from+2,'?');
	    char ch = (q && q[2] == '?')? q[1] : 0;
	    char* e;

	    if (ch == 'q' || ch == 'Q' || ch == 'b' || ch == 'B') {
		const char* old_ics = input_charset_name();
		const char* old_ocs = output_charset_name();
#ifdef USE_UTF_HACK
		*q = '\0';
		utf_init(from+2, "utf-8"); /*FIXME*/
		*q = '?';
#endif
		e = q+2;
		do {
		    e = strchr(e+1, '?');
		} while (e && e[1] != '=');
		if (e) {
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
			len = qp_decodestring(to, q, true);
		    else
			len = b64_decodestring(to, q);
#ifdef USE_UTF_HACK
		    d = create_utf8_copy(to);
		    if (d) {
			for (len = 0; d[len]; ) {
			    to[len] = d[len];
			    len++;
			}
			free(d);
		    }
#endif
		    *e = '?';
		    to += len;
		    size += len;
		    /* If the next character is whitespace we should eat it now */
		    while (*from == ' ' || *from == '\t')
			from++;
		}
		else
		    *to++ = *from++;
#ifdef USE_UTF_HACK
		utf_init(old_ics, old_ocs);
#endif
	    }
	    else
		*to++ = *from++;
	} else if (*from != '\n')
	    *to++ = *from++;
	else
	    from++, size--;
	pass2needed = true;
    }
    while (size > 1 && to[-1] == ' ')
	to--, size--;
    *to = '\0';

    /* Pass 2 to clear out "control" characters */
    if (pass2needed)
	dectrl(s);
    return size;
}

void dectrl(char *str)
{
    if (str == NULL)
	return;

    for ( ; *str;) {
	int w = byte_length_at(str);
	if (at_grey_space(str)) {
	    int i;
	    for (i = 0; i < w; i += 1) {
		str[i] = ' ';
	    }
	}
	str += w;
    }
}

/* s already allocated, ready to save */
void set_cached_line(ARTICLE *ap, int which_line, char *s)
{
    char* cp;
    /* SUBJ_LINE is handled specially above */
    switch (which_line) {
      case FROM_LINE:
	ap->flags &= ~AF_FROMTRUNCED;
	if (ap->from)
	    free(ap->from);
	decode_header(s, s, strlen(s));
	ap->from = s;
	break;
      case XREF_LINE:
	if (ap->xrefs && ap->xrefs != "")
	    free(ap->xrefs);
	/* Exclude an xref for just this group or "(none)". */
	cp = strchr(s, ':');
	if (!cp || !strchr(cp+1, ':')) {
	    free(s);
	    s = "";
	}
	ap->xrefs = s;
	break;
      case MSGID_LINE:
	if (ap->msgid)
	    free(ap->msgid);
	ap->msgid = s;
	break;
      case LINES_LINE:
	ap->lines = atol(s);
	break;
      case BYTES_LINE:
	ap->bytes = atol(s);
	break;
    }
}

int subject_cmp(const char *key, int keylen, HASHDATUM data)
{
    /* We already know that the lengths are equal, just compare the strings */
    return memcmp(key, ((SUBJECT*)data.dat_ptr)->str+4, keylen);
}

/* see what we can do while they are reading */

#ifdef PENDING
void look_ahead()
{
#ifdef ARTSEARCH
    char* h;
    char* s;

#ifdef DEBUG
    if (debug && g_srchahead) {
	printf("(%ld)",(long)g_srchahead);
	fflush(stdout);
    }
#endif
#endif

    if (g_threaded_group) {
	g_artp = g_curr_artp;
	inc_art(g_selected_only,false);
	if (g_artp)
	    parseheader(g_art);
    }
    else
#ifdef ARTSEARCH
    if (g_srchahead && g_srchahead < g_art) {	/* in ^N mode? */
	char* pattern;

	pattern = g_buf+1;
	strcpy(pattern,": *");
	h = pattern + strlen(pattern);
	interp(h,(sizeof g_buf) - (h-g_buf),"%\\s");
	{			/* compensate for notesfiles */
	    int i;
	    for (i = 24; *h && i--; h++)
		if (*h == '\\')
		    h++;
	    *h = '\0';
	}
#ifdef DEBUG
	if (debug & DEB_SEARCH_AHEAD) {
	    fputs("(hit CR)",stdout);
	    fflush(stdout);
	    fgets(g_buf+128, sizeof g_buf-128, stdin);
	    printf("\npattern = %s\n",pattern);
	    termdown(2);
	}
#endif
	if ((s = compile(&s_srchcompex,pattern,true,true)) != nullptr) {
				    /* compile regular expression */
	    printf("\n%s\n",s) FLUSH;
	    termdown(2);
	    g_srchahead = 0;
	}
	if (g_srchahead) {
	    g_srchahead = g_art;
	    for (;;) {
		g_srchahead++;	/* go forward one article */
		if (g_srchahead > g_lastart) { /* out of articles? */
#ifdef DEBUG
		    if (debug)
			fputs("(not found)",stdout);
#endif
		    break;
		}
		if (!was_read(g_srchahead) &&
		    wanted(&s_srchcompex,g_srchahead,0)) {
				    /* does the shoe fit? */
#ifdef DEBUG
		    if (debug)
			printf("(%ld)",(long)g_srchahead);
#endif
		    parseheader(g_srchahead);
		    break;
		}
		if (input_pending())
		    break;
	    }
	    fflush(stdout);
	}
    }
    else
#endif /* ARTSEARCH */
    {
	if (article_next(g_art) <= g_lastart)	/* how about a pre-fetch? */
	    parseheader(article_next(g_art));	/* look for the next article */
    }
}
#endif /* PENDING */

/* see what else we can do while they are reading */

void cache_until_key()
{
    if (!g_in_ng)
	return;
#ifdef PENDING
    if (input_pending())
	return;

    if ((g_datasrc->flags & DF_REMOTE) && nntp_finishbody(FB_BACKGROUND))
	return;

    g_untrim_cache = true;
    g_sentinel_artp = g_curr_artp;

    /* Prioritize our caching based on what mode we're in */
    if (g_general_mode == 's') {
	if (cache_subjects()) {
	    if (cache_xrefs()) {
		if (chase_xrefs(true)) {
		    if (g_threaded_group)
			cache_all_arts();
		    else
			cache_unread_arts();
		}
	    }
	}
    } else {
	if (!g_threaded_group || cache_all_arts()) {
	    if (cache_subjects()) {
		if (cache_unread_arts()) {
		    if (cache_xrefs())
			chase_xrefs(true);
		}
	    }
	}
    }

    if (!input_pending() && g_sc_initialized)
	sc_lookahead(true,true);

    setspin(SPIN_OFF);
    g_untrim_cache = false;
#endif
    check_datasrcs();
}

#ifdef PENDING
bool cache_subjects()
{
    ART_NUM an;

    if (s_subj_to_get > g_lastart)
	return true;
    setspin(SPIN_BACKGROUND);
    for (an=article_first(s_subj_to_get); an <= g_lastart; an=article_next(an)) {
	if (input_pending())
	    break;
	
	if (article_unread(an))
	    fetchsubj(an,false);
    }
    s_subj_to_get = an;
    return s_subj_to_get > g_lastart;
}

bool cache_xrefs()
{
    ART_NUM an;

    if (g_olden_days || (g_datasrc->flags & DF_NOXREFS) || s_xref_to_get > g_lastart)
	return true;
    setspin(SPIN_BACKGROUND);
    for (an=article_first(s_xref_to_get); an <= g_lastart; an=article_next(an)) {
	if (input_pending())
	    break;
	if (article_unread(an))
	    fetchxref(an,false);
    }
    s_xref_to_get = an;
    return s_xref_to_get > g_lastart;
}

bool cache_all_arts()
{
    int old_last_cached = g_last_cached;
    if (!g_cached_all_in_range)
	g_last_cached = g_first_cached-1;
    if (g_last_cached >= g_lastart && g_first_cached <= g_absfirst)
	return true;

    /* turn it on as late as possible to avoid fseek()ing openart */
    setspin(SPIN_BACKGROUND);
    if (g_last_cached < g_lastart) {
	if (g_datasrc->ov_opened)
	    ov_data(g_last_cached+1, g_lastart, true);
	if (!art_data(g_last_cached+1, g_lastart, true, true)) {
	    g_last_cached = old_last_cached;
	    return false;
	}
	g_cached_all_in_range = true;
    }
    if (g_first_cached > g_absfirst) {
	if (g_datasrc->ov_opened)
	    ov_data(g_absfirst, g_first_cached-1, true);
	else
	    art_data(g_absfirst, g_first_cached-1, true, true);
	/* If we got interrupted, make a quick exit */
	if (g_first_cached > g_absfirst) {
	    g_last_cached = old_last_cached;
	    return false;
	}
    }
    /* We're all done threading the group, so if the current article is
    ** still in doubt, tell them it's missing. */
    if (g_curr_artp && !(g_curr_artp->flags & AF_CACHED) && !input_pending())
	pushchar('\f' | 0200);
    /* A completely empty group needs a count & a sort */
    if (g_general_mode != 's' && !g_obj_count && !g_selected_only)
	thread_grow();
    return true;
}

bool cache_unread_arts()
{
    if (g_last_cached >= g_lastart)
	return true;
    setspin(SPIN_BACKGROUND);
    return art_data(g_last_cached+1, g_lastart, true, false);
}
#endif

bool art_data(ART_NUM first, ART_NUM last, bool cheating, bool all_articles)
{
    ART_NUM i;
    ART_NUM expected_i = first;

    int cachemask = (g_threaded_group ? AF_THREADED : AF_CACHED)
		  + (all_articles? 0 : AF_UNREAD);
    int cachemask2 = (all_articles? 0 : AF_UNREAD);

    if (cheating)
	setspin(SPIN_BACKGROUND);
    else {
	int lots2do = ((g_datasrc->flags & DF_REMOTE)? g_net_speed : 20) * 25;
	setspin(g_spin_estimate > lots2do? SPIN_BARGRAPH : SPIN_FOREGROUND);
    }
    /*TRN_ASSERT(first >= g_absfirst && last <= g_lastart);*/
    for (i = article_first(first); i <= last; i = article_next(i)) {
	if ((article_ptr(i)->flags & cachemask) ^ cachemask2)
	    continue;

	g_spin_todo -= i - expected_i;
	expected_i = i + 1;

	/* This parses the header which will cache/thread the article */
	(void) parseheader(i);

	if (g_int_count) {
	    g_int_count = 0;
	    break;
	}
	if (cheating) {
	    if (input_pending())
		break;
	    /* If the current article is no longer a '?', let them know. */
	    if (g_curr_artp != g_sentinel_artp) {
		pushchar('\f' | 0200);
		break;
	    }
	}
    }
    setspin(SPIN_POP);
    if (i > last)
	i = last;
    if (i > g_last_cached)
	g_last_cached = i;
    if (i == last) {
	if (first < g_first_cached)
	    g_first_cached = first;
	return true;
    }
    return false;
}

bool cache_range(ART_NUM first, ART_NUM last)
{
    bool success = true;
    bool all_arts = (g_sel_rereading || g_thread_always);
    ART_NUM count = 0;

    if (g_sel_rereading && !g_cached_all_in_range) {
	g_first_cached = first;
	g_last_cached = first-1;
    }
    if (first < g_first_cached)
	count = g_first_cached-first;
    if (last > g_last_cached)
	count += last-g_last_cached;
    if (!count)
	return true;
    g_spin_todo = count;

    if (g_first_cached > g_last_cached) {
	if (g_sel_rereading) {
	    if (g_first_subject)
		count -= g_ngptr->toread;
	} else if (first == g_firstart && last == g_lastart && !all_arts)
	    count = g_ngptr->toread;
    }
    g_spin_estimate = count;

    printf("\n%sing %ld article%s.", g_threaded_group? "Thread" : "Cach",
	   (long)count, PLURAL(count));
    termdown(1);

    setspin(SPIN_FOREGROUND);

    if (first < g_first_cached) {
	if (g_datasrc->ov_opened) {
	    ov_data(g_absfirst,g_first_cached-1,false);
	    success = (g_first_cached == g_absfirst);
	} else {
	    success = art_data(first, g_first_cached-1, false, all_arts);
	    g_cached_all_in_range = (all_arts && success);
	}
    }
    if (success && g_last_cached < last) {
	if (g_datasrc->ov_opened)
	    ov_data(g_last_cached+1, last, false);
	success = art_data(g_last_cached+1, last, false, all_arts);
	g_cached_all_in_range = (all_arts && success);
    }
    setspin(SPIN_POP);
    return success;
}

void clear_article(ARTICLE *ap)
{
    if (ap->from)
	free(ap->from);
    if (ap->msgid)
	free(ap->msgid);
    if (ap->xrefs && ap->xrefs != "")
	free(ap->xrefs);
}
