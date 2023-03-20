/* This file Copyright 1992 by Clifford A. Adams */
/* scorefile.h
 *
 */

#define DEFAULT_SCOREDIR "%+/scores"

struct SF_ENTRY
{
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
EXT SF_ENTRY *sf_entries INIT(nullptr); /* array of entries */

/* for cached score rules */
struct SF_FILE
{
    char* fname;
    int num_lines;
    int num_alloc;
    long line_on;
    char** lines;
};

EXT SF_FILE *sf_files INIT(nullptr);
EXT int sf_num_files INIT(0);

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

void sf_init();
void sf_clean();
void sf_grow();
int sf_check_extra_headers(const char *head);
void sf_add_extra_header(const char *head);
char *sf_get_extra_header(ART_NUM art, int hnum);

/* Returns true if text pointed to by s is a text representation of
 * the number 0.  Used for error checking.
 * Note: does not check for trailing garbage ("+00kjsdfk" returns true).
 */
inline bool is_text_zero(const char *s)
{
    return *s == '0' || ((*s == '+' || *s == '-') && s[1]=='0');
}

char *sf_get_filename(int level);
char *sf_cmd_fname(char *s);
bool sf_do_command(char *cmd, bool check);
char *sf_freeform(char *start1, char *end1);
bool sf_do_line(char *line, bool check);
void sf_do_file(const char *fname);
int score_match(char *str, int ind);
int sf_score(ART_NUM a);
char *sf_missing_score(const char *line);
void sf_append(char *line);
char *sf_get_line(ART_NUM a, int h);
void sf_print_match(int indx);
void sf_exclude_file(const char *fname);
void sf_edit_file(const char *filespec);
