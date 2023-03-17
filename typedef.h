/* typedef.h
 */

/* some important types */

typedef int		NG_NUM;		/* newsgroup number */
typedef long		ART_NUM;	/* article number */
typedef long		ART_UNREAD;	/* could be short to save space */
typedef long		ART_POS;	/* char position in article file */
typedef int		ART_LINE;	/* line position in article file */
typedef long		ACT_POS;	/* char position in active file */
typedef unsigned int	MEM_SIZE;	/* for passing to malloc */
typedef unsigned char	Uchar;		/* more space-efficient */

/* addng.h */

struct ADDGROUP;

/* cache.h */

struct SUBJECT;
struct ARTICLE;

/* color.ih */

struct COLOR_OBJ;

/* datasrc.h */

struct SRCFILE;
struct DATASRC;

/* hash.h */

struct HASHDATUM;

/* hash.ih */

struct HASHENT;
struct HASHTABLE;

/* head.h */

struct HEADTYPE;
struct USER_HEADTYPE;

/* list.h */

struct LISTNODE;
struct LIST;

/* mime.h */

struct HBLK;
struct MIME_SECT;
struct HTML_TAGS;
struct MIMECAP_ENTRY;

/* ngdata.h */

struct NGDATA;

/* nntpclient.h */

struct NNTPLINK;

/* rcstuff.h */

struct NEWSRC;
struct MULTIRC;

/* rt-mt.ih */

struct PACKED_ROOT;
struct PACKED_ARTICLE;
struct TOTAL;
struct BMAP;

/* rt-page.h */

union SEL_UNION;
struct SEL_ITEM;

/* scan.h */

struct PAGE_ENT;
struct SCONTEXT;

/* scanart.h */

struct SA_ENTRYDATA;

/* scorefile.h */

struct SF_ENTRY;
struct SF_FILE;

/* search.h */

struct COMPEX;

/* term.ih */

struct KEYMAP;

/* univ.h */

struct UNIV_GROUPMASK_DATA;
struct UNIV_CONFIGFILE_DATA;
struct UNIV_VIRT_DATA;
struct UNIV_VIRT_GROUP;
struct UNIV_NEWSGROUP;
struct UNIV_TEXTFILE;
union UNIV_DATA;
struct UNIV_ITEM;

/* util.h */

struct INI_WORDS;
