/* util.h
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef UTIL_H
#define UTIL_H

#include "utf.h"

extern bool g_waiting; /* waiting for subprocess (in doshell)? */
extern bool g_nowait_fork;
extern bool export_nntp_fds;

/* the strlen and the buffer length of "some_buf" after a call to:
 *     some_buf = get_a_line(bufptr,bufsize,realloc,fp); */
extern int g_len_last_line_got;
extern MEM_SIZE g_buflen_last_line_got;

inline bool at_grey_space(const char *s)
{
    return ((s) && ((!at_norm_char(s)) || ((*s) && (*s) == ' ')));
}

/* is the string for makedir a directory name or a filename? */

enum
{
    MD_DIR = 0,
    MD_FILE = 1
};

/* a template for parsing an ini file */

struct INI_WORDS
{
    int checksum;
    char* item;
    char* help_str;
};

#define INI_LEN(words)         (words)[0].checksum
#define INI_VALUES(words)      ((char**)(words)[0].help_str)
#define INI_VALUE(words,num)   INI_VALUES(words)[num]

#define safefree(ptr)  if (!ptr) ; else free((char*)(ptr))
#define safefree0(ptr)  if (!ptr) ; else free((char*)(ptr)), (ptr)=0

void util_init();
int doshell(const char *sh, const char *cmd);
#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE size);
char *saferealloc(char *where, MEM_SIZE size);
#endif
char *safecat(char *to, const char *from, int len);
#ifdef SETUIDGID
int eaccess(char *, int);
#endif
char *trn_getwd(char *buf, int buflen);
char *get_a_line(char *buffer, int buffer_length, bool realloc_ok, FILE *fp);
int makedir(char *dirname, int nametype);
void notincl(const char *feature);
void growstr(char **strptr, int *curlen, int newlen);
void setdef(char *buffer, const char *dflt);
#ifndef NO_FILELINKS
void safelink(char *old, char *new);
#endif
void verify_sig();
double current_time();
time_t text2secs(char *s, time_t defSecs);
char *secs2text(time_t secs);
char *temp_filename();
char *get_auth_user();
char *get_auth_pass();
char **prep_ini_words(INI_WORDS words[]);
void unprep_ini_words(INI_WORDS words[]);
void prep_ini_data(char *cp, char *filename);
bool parse_string(char **to, char **from);
char *next_ini_section(char *cp, char **section, char **cond);
char *parse_ini_section(char *cp, INI_WORDS words[]);
bool check_ini_cond(char *cond);
char menu_get_char();
int edit_file(char *fname);

#endif
