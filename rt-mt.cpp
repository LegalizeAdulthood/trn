/* rt-mt.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "rt-mt.h"

#include "bits.h"
#include "cache.h"
#include "datasrc.h"
#include "hash.h"
#include "kfile.h"
#include "ngdata.h"
#include "nntp.h"
#include "nntpclient.h"
#include "rt-process.h"
#include "rthread.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

#include <cstdint>

enum
{
    DB_VERSION = 2
};

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using LONG = std::uint32_t;

enum packedarticle_flags
{
    PA_NONE = 0x0000,
    ROOT_ARTICLE = 0x0001, /* article flag definitions */
    HAS_XREFS = 0x0004     /* article has an xref line */
};

struct PACKED_ROOT
{
    LONG root_num;
    WORD articles;
    WORD article_cnt;
    WORD subject_cnt;
    WORD pad_hack;
};

struct PACKED_ARTICLE
{
    LONG num;
    LONG date;
    WORD subject, author;
    packedarticle_flags flags;
    WORD child_cnt;
    WORD parent;
    WORD padding;
    WORD sibling;
    WORD root;
};

struct TOTAL
{
    LONG first, last;
    LONG string1;
    LONG string2;
    WORD root;
    WORD article;
    WORD subject;
    WORD author;
    WORD domain;
    WORD pad_hack;
};

struct BMAP
{
    BYTE l[sizeof (LONG)];
    BYTE w[sizeof (WORD)];
    BYTE version;
    BYTE pad_hack;
};

static FILE *s_fp{};
static bool s_word_same{};
static bool s_long_same{};
static BMAP s_my_bmap{};
static BMAP s_mt_bmap{};
static char *s_strings{};
static WORD *s_author_cnts{};
static WORD *s_ids{};
static ARTICLE **s_article_array{};
static SUBJECT **s_subject_array{};
static char **s_author_array{};
static TOTAL s_total{};
static PACKED_ROOT s_p_root{};
static PACKED_ARTICLE s_p_article{};

static char *mt_name(const char *group);
static int read_authors();
static int read_subjects();
static int read_roots();
static SUBJECT *the_subject(int num);
static char *the_author(int num);
static ARTICLE *the_article(int relative_offset, int num);
static int read_articles();
static int read_ids();
static void tweak_data();
static int read_item(char **dest, MEM_SIZE len);
static void mybytemap(BMAP *map);
static void wp_bmap(WORD *buf, int len);
static void lp_bmap(LONG *buf, int len);

/* Initialize our thread code by determining the byte-order of the thread
** files and our own current byte-order.  If they differ, set flags to let
** the read code know what we'll need to translate.
*/
bool mt_init()
{
    long size;
    bool success = true;

    g_datasrc->flags &= ~DF_TRY_THREAD;

    s_word_same = true;
    s_long_same = true;
#ifdef SUPPORT_XTHREAD
    if (!g_datasrc->thread_dir) {
	if (nntp_command("XTHREAD DBINIT") <= 0)
	    return false;
	size = nntp_readcheck();
	if (size >= 0)
	    size = nntp_read((char*)&s_mt_bmap, (long)sizeof (BMAP));
    }
    else
#endif
    {
        s_fp = fopen(filexp(DBINIT), FOPEN_RB);
        if (s_fp != nullptr)
	    size = fread((char*)&s_mt_bmap, 1, sizeof (BMAP), s_fp);
	else
	    size = 0;
    }
    if (size >= (long)(sizeof (BMAP)) - 1) {
	if (s_mt_bmap.version != DB_VERSION) {
	    printf("\nMthreads database is the wrong version -- ignoring it.\n")
		FLUSH;
	    return false;
	}
	mybytemap(&s_my_bmap);
	for (int i = 0; i < sizeof (LONG); i++) {
	    if (i < sizeof (WORD)) {
		if (s_my_bmap.w[i] != s_mt_bmap.w[i])
		    s_word_same = false;
	    }
	    if (s_my_bmap.l[i] != s_mt_bmap.l[i])
		s_long_same = false;
	}
    } else
	success = false;
#ifdef SUPPORT_XTHREAD
    if (!g_datasrc->thread_dir) {
	while (nntp_read(g_ser_line, (long)sizeof g_ser_line))
	    ;		/* trash any extraneous bytes */
    }
    else
#endif
    if (s_fp != nullptr)
	fclose(s_fp);

    if (success)
	g_datasrc->flags |= DF_TRY_THREAD;
    return success;
}

