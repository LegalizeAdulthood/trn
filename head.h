/* head.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#define HEAD_FIRST SOME_LINE

/* types of header lines (if only C really believed in enums)
 * (These must stay in alphabetic order at least in the first letter.
 * Within each letter it helps to arrange in increasing likelihood.)
 */

enum header_line_type
{
    PAST_HEADER = 0,                     /* body */
    SHOWN_LINE = (PAST_HEADER + 1),      /* unrecognized but shown */
    HIDDEN_LINE = (SHOWN_LINE + 1),      /* unrecognized but hidden */
    CUSTOM_LINE = (HIDDEN_LINE + 1),     /* to isolate a custom line */
    SOME_LINE = (CUSTOM_LINE + 1),       /* default for unrecognized */
    AUTHOR_LINE = (SOME_LINE + 1),       /* Author */
    BYTES_LINE = (AUTHOR_LINE + 1),      /* Bytes */
    CONTNAME_LINE = (BYTES_LINE + 1),    /* Content-Name */
    CONTDISP_LINE = (CONTNAME_LINE + 1), /* Content-Disposition */
    CONTLEN_LINE = (CONTDISP_LINE + 1),  /* Content-Length */
    CONTXFER_LINE = (CONTLEN_LINE + 1),  /* Content-Transfer-Encoding */
    CONTTYPE_LINE = (CONTXFER_LINE + 1), /* Content-Type */
    DIST_LINE = (CONTTYPE_LINE + 1),     /* distribution */
    DATE_LINE = (DIST_LINE + 1),         /* date */
    EXPIR_LINE = (DATE_LINE + 1),        /* expires */
    FOLLOW_LINE = (EXPIR_LINE + 1),      /* followup-to */
    FROM_LINE = (FOLLOW_LINE + 1),       /* from */
    INREPLY_LINE = (FROM_LINE + 1),      /* in-reply-to */
    KEYW_LINE = (INREPLY_LINE + 1),      /* keywords */
    LINES_LINE = (KEYW_LINE + 1),        /* lines */
    MIMEVER_LINE = (LINES_LINE + 1),     /* mime-version */
    MSGID_LINE = (MIMEVER_LINE + 1),     /* message-id */
    NGS_LINE = (MSGID_LINE + 1),         /* newsgroups */
    PATH_LINE = (NGS_LINE + 1),          /* path */
    RVER_LINE = (PATH_LINE + 1),         /* relay-version */
    REPLY_LINE = (RVER_LINE + 1),        /* reply-to */
    REFS_LINE = (REPLY_LINE + 1),        /* references */
    SUMRY_LINE = (REFS_LINE + 1),        /* summary */
    SUBJ_LINE = (SUMRY_LINE + 1),        /* subject */
    XREF_LINE = (SUBJ_LINE + 1),         /* xref */
    HEAD_LAST = (XREF_LINE + 1),         /* total # of headers */
};

struct HEADTYPE
{
    char* name;			/* header line identifier */
    ART_POS minpos;		/* pointer to beginning of line in article */
    ART_POS maxpos;		/* pointer to end of line in article */
    char length;		/* the header's string length */
    char flags;			/* the header's flags */
};

struct USER_HEADTYPE
{
    char* name;			/* user-defined headers */
    char length;		/* the header's string length */
    char flags;			/* the header's flags */
};

enum
{
    HT_HIDE = 0x01,     /* hide this line */
    HT_MAGIC = 0x02,    /* do any special processing on this line */
    HT_CACHED = 0x04,   /* this information is cached article data */
    HT_DEFHIDE = 0x08,  /* hidden by default */
    HT_DEFMAGIC = 0x10, /* magic by default */
    HT_MAGICOK = 0x20   /* magic even possible for line */
};

/* This array must stay in the same order as the list above */

#ifndef DOINIT
EXT HEADTYPE htype[HEAD_LAST];
#else

#define HIDDEN    (HT_HIDE|HT_DEFHIDE)
#define MAGIC_ON  (HT_MAGICOK|HT_MAGIC|HT_DEFMAGIC)
#define MAGIC_OFF (HT_MAGICOK)

