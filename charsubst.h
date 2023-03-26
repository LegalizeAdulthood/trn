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
#ifndef TRN_CHARSUBST_H
#define TRN_CHARSUBST_H

/* Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced */
extern char* g_charsets;
extern char* g_charsubst;

#define HEADER_CONV() (g_charsubst[0]=='a' || g_charsubst[0]=='m'? g_charsubst[0] : '\0')

int putsubstchar(int c, int limit, bool outputok);
const char *current_charsubst();
int strcharsubst(char *outb, const char *inb, int limit, char_int subst);

#endif
