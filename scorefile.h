/* This file Copyright 1992 by Clifford A. Adams */
/* scorefile.h
 *
 */

#define DEFAULT_SCOREDIR "%+/scores"

struct sf_entry {
    int head_type;	/* header # (see head.h) */
    int score;		/* score change */
    char* str1;		/* first string part */
    char* str2;		/* second string part */
    COMPEX* compex;	/* regular expression ptr */
    char flags;		/* 1: regex is valid
			 * 2: rule has been applied to the current article.
			 * 4: use faster rule checking  (later)
			 */
};
/* note that negative header #s are used to indicate special entries... */

EXT int sf_num_entries INIT(0);	/* # of entries */
EXT SF_ENTRY* sf_entries;	/* array of entries */

#ifdef SCOREFILE_CACHE
/* for cached score rules */
struct sf_file {
    char* fname;
    int num_lines;
    int num_alloc;
    long line_on;
    char** lines;
};

EXT SF_FILE *sf_files INIT((SF_FILE*)NULL);
EXT int sf_num_files INIT(0);
#endif

EXT char **sf_abbr;		/* abbreviations */

/* when true, the scoring routine prints lots of info... */
EXT int sf_score_verbose INIT(false);

EXT bool sf_verbose INIT(true);  /* if true print more stuff while loading */

/* if true, only header types that are cached are scored... */
EXT bool cached_rescore INIT(false);

/* if true, newauthor is active */
EXT bool newauthor_active INIT(false);
/* bonus score given to a new (unscored) author */
EXT int newauthor INIT(0);

/* if true, reply_score is active */
EXT bool reply_active INIT(false);
/* score amount added to an article reply */
EXT int reply_score INIT(0);

/* should we match by pattern? */
EXT int sf_pattern_status INIT(false);

void sf_init(void);
void sf_clean(void);
void sf_grow(void);
int sf_check_extra_headers(char *);
void sf_add_extra_header(char *);
char *sf_get_extra_header(ART_NUM, int);
bool is_text_zero(char *);
char *sf_get_filename(int);
char *sf_cmd_fname(char *);
bool sf_do_command(char *cmd, bool check);
char *sf_freeform(char *, char *);
bool sf_do_line(char *line, bool check);
void sf_do_file(char *);
int score_match(char *, int);
int sf_score(ART_NUM);
char *sf_missing_score(char *);
void sf_append(char *);
char *sf_get_line(ART_NUM, int);
void sf_print_match(int);
void sf_exclude_file(char *);
void sf_edit_file(char *);
