/* util2.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_UTIL2_H
#define TRN_UTIL2_H

char *save_str(const char *str);
char *safe_copy(char *to, const char *from, int len);
const char *copy_till(char *to, const char *from, int delim);
char       *file_exp(const char *text);
const char *in_string(const char *big, const char *little, bool case_matters);
char       *in_string(char *big, const char *little, bool case_matters);
char *read_auth_file(const char *file, char **pass_ptr);

#endif