/* Open and process the data in the group's thread file.  Returns true unless
** we discovered a bogus thread file, destroyed the cache, and re-built it.
*/
int mt_data()
{
    int ret = 1;
#ifdef SUPPORT_XTHREAD		/* use remote thread file? */

    if (!g_datasrc->thread_dir) {
	if (nntp_command("XTHREAD THREAD") <= 0)
	    return 0;
	long size = nntp_readcheck();
	if (size < 0)
	    return 0;

	if (g_verbose)
        {
            printf("\nGetting thread file.");
	    fflush(stdout);
        }
	if (nntp_read((char*)&s_total, (long)sizeof (TOTAL)) < sizeof (TOTAL))
	    goto exit;
    }
    else
#endif
    {
        s_fp = fopen(mt_name(g_ngname), FOPEN_RB);
        if (s_fp == nullptr)
	    return 0;
	if (g_verbose)
        {
            printf("\nReading thread file.");
	    fflush(stdout);
        }

	if (fread((char*)&s_total, 1, sizeof (TOTAL), s_fp) < sizeof (TOTAL))
	    goto exit;
    }

    lp_bmap(&s_total.first, 4);
    wp_bmap(&s_total.root, 5);
    if (!s_total.root) {
	tweak_data();
	goto exit;
    }
    if (!g_datasrc->thread_dir && s_total.last > g_lastart)
	s_total.last = g_lastart;

    if (read_authors()
     && read_subjects()
     && read_roots()
     && read_articles()
     && read_ids())
    {
	tweak_data();
	g_first_cached = g_absfirst;
	g_last_cached = (s_total.last < g_absfirst ? g_absfirst-1: s_total.last);
	g_cached_all_in_range = true;
	goto exit;
    }
    /* Something failed.  Safefree takes care of checking if some items
    ** were already freed.  Any partially-allocated structures were freed
    ** before we got here.  All other structures are cleaned up now.
    */
    close_cache();
    safefree0(s_strings);
    safefree0(s_article_array);
    safefree0(s_subject_array);
    safefree0(s_author_array);
    safefree0(s_ids);
    g_datasrc->flags &= ~DF_TRY_THREAD;
    build_cache();
    g_datasrc->flags |= DF_TRY_THREAD;
    ret = -1;

exit:
#ifdef SUPPORT_XTHREAD
    if (!g_datasrc->thread_dir) {
	while (nntp_read(g_ser_line, (long)sizeof g_ser_line))
	    ;		/* trash any extraneous bytes */
    }
    else
#endif
	fclose(s_fp);

    return ret;
}

/* Change a newsgroup name into the name of the thread data file.  We
** subsitute any '.'s in the group name into '/'s, prepend the path,
** and append the '/.thread' or '.th' on to the end.
*/
static char *mt_name(const char *group)
{
    sprintf(g_buf, "%s/%s", g_datasrc->thread_dir, group);
    return g_buf;
}

static char *s_subject_strings{};
static char *s_string_end{};