#define XREF_CACHED HT_CACHED
#define NGS_CACHED  0
#define FILT_CACHED 0

HEADTYPE htype[HEAD_LAST] = {
 /* name             minpos   maxpos  length   flag */
    {"",/*BODY*/	0,	0,	0,	0		},
    {"",/*SHOWN*/	0,	0,	0,	0		},
    {"",/*HIDDEN*/	0,	0,	0,	HIDDEN		},
    {"",/*CUSTOM*/	0,	0,	0,	0		},
    {"unrecognized",	0,	0,	12,	HIDDEN		},
    {"author",		0,	0,	6,	0		},
    {"bytes",		0,	0,	5,	HIDDEN|HT_CACHED},
    {"content-name",	0,	0,	12,	HIDDEN		},
    {"content-disposition",
			0,	0,	19,	HIDDEN		},
    {"content-length",	0,	0,	14,	HIDDEN		},
    {"content-transfer-encoding",
			0,	0,	25,	HIDDEN		},
    {"content-type",	0,	0,	12,	HIDDEN		},
    {"distribution",	0,	0,	12,	0		},
    {"date",		0,	0,	4,	MAGIC_ON	},
    {"expires",		0,	0,	7,	HIDDEN|MAGIC_ON	},
    {"followup-to",	0,	0,	11,	0		},
    {"from",		0,	0,	4,	MAGIC_OFF|HT_CACHED},
    {"in-reply-to",	0,	0,	11,	HIDDEN		},
    {"keywords",	0,	0,	8,	0		},
    {"lines",		0,	0,	5,	HT_CACHED	},
    {"mime-version",	0,	0,	12,	MAGIC_ON|HIDDEN	},
    {"message-id",	0,	0,	10,	HIDDEN|HT_CACHED},
    {"newsgroups",	0,	0,	10,	MAGIC_ON|HIDDEN|NGS_CACHED},
    {"path",		0,	0,	4,	HIDDEN		},
    {"relay-version",	0,	0,	13,	HIDDEN		},
    {"reply-to",	0,	0,	8,	0		},
    {"references",	0,	0,	10,	HIDDEN|FILT_CACHED},
    {"summary",		0,	0,	7,	0		},
    {"subject",		0,	0,	7,	MAGIC_ON|HT_CACHED},
    {"xref",		0,	0,	4,	HIDDEN|XREF_CACHED},
};

#undef HIDDEN
#undef MAGIC_ON
#undef MAGIC_OFF
#undef NGS_CACHED
#undef XREF_CACHED

#endif

EXT USER_HEADTYPE* user_htype INIT(nullptr);
EXT short user_htypeix[26];
EXT int user_htype_cnt INIT(0);
EXT int user_htype_max INIT(0);

EXT ART_NUM parsed_art INIT(0);		/* the article number we've parsed */
EXT ARTICLE* parsed_artp INIT(nullptr);	/* the article ptr we've parsed */
EXT int in_header INIT(0);		/* are we decoding the header? */
EXT char* headbuf;
EXT long headbuf_size;

#define PREFETCH_SIZE 5

#define fetchsubj(artnum,copy) prefetchlines(artnum,SUBJ_LINE,copy)
#define fetchfrom(artnum,copy) prefetchlines(artnum,FROM_LINE,copy)
#define fetchxref(artnum,copy) prefetchlines(artnum,XREF_LINE,copy)

void head_init();
#ifdef DEBUG
void dumpheader(char *where);
#endif
int set_line_type(char *bufptr, const char *colon);
int get_header_num(char *s);
void start_header(ART_NUM artnum);
void end_header_line();
bool parseline(char *art_buf, int newhide, int oldhide);
void end_header();
bool parseheader(ART_NUM artnum);
char *fetchlines(ART_NUM artnum, int which_line);
char *mp_fetchlines(ART_NUM artnum, int which_line, int pool);
char *prefetchlines(ART_NUM artnum, int which_line, bool copy);
