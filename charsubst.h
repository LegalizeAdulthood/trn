/* charsubst.h
 */
/*
 * Permission is hereby granted to copy, reproduce, redistribute or otherwise
 * use this software as long as: there is no monetary profit gained
 * specifically from the use or reproduction of this software, it is not
 * sold, rented, traded or otherwise marketed, and this copyright notice is
 * included prominently in any copy made. 
 *
 * The authors make no claims as to the fitness or correctness of this software
 * for any use whatsoever, and it is provided as is. Any use of this software
 * is at the user's own risk. 
 */

/* Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced */
EXT char* charsets INIT("patm");
EXT char* charsubst;

#define HEADER_CONV() (*charsubst=='a' || *charsubst=='m'? *charsubst : '\0')

int putsubstchar(int c, int limit, bool outputok);
const char *current_charsubst();
int strcharsubst(char *, char *, int, char_int);