/* The author information is an array of use-counts, followed by all the
** null-terminated strings crammed together.  The subject strings are read
** in at the same time, since they are appended to the end of the author
** strings.
*/
static int read_authors()
{
    int   count;

    if (!read_item((char**)&s_author_cnts, (MEM_SIZE)s_total.author*sizeof (WORD)))
	return 0;
    safefree0(s_author_cnts);   /* we don't need these */

    if (!read_item(&s_strings, (MEM_SIZE)s_total.string1))
	return 0;

    char *string_ptr = s_strings;
    s_string_end = string_ptr + s_total.string1;
    if (s_string_end[-1] != '\0') {
	/*error("first string table is invalid.\n");*/
	return 0;
    }

    /* We'll use this array to point each article at its proper author
    ** (the packed values were saved as indexes).
    */
    s_author_array = (char**)safemalloc(s_total.author * sizeof (char*));
    char **author_ptr = s_author_array;

    for (count = s_total.author; count; count--) {
	if (string_ptr >= s_string_end)
	    break;
	*author_ptr++ = string_ptr;
	string_ptr += strlen(string_ptr) + 1;
    }
    s_subject_strings = string_ptr;

    if (count) {
	/*error("author unpacking failed.\n");*/
	return 0;
    }
    return 1;
}

/* The subject values consist of the crammed-together null-terminated strings
** (already read in above) and the use-count array.  They were saved in the
** order that the roots require while being unpacked.
*/
static int read_subjects()
{
    int  count;
    WORD*subject_cnts;

    if (!read_item((char**)&subject_cnts,
		   (MEM_SIZE)s_total.subject * sizeof (WORD))) {
	/* (Error already logged.) */
	return 0;
    }
    free((char*)subject_cnts);		/* we don't need these */

    /* Use this array when unpacking the article's subject offset. */
    s_subject_array = (SUBJECT**)safemalloc(s_total.subject * sizeof (SUBJECT*));
    SUBJECT **subj_ptr = s_subject_array;

    char *string_ptr = s_subject_strings; /* s_string_end is already set */

    for (count = s_total.subject; count; count--) {
        ARTICLE arty;
	if (string_ptr >= s_string_end)
	    break;
	int len = strlen(string_ptr);
	arty.subj = 0;
	set_subj_line(&arty, string_ptr, len);
	if (len == 72)
	    arty.subj->flags |= SF_SUBJTRUNCED;
	arty.subj->thread_link = nullptr;
	string_ptr += len + 1;
	*subj_ptr++ = arty.subj;
    }
    if (count || string_ptr != s_string_end) {
	/*error("subject data is invalid.\n");*/
	return 0;
    }
    return 1;
}

/* Read in the packed root structures to set each subject's thread article
** offset.  This gets turned into a real pointer later.
*/
static int read_roots()
{
    int     i;
    SUBJECT*sp;
    int     ret;

    SUBJECT **subj_ptr = s_subject_array;

    for (int count = s_total.root; count--; ) {
#ifdef SUPPORT_XTHREAD
	if (!g_datasrc->thread_dir)
	    ret = nntp_read((char*)&s_p_root, (long)sizeof (PACKED_ROOT));
	else
#endif
	    ret = fread((char*)&s_p_root, 1, sizeof (PACKED_ROOT), s_fp);

	if (ret != sizeof (PACKED_ROOT)) {
	    /*error("failed root read -- %d bytes instead of %d.\n",
		ret, sizeof (PACKED_ROOT));*/
	    return 0;
	}
	wp_bmap(&s_p_root.articles, 3);	/* converts subject_cnt too */
	if (s_p_root.articles < 0 || s_p_root.articles >= s_total.article) {
	    /*error("root has invalid values.\n");*/
	    return 0;
	}
	i = s_p_root.subject_cnt;
	if (i <= 0 || (subj_ptr - s_subject_array) + i > s_total.subject) {
	    /*error("root has invalid values.\n");*/
	    return 0;
	}
	for (SUBJECT *prev_sp = *subj_ptr; i--; prev_sp = sp, subj_ptr++) {
	    sp = *subj_ptr;
	    if (sp->thread_link == nullptr) {
		sp->thread_link = prev_sp->thread_link;
		prev_sp->thread_link = sp;
	    }
	    else {
		while (sp != prev_sp && sp->thread_link != *subj_ptr)
		    sp = sp->thread_link;
		if (sp == prev_sp)
		    continue;
		sp->thread_link = prev_sp->thread_link;
		prev_sp->thread_link = *subj_ptr;
	    }
	}
    }
    return 1;
}

