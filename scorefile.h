/* This file Copyright 1992 by Clifford A. Adams */
/* scorefile.h
 *
 */
#ifndef SCOREFILE_H
#define SCOREFILE_H

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

/* for cached score rules */
struct SF_FILE
{
    char* fname;
    int num_lines;
    int num_alloc;
    long line_on;
    char** lines;
};

extern int g_sf_num_entries;   /* # of entries */
extern SF_ENTRY *g_sf_entries; /* array of entries */
extern SF_FILE *g_sf_files;
extern int g_sf_num_files;
extern char **g_sf_abbr;        /* abbreviations */
extern int g_sf_score_verbose;  /* when true, the scoring routine prints lots of info... */
extern bool g_sf_verbose;       /* if true print more stuff while loading */
extern bool g_cached_rescore;   /* if true, only header types that are cached are scored... */
extern bool g_newauthor_active; /* if true, g_newauthor is active */
extern int g_newauthor;         /* bonus score given to a new (unscored) author */
extern bool g_reply_active;     /* if true, g_reply_score is active */
extern int g_reply_score;       /* score amount added to an article reply */
extern int g_sf_pattern_status; /* should we match by pattern? */

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

#endif
