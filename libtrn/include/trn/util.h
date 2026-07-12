/* trn/util.h
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_UTIL_H
#define TRN_UTIL_H

#include <config/typedef.h>
#include <trn/utf.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string_view>

extern bool g_waiting; // waiting for subprocess (in doshell)?
extern bool g_no_wait_fork;

// the strlen and the buffer length of "some_buf" after a call to:
//     some_buf = get_a_line(bufptr,bufsize,realloc,fp);
extern int g_len_last_line_got;
extern MemorySize g_buf_len_last_line_got;

inline bool at_grey_space(const char *s)
{
    return ((s) && ((!at_norm_char(s)) || ((*s) && (*s) == ' ')));
}

// is the string for makedir a directory name or a filename?

enum MakeDirNameType
{
    MD_DIR = 0,
    MD_FILE = 1
};

class IniSchema;
class IniSectionValues;

void util_init();
void util_final();
int  do_shell(const char *shell, const char *cmd);
#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size);
char *safe_realloc(char *where, MemorySize size);
#endif
char *safe_cat(char *to, const char *from, int len);
#ifdef SETUIDGID
int eaccess(char *, int);
#endif
char *trn_getwd(char *buf, int buflen);
char *get_a_line(char *buffer, int buffer_length, bool realloc_ok, std::FILE *fp);
bool  make_dir(const char *dirname, MakeDirNameType nametype);
void  not_incl(std::string_view feature);
void  grow_str(char **strptr, int *curlen, int newlen);
void  set_def(char *buffer, const char *dflt);
#ifndef NO_FILELINKS
void safe_link(char *old_name, char *new_name);
#endif
void        verify_sig();
double      current_time();
std::time_t text_to_secs(const char *s, std::time_t defSecs);
const char *secs_to_text(std::time_t secs);
char       *temp_filename();
char       *get_auth_user();
char       *get_auth_pass();
void        prep_ini_data(char *cp, const char *filename);
bool        parse_string(char **to, char **from);
char       *next_ini_section(char *cp, char **section, char **cond);
char       *parse_ini_section(char *cp, const IniSchema &schema, IniSectionValues &values);
bool        check_ini_cond(char *cond);
char        menu_get_char();
int         edit_file(const char *fname);

inline void safe_free(void *ptr)
{
    if (ptr)
    {
        std::free(ptr);
    }
}

template <typename T>
void safe_free0(T *&ptr)
{
    if (ptr)
    {
        std::free(ptr);
        ptr = nullptr;
    }
}

#endif
