/* cache.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* Subjects get their own structure */

struct SUBJECT
{
    SUBJECT* next;
    SUBJECT* prev;
    ARTICLE* articles;
    ARTICLE* thread;
    SUBJECT* thread_link;
    char* str;
    time_t date;
    short flags;
    short misc;		/* used for temporary totals and subject numbers */
};

/* subject flags */
enum
{
    SF_SEL = 0x0001,
    SF_DEL = 0x0002,
    SF_DELSEL = 0x0004,
    SF_OLDVISIT = 0x0008,
    SF_INCLUDED = 0x0010,
    SF_VISIT = 0x0200,
    SF_WASSELECTED = 0x0400,
    SF_SUBJTRUNCED = 0x1000,
    SF_ISOSUBJ = 0x2000
};

/* This is our article-caching structure */

struct ARTICLE
{
    ART_NUM num;
    time_t date;
    SUBJECT* subj;
    char* from;
    char* msgid;
    char* xrefs;
    ARTICLE* parent;		/* parent article */
    ARTICLE* child1;		/* first child of a chain */
    ARTICLE* sibling;		/* our next sibling */
    ARTICLE* subj_next;		/* next article in subject order */
    long bytes;
    long lines;
#ifdef SCORE
    int score;
    unsigned short scoreflags;
#endif
    unsigned short flags;	/* article state flags */
    unsigned short flags2;	/* more state flags */
    unsigned short autofl;	/* auto-processing flags */
};

/* article flags */
enum
{
    AF_SEL = 0x0001,
    AF_DEL = 0x0002,
    AF_DELSEL = 0x0004,
    AF_OLDSEL = 0x0008,
    AF_INCLUDED = 0x0010,
    AF_UNREAD = 0x0020,
    AF_CACHED = 0x0040,
    AF_THREADED = 0x0080,
    AF_EXISTS = 0x0100,
    AF_HAS_RE = 0x0200,
    AF_KCHASE = 0x0400,
    AF_MCHASE = 0x0800,
    AF_YANKBACK = 0x1000,
    AF_FROMTRUNCED = 0x2000,
    AF_TMPMEM = 0x4000,
    AF_FAKE = 0x8000,
    AF2_WASUNREAD = 0x0001,
    AF2_NODEDRAWN = 0x0002,
    AF2_CHANGED = 0x0004,
    AF2_BOGUS = 0x0008
};

/* See kfile.h for the AUTO_* flags */

#define article_ptr(an)      ((ARTICLE*)listnum2listitem(article_list,(long)(an)))
#define article_num(ap)      ((ap)->num)
#define article_find(an)     ((an) <= lastart && article_hasdata(an)? \
			      article_ptr(an) : nullptr)
#define article_walk(cb,ag)  walk_list(article_list,cb,ag)
#define article_hasdata(an)  existing_listnum(article_list,(long)(an),0)
#define article_first(an)    existing_listnum(article_list,(long)(an),1)
#define article_next(an)     existing_listnum(article_list,(long)(an)+1,1)
#define article_last(an)     existing_listnum(article_list,(long)(an),-1)
#define article_prev(an)     existing_listnum(article_list,(long)(an)-1,-1)
#define article_nextp(ap)    ((ARTICLE*)next_listitem(article_list,(char*)(ap)))

#define article_exists(an)   (article_ptr(an)->flags & AF_EXISTS)
#define article_unread(an)   (article_ptr(an)->flags & AF_UNREAD)

#define was_read(an)	    (!article_hasdata(an) || !article_unread(an))
#define is_available(an)    ((an) <= lastart && article_hasdata(an) \
			  && article_exists(an))
#define is_unavailable(an)  (!is_available(an))

EXT LIST* article_list INIT(nullptr);		/* a list of ARTICLEs */
EXT ARTICLE** artptr_list INIT(nullptr);	/* the article-selector creates this */
EXT ARTICLE** artptr;			/* ditto -- used for article order */
EXT ART_NUM artptr_list_size INIT(0);

EXT ART_NUM srchahead INIT(0); 	/* are we in subject scan mode? */
				/* (if so, contains art # found or -1) */

EXT ART_NUM first_cached;
EXT ART_NUM last_cached;
EXT bool cached_all_in_range;
EXT ARTICLE* sentinel_artp;

#define DONT_FILL_CACHE	false
#define FILL_CACHE	true

EXT SUBJECT* first_subject INIT(nullptr);
EXT SUBJECT* last_subject INIT(nullptr);

EXT bool untrim_cache INIT(false);

#ifdef PENDING
EXT ART_NUM subj_to_get;
EXT ART_NUM xref_to_get;
#endif

void cache_init();
void build_cache();
void close_cache();
void cache_article(ARTICLE *ap);
void check_for_near_subj(ARTICLE *ap);
void change_join_subject_len(int);
void check_poster(ARTICLE *ap);
void uncache_article(ARTICLE *ap, bool remove_empties);
char *fetchcache(ART_NUM artnum, int which_line, bool fill_cache);
char *get_cached_line(ARTICLE *ap, int which_line, bool no_truncs);
void set_subj_line(ARTICLE *ap, char *subj, int size);
int decode_header(char *, char *, int);
void dectrl(char *);
void set_cached_line(ARTICLE *ap, int which_line, char *s);
int subject_cmp(char *key, int keylen, HASHDATUM data);
#ifdef PENDING
void look_ahead();
#endif
void cache_until_key();
#ifdef PENDING
bool cache_subjects();
#endif
bool cache_xrefs();
bool cache_all_arts();
bool cache_unread_arts();
bool art_data(ART_NUM first, ART_NUM last, bool cheating, bool all_articles);
bool cache_range(ART_NUM, ART_NUM);
void clear_article(ARTICLE *ap);
