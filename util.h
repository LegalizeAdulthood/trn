/* util.h
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "utf.h"
EXT bool waiting INIT(false);  	/* waiting for subprocess (in doshell)? */
EXT bool nowait_fork INIT(false);
EXT bool export_nntp_fds INIT(false);

/* the strlen and the buffer length of "some_buf" after a call to:
 *     some_buf = get_a_line(bufptr,bufsize,realloc,fp); */
EXT int len_last_line_got INIT(0);
EXT MEM_SIZE buflen_last_line_got INIT(0);

#define AT_GREY_SPACE(s) ((s) && ((!at_norm_char(s)) || ((*s) && (*s) == ' ')))
#define AT_NORM_CHAR(s)  at_norm_char(s)

/* is the string for makedir a directory name or a filename? */

#define MD_DIR 	0
#define MD_FILE 1

/* a template for parsing an ini file */

struct ini_words {
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
int doshell(char *, char *);
#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE);
char *saferealloc(char *, MEM_SIZE);
#endif
char *safecat(char *, char *, int);
#ifdef SETUIDGID
int eaccess(char *, int);
#endif
char *trn_getwd(char *, int);
char *get_a_line(char *buffer, int buffer_length, bool realloc_ok, FILE *fp);
int makedir(char *, int);
void notincl(char *);
void growstr(char **, int *, int);
void setdef(char *, char *);
#ifndef NO_FILELINKS
void safelink(char *, char *);
#endif
void verify_sig();
double current_time();
time_t text2secs(char *, time_t);
char *secs2text(time_t);
char *temp_filename();
#ifdef SUPPORT_NNTP
char *get_auth_user();
char *get_auth_pass();
#endif
#if defined(USE_GENAUTH) && defined(SUPPORT_NNTP)
char *get_auth_command();
#endif
char **prep_ini_words(INI_WORDS *);
void unprep_ini_words(INI_WORDS *);
void prep_ini_data(char *, char *);
bool parse_string(char **, char **);
char *next_ini_section(char *, char **, char **);
char *parse_ini_section(char *, INI_WORDS *);
bool check_ini_cond(char *);
char menu_get_char();
int edit_file(char *);