static bool s_invalid_data{};

/* A simple routine that checks the validity of the article's subject value.
** A -1 means that it is nullptr, otherwise it should be an offset into the
** subject array we just unpacked.
*/
static SUBJECT *the_subject(int num)
{
    if (num == -1)
	return nullptr;
    if (num < 0 || num >= s_total.subject) {
	/*printf("Invalid subject in thread file: %d [%ld]\n", num, art_num);*/
	s_invalid_data = true;
	return nullptr;
    }
    return s_subject_array[num];
}

/* Ditto for author checking. */
static char *the_author(int num)
{
    if (num == -1)
	return nullptr;
    if (num < 0 || num >= s_total.author) {
	/*error("invalid author in thread file: %d [%ld]\n", num, art_num);*/
	s_invalid_data = true;
	return nullptr;
    }
    return savestr(s_author_array[num]);
}

/* Our parent/sibling information is a relative offset in the article array.
** zero for none.  Child values are always found in the very next array
** element if child_cnt is non-zero.
*/
static ARTICLE *the_article(int relative_offset, int num)
{
    union { ARTICLE* ap; int num; } uni;

    if (!relative_offset)
	return nullptr;
    num += relative_offset;
    if (num < 0 || num >= s_total.article) {
	/*error("invalid article offset in thread file.\n");*/
	s_invalid_data = true;
	return nullptr;
    }
    uni.num = num+1;
    return uni.ap;		/* slip them an offset in disguise */
}

/* Read the articles into their trees.  Point everything everywhere. */
static int read_articles()
{
    ARTICLE*article;
    int     ret;

    /* Build an array to interpret interlinkages of articles. */
    s_article_array = (ARTICLE**)safemalloc(s_total.article * sizeof (ARTICLE*));
    ARTICLE **art_ptr = s_article_array;

    s_invalid_data = false;
    for (int count = 0; count < s_total.article; count++) {
#ifdef SUPPORT_XTHREAD
	if (!g_datasrc->thread_dir)
	    ret = nntp_read((char*)&s_p_article, (long)sizeof (PACKED_ARTICLE));
	else
#endif
	    ret = fread((char*)&s_p_article, 1, sizeof (PACKED_ARTICLE), s_fp);

	if (ret != sizeof (PACKED_ARTICLE)) {
	    /*error("failed article read -- %d bytes instead of %d.\n",
		ret, sizeof (PACKED_ARTICLE));*/
	    return 0;
	}
	lp_bmap(&s_p_article.num, 2);
	wp_bmap(&s_p_article.subject, 8);

	article = *art_ptr++ = allocate_article(s_p_article.num > g_lastart?
						0 : s_p_article.num);
	article->date = s_p_article.date;
	if (g_olden_days < 2 && !(s_p_article.flags & HAS_XREFS))
	    article->xrefs = "";
	article->from = the_author(s_p_article.author);
	article->parent = the_article(s_p_article.parent, count);
	article->child1 = the_article((WORD)(s_p_article.child_cnt?1:0), count);
	article->sibling = the_article(s_p_article.sibling, count);
	article->subj = the_subject(s_p_article.subject);
	if (s_invalid_data) {
	    /* (Error already logged.) */
	    return 0;
	}
	/* This is OK because parent articles precede their children */
	if (article->parent) {
	    union { ARTICLE* ap; int num; } uni;
	    uni.ap = article->parent;
	    article->parent = s_article_array[uni.num-1];
	}
	else
	    article->sibling = nullptr;
	if (article->subj) {
	    article->flags |= AF_FROMTRUNCED | AF_THREADED
		    | ((s_p_article.flags & ROOT_ARTICLE)? AF_NONE : AF_HAS_RE);
	    /* Give this subject to any faked parent articles */
	    while (article->parent && !article->parent->subj) {
		article->parent->subj = article->subj;
		article = article->parent;
	    }
	} else
	    article->flags |= AF_FAKE;
    }

    /* We're done with most of the pointer arrays at this point. */
    safefree0(s_subject_array);
    safefree0(s_author_array);
    safefree0(s_strings);

    return 1;
}

