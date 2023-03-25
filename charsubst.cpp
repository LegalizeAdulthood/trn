/* charsubst.c
 * vi: set sw=4 ts=8 ai sm noet :
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

#include "common.h"
#include "util2.h"
#include "utf.h"
#include "charsubst.h"

/* Conversions are: plain, ISO->USascii, TeX->ISO, ISO->USascii monospaced */
char *g_charsets{"patm"};
char *g_charsubst{};

/* TeX encoding table - gives ISO char for "x (x=32..127) */

static Uchar s_textbl[96] = {
    0,  0,'"',  0,  0,  0,  0,'"',  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,196,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,214,
    0,  0,  0,  0,  0,220,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  '"',228,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,246,
    0,  0,  0,223,  0,252,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};
static char s_texchar = '\0';

static int Latin1toASCII(Uchar *asc, const Uchar *iso, int limit, int t);

int putsubstchar(int c, int limit, bool outputok)
{
    Uchar d, oc[2], nc[5];
    int t, i = 0;
    switch (*g_charsubst) {
      case 'm':
	t = 1;
	goto doconvert;
      case 'a': 
	t = 2;
	/* FALL THROUGH */
      doconvert:
	oc[0] = (Uchar)c;
	oc[1] = '\0';
	if ((i = Latin1toASCII(nc, oc, sizeof nc, t)) <= limit) {
	    if (outputok) {
		for (int t2 = 0; t2 < i; t2++)
		    putchar((char)nc[t2]);
	    }
	}
	else
	    i = -1;
	break;
      case 't':
	if (c == '\\' || c == '"') {
	    if (s_texchar && (c == '\\' || s_texchar != '\\')) {
		if (outputok)
		    putchar(s_texchar);
		i++;
	    }
	    s_texchar = (char)c;
	    break;
	}
	else if (s_texchar == '\\') {
	    if (outputok)
		putchar('\\');
	    if (limit == 1) {
		i = -2;
		break;
	    }
	    i++;
	}
	else if (s_texchar == '"') {
	    if (c < 32 || c > 128)
		d = '\0';
	    else
		d = s_textbl[c-32];
	    s_texchar = '\0'; 
	    if (d)
		c = d;
	    else {
		if (outputok)
		    putchar('"');
		if (limit == 1) {
		    i = -2;
		    break;
		}
		i++;
	    }
	}
	/* FALL THROUGH */
      default: 
	if (outputok)
	    putchar(c);
	i++;
	break;
    }
    return i;
}

const char *current_charsubst()
{
#ifdef USE_UTF_HACK
    static char show[50];
    const char* ics = input_charset_name();
    const char* ocs = output_charset_name();
    int maxlen = (sizeof show - 5)/2;
    if (strcmp(ics, ocs) == 0)
	sprintf(show, "[%.*s]", maxlen, ics, maxlen, ocs);
    else
	sprintf(show, "[%.*s->%.*s]", maxlen, ics, maxlen, ocs);
#else /*!USE_UTF_HACK */
    static const char* show;

    switch (*g_charsubst) {
      case 'm':
	if (g_verbose)
	    show = "[ISO->USmono] ";
	else
	    show = "[M] ";
	break;
      case 'a':
	if (g_verbose)
	    show = "[ISO->US] ";
	else
	    show = "[U] ";
	break;
      case 't':
	if (g_verbose)
	    show = "[TeX->ISO] ";
	else
	    show = "[T] ";
	break;
      default:
	show = "";
	break;
    }
#endif
    return show;
}

int strcharsubst(char *outb, const char *inb, int limit, char_int subst)
{
    const char* s;
    int len;
    switch (subst) {
      case 'm':
	return Latin1toASCII((Uchar*)outb, (const Uchar*)inb, limit, 1);
      case 'a':
	return Latin1toASCII((Uchar*)outb, (const Uchar*)inb, limit, 2);
      default:
	if ((s = strchr(inb,'\n')) != nullptr && s - inb + 1 < limit) {
	    len = s - inb + 1;
	    limit = len + 1;
	}
	else
	    len = strlen(inb);
	safecpy(outb, inb, limit);
	return len;
    }
}

/* The following is an adapted version of iso2asc by Markus Kuhn, 
   University of Erlangen, Germany <mskuhn@immd4.uni-erlangen.de>
*/

#define ISO_TABLES 2 /* originally: 7 */

/* Conversion tables for displaying the G1 set (0xa0-0xff) of
   ISO Latin 1 (ISO 8859-1) with 7-bit ASCII characters.

   Version 1.2 -- error corrections are welcome

   Table   Purpose
     0     universal table for many languages
     1     single-spacing universal table
     2     table for Danish, Dutch, German, Norwegian and Swedish
     3     table for Danish, Finnish, Norwegian and Swedish using
           the appropriate ISO 646 variant.
     4     table with RFC 1345 codes in brackets
     5     table for printers that allow overstriking with backspace

   Markus Kuhn <mskuhn@immd4.informatik.uni-erlangen.de>                 */
/* In this version, I have taken out all tables except 1 and 2 -ot */

#define SUB nullptr       /* used if no reasonable ASCII string is possible */

static char* iso2asc[ISO_TABLES][96] = {
 {
  " ","!","c",SUB,SUB,"Y","|",SUB,"\"","c","a","<","-","-","R","-",
  " ",SUB,"2","3","'","u","P",".",",","1","o",">",SUB,SUB,SUB,"?",
  "A","A","A","A","A","A","A","C","E","E","E","E","I","I","I","I",
  "D","N","O","O","O","O","O","x","O","U","U","U","U","Y","T","s",
  "a","a","a","a","a","a","a","c","e","e","e","e","i","i","i","i",
  "d","n","o","o","o","o","o",":","o","u","u","u","u","y","t","y"
 },{
  " ","!","c",SUB,SUB,"Y","|",SUB,"\"","(c)","a","<<","-","-","(R)","-",
  " ","+/-","2","3","'","u","P",".",",","1","o",">>"," 1/4"," 1/2"," 3/4","?",
  "A","A","A","A","Ae","Aa","AE","C","E","E","E","E","I","I","I","I",
  "D","N","O","O","O","O","Oe","x","Oe","U","U","U","Ue","Y","Th","ss",
  "a","a","a","a","ae","aa","ae","c","e","e","e","e","i","i","i","i",
  "d","n","o","o","o","o","oe",":","oe","u","u","u","ue","y","th","ij"
 }
};

/*
 *  Transform an 8-bit ISO Latin 1 string iso into a 7-bit ASCII string asc
 *  readable on old terminals using conversion table t.
 *
 *  worst case: strlen(iso) == 4*strlen(asc)
 */
static int Latin1toASCII(Uchar *asc, const Uchar *iso, int limit, int t)
{
    Uchar* s = asc;
    char* p;
    char** tab;

    if (iso == nullptr || asc == nullptr || limit <= 0)
	return 0;
    if (limit == 1)
	goto done;
    t--;	/* offset correction -ot */
    tab = iso2asc[t] - 0xa0;
    while (*iso) {
	if (*iso > 0x9f) {
	    p = tab[*iso++];
	    if (p) {
		while (*p) {
		    *s++ = *p++;
		    if (!--limit)
			goto done;
		}
	    }
	}
	else {
	    if (*iso < 0x80)
		*s++ = *iso++;
	    else {
		*s++ = ' ';
		iso++;
	    }
	    if (!--limit)
		break;
	}
    }
  done:
    *s = '\0';
    return s - asc;
}
