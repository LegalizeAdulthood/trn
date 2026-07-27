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
#include <filesystem>
#include <string>
#include <string_view>

extern bool g_waiting; // waiting for subprocess (in doshell)?
extern bool g_no_wait_fork;

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
class IniSection;
class IniSectionValues;

void util_init();
void util_final();
int  do_shell(std::string_view shell, std::string_view cmd);
#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size);
char *safe_realloc(char *where, MemorySize size);
#endif
int         eaccess(const std::filesystem::path &filename, int mode);
std::string trn_getwd();
std::string get_a_line(std::FILE *fp);
bool        make_dir(const std::filesystem::path &dirname, MakeDirNameType nametype);
void        not_incl(std::string_view feature);
std::string set_def(std::string_view command, std::string_view dflt);
void        verify_sig();
double      current_time();
std::time_t text_to_secs(std::string_view text, std::time_t defSecs);
std::string secs_to_text(std::time_t secs);
std::string temp_filename();
std::string get_auth_user();
std::string get_auth_pass();
void        parse_string(std::string &to, std::string_view &from);
bool        parse_ini_section(const IniSection &section, const IniSchema &schema, IniSectionValues &values);
bool        check_ini_cond(std::string_view cond);
char        menu_get_char();
int         edit_file(std::string_view fname);

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
