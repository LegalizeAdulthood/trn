/* util.h
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_UTIL_H
#define TRN_UTIL_H

#include "utf.h"

extern bool g_waiting; /* waiting for subprocess (in doshell)? */
extern bool g_nowait_fork;

/* the strlen and the buffer length of "some_buf" after a call to:
 *     some_buf = get_a_line(bufptr,bufsize,realloc,fp); */
extern int g_len_last_line_got;
extern MEM_SIZE g_buflen_last_line_got;

inline bool at_grey_space(const char *s)
{
    return ((s) && ((!at_norm_char(s)) || ((*s) && (*s) == ' ')));
}

/* is the string for makedir a directory name or a filename? */

enum makedir_name_type
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

void util_init();
void util_final();
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
bool makedir(char *dirname, makedir_name_type nametype);
void notincl(const char *feature);
void growstr(char **strptr, int *curlen, int newlen);
void setdef(char *buffer, const char *dflt);
#ifndef NO_FILELINKS
void safelink(char *old, char *new);
#endif
void   verify_sig();
double current_time();
time_t text2secs(const char *s, time_t defSecs);
char * secs2text(time_t secs);
char * temp_filename();
char * get_auth_user();
char * get_auth_pass();
char **prep_ini_words(INI_WORDS words[]);
void   unprep_ini_words(INI_WORDS words[]);
void   prep_ini_data(char *cp, const char *filename);
bool   parse_string(char **to, char **from);
char * next_ini_section(char *cp, char **section, char **cond);
char * parse_ini_section(char *cp, INI_WORDS words[]);
bool   check_ini_cond(char *cond);
char   menu_get_char();
int    edit_file(const char *fname);

inline int ini_len(const INI_WORDS *words)
{
    return words[0].checksum;
}
inline char **ini_values(INI_WORDS *words)
{
    return (char **) words[0].help_str;
}
inline char *ini_value(INI_WORDS *words, int num)
{
    return ini_values(words)[num];
}

inline void safefree(void *ptr)
{
    if (ptr)
        free(ptr);
}

template <typename T>
void safefree0(T *&ptr)
{
    if (ptr)
    {
        free(ptr);
        ptr = nullptr;
    }
}

#endif