/* Read the message-id strings and attach them to each article.  The data
** format consists of the mushed-together null-terminated strings (a domain
** name followed by all its unique-id prefixes) and then the article offsets
** to which they belong.  The first domain name was omitted, as it is a null
** domain for those truly weird message-id's without '@'s.
*/
static int read_ids()
{
    int i, count, len;

    if (!read_item(&s_strings, (MEM_SIZE)s_total.string2)
     || !read_item((char**)&s_ids,
		(MEM_SIZE)(s_total.article+s_total.domain+1) * sizeof (WORD))) {
	return 0;
    }
    wp_bmap(s_ids, s_total.article + s_total.domain + 1);

    char *string_ptr = s_strings;
    s_string_end = string_ptr + s_total.string2;

    if (s_string_end[-1] != '\0') {
	/*error("second string table is invalid.\n");*/
	return 0;
    }

    for (i = 0, count = s_total.domain + 1; count--; i++) {
	if (i) {
	    if (string_ptr >= s_string_end) {
		/*error("error unpacking domain strings.\n");*/
		return 0;
	    }
	    sprintf(g_buf, "@%s", string_ptr);
	    len = strlen(string_ptr) + 1;
	    string_ptr += len;
	} else {
	    *g_buf = '\0';
	    len = 0;
	}
	if (s_ids[i] != -1) {
	    if (s_ids[i] < 0 || s_ids[i] >= s_total.article) {
		/*error("error in id array.\n");*/
		return 0;
	    }
	    ARTICLE *article = s_article_array[s_ids[i]];
	    for (;;) {
		if (string_ptr >= s_string_end) {
		    /*error("error unpacking domain strings.\n");*/
		    return 0;
		}
		int len2 = strlen(string_ptr);
		article->msgid = safemalloc(len2 + len + 2 + 1);
		sprintf(article->msgid, "<%s%s>", string_ptr, g_buf);
		string_ptr += len2 + 1;
		if (g_msgid_hash) {
                    HASHDATUM data = hashfetch(g_msgid_hash, article->msgid, len2 + len + 2);
		    if (data.dat_len) {
			article->autofl = static_cast<autokill_flags>(data.dat_len) &(AUTO_SEL_MASK|AUTO_KILL_MASK);
			if ((data.dat_len & KF_AGE_MASK) == 0)
			    article->autofl |= AUTO_OLD;
			else
			    g_kf_changethd_cnt++;
			data.dat_len = 0;
			free(data.dat_ptr);
		    }
		    data.dat_ptr = (char*)article;
		    hashstorelast(data);
		}
		if (++i >= s_total.article + s_total.domain + !count) {
		    /*error("overran id array unpacking domains.\n");*/
		    return 0;
		}
		if (s_ids[i] != -1) {
		    if (s_ids[i] < 0 || s_ids[i] >= s_total.article)
			return 0;
		    article = s_article_array[s_ids[i]];
		} else
		    break;
	    }
	}
    }
    safefree0(s_ids);
    safefree0(s_strings);

    return 1;
}

