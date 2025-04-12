/* trn/utf.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#ifndef UTF_H_INCLUDED
#define UTF_H_INCLUDED

#define USE_UTF_HACK

enum charset_type
{
    CHARSET_ASCII = 0x0000,
    CHARSET_UTF8 = 0x8000,
    CHARSET_ISO8859_1 = 0x4010,
    CHARSET_ISO8859_15 = 0x4011,
    CHARSET_WINDOWS_1252 = 0x4020,
    CHARSET_UNKNOWN = 0x0FFF
};

#define CHARSET_NAME_ASCII          "US"
#define CHARSET_NAME_UTF8           "UTF"
#define CHARSET_NAME_ISO8859_1      "Latin1"
#define CHARSET_NAME_ISO8859_15     "Latin9"
#define CHARSET_NAME_WINDOWS_1252   "CP1252"

using CODE_POINT = unsigned long;

charset_type utf_init(const char *from, const char *to);
const char *input_charset_name();
const char *output_charset_name();

bool at_norm_char(const char *s);

int byte_length_at(const char *s);
int visual_width_at(const char *s);
int visual_length_of(const char *s);
int visual_length_between(const char *s1, const char *s2);
int insert_unicode_at(char *s, CODE_POINT c);

enum : CODE_POINT
{
    INVALID_CODE_POINT = static_cast<CODE_POINT>(~0L)
};
CODE_POINT code_point_at(const char *s);

int put_char_adv(char **strptr, bool outputok);

char *create_utf8_copy(char *s);

void terminate_string_at_visual_index(char *s, int i);

#endif