/* And finally, turn all the links into real pointers and mark missing
** articles as read.
*/
static void tweak_data()
{
    int             count;
    ARTICLE*        ap;
    union { ARTICLE*ap; int num; } uni;

    ARTICLE **art_ptr = s_article_array;
    for (count = s_total.article; count--; ) {
	ap = *art_ptr++;
	if (ap->child1) {
	    uni.ap = ap->child1;
	    ap->child1 = s_article_array[uni.num-1];
	}
	if (ap->sibling) {
	    uni.ap = ap->sibling;
	    ap->sibling = s_article_array[uni.num-1];
	}
	if (!ap->parent)
	    link_child(ap);
    }

    art_ptr = s_article_array;
    for (count = s_total.article; count--; ) {
	ap = *art_ptr++;
	if (ap->subj && !(ap->flags & AF_FAKE))
	    cache_article(ap);
	else
	    onemissing(ap);
    }

    art_ptr = s_article_array;
    for (count = s_total.article; count--; ) {
	ap = *art_ptr++;
        autokill_flags fl = ap->autofl;
        if (fl != AUTO_KILL_NONE)
	    perform_auto_flags(ap, fl, fl, fl);
    }

    safefree0(s_article_array);
}

/* A shorthand for reading a chunk of the file into a malloc'ed array.
*/
static int read_item(char **dest, MEM_SIZE len)
{
    long ret;

    *dest = safemalloc(len);
#ifdef SUPPORT_XTHREAD
    if (!g_datasrc->thread_dir)
	ret = nntp_read(*dest, (long)len);
    else
#endif
	ret = fread(*dest, 1, (int)len, s_fp);

    if (ret != len) {
	free(*dest);
	*dest = nullptr;
	return 0;
    }
    putchar('.');
    fflush(stdout);
    return 1;
}

/* Determine this machine's byte map for WORDs and LONGs.  A byte map is an
** array of BYTEs (sizeof (WORD) or sizeof (LONG) of them) with the 0th BYTE
** being the byte number of the high-order byte in my <type>, and so forth.
*/
static void mybytemap(BMAP *map)
{
    union {
	BYTE b[sizeof (LONG)];
	WORD w;
	LONG l;
    }        u;
    int      i, j;

    BYTE *mp = &map->w[sizeof(WORD)];
    u.w = 1;
    for (i = sizeof (WORD); i > 0; i--) {
	for (j = 0; j < sizeof (WORD); j++) {
	    if (u.b[j] != 0)
		break;
	}
	if (j == sizeof (WORD))
	    goto bad_news;
	*--mp = j;
	while (u.b[j] != 0 && u.w)
	    u.w <<= 1;
    }

    mp = &map->l[sizeof (LONG)];
    u.l = 1;
    for (i = sizeof (LONG); i > 0; i--) {
	for (j = 0; j < sizeof (LONG); j++) {
	    if (u.b[j] != 0)
		break;
	}
	if (j == sizeof (LONG)) {
	  bad_news:
	    /* trouble -- set both to *something* consistent */
	    for (j = 0; j < sizeof (WORD); j++)
		map->w[j] = j;
	    for (j = 0; j < sizeof (LONG); j++)
		map->l[j] = j;
	    return;
	}
	*--mp = j;
	while (u.b[j] != 0 && u.l)
	    u.l <<= 1;
    }
}

/* Transform each WORD's byte-ordering in a buffer of the designated length.
*/
static void wp_bmap(WORD *buf, int len)
{
    union {
	BYTE b[sizeof (WORD)];
	WORD w;
    } in, out;

    if (s_word_same)
	return;

    while (len--) {
	in.w = *buf;
	for (int i = 0; i < sizeof (WORD); i++)
	    out.b[s_my_bmap.w[i]] = in.b[s_mt_bmap.w[i]];
	*buf++ = out.w;
    }
}

/* Transform each LONG's byte-ordering in a buffer of the designated length.
*/
static void lp_bmap(LONG *buf, int len)
{
    union {
	BYTE b[sizeof (LONG)];
	LONG l;
    } in, out;

    if (s_long_same)
	return;

    while (len--) {
	in.l = *buf;
	for (int i = 0; i < sizeof (LONG); i++)
	    out.b[s_my_bmap.l[i]] = in.b[s_mt_bmap.l[i]];
	*buf++ = out.l;
    }
}
