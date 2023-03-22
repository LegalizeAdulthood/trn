/* decode.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "term.h"
#include "head.h"
#include "art.h"
#include "artio.h"
#include "artstate.h"
#include "intrp.h"
#include "final.h"
#include "mime.h"
#include "util.h"
#include "util2.h"
#include "uudecode.h"
#include "decode.h"

#ifdef MSDOS
#include <direct.h>
#endif

#ifdef MSDOS
#define GOODCHARS                \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefghijklmnopqrstuvwxyz" \
    "0123456789-_^#%"
#else
#define BADCHARS "!$&*()|\'\";<>[]{}?/`\\ \t"
#endif

char *g_decode_filename{};

static bool bad_filename(const char *filename);

void decode_init()
{
}

char *decode_fix_fname(const char *s)
{
    char* t;
#ifdef MSDOS
    int dotcount = 0;
#endif

    if (!s)
	s = "unknown";

    safefree(g_decode_filename);
    g_decode_filename = safemalloc(strlen(s) + 2);

/*$$ we need to eliminate any "../"s from the string */
    while (*s == '/' || *s == '~') s++;
    for (t = g_decode_filename; *s; s++) {
#ifdef MSDOS
/*$$ we should also handle backslashes here */
	if (*s == '.' && (t == g_decode_filename || dotcount++))
	    continue;
#endif
	if (isprint(*s)
#ifdef GOODCHARS
	 && strchr(GOODCHARS, *s)
#else
	 && !strchr(BADCHARS, *s)
#endif
	)
	    *t++ = *s;
    }
    *t = '\0';
    if (t == g_decode_filename || bad_filename(g_decode_filename)) {
	*t++ = 'x';
	*t = '\0';
    }
    return g_decode_filename;
}

/* Returns nonzero if "filename" is a bad choice */
static bool bad_filename(const char *filename)
{
    int len = strlen(filename);
#ifdef MSDOS
    if (len == 3) {
	if (!strcasecmp(filename, "aux") || !strcasecmp(filename, "con")
	 || !strcasecmp(filename, "nul") || !strcasecmp(filename, "prn"))
	    return true;
    }
    else if (len == 4) {
	if (!strcasecmp(filename, "com1") || !strcasecmp(filename, "com2")
	 || !strcasecmp(filename, "com3") || !strcasecmp(filename, "com4")
	 || !strcasecmp(filename, "lpt1") || !strcasecmp(filename, "lpt2")
	 || !strcasecmp(filename, "lpt3"))
	    return true;
    }
#else
    if (len <= 2) {
	if (*filename == '.' && (*filename == '\0' || *filename == '.'))
	    return true;
    }
#endif
    return false;
}

/* Parse the subject looking for filename and part number information. */
char *decode_subject(ART_NUM artnum, int *partp, int *totalp)
{
    static char* subject = nullptr;
    char* filename;
    char* s;
    char* t;
    char* end;
    int part = -1, total = 0, hasdot = 0;

    *partp = part;
    *totalp = total;
    safefree(subject);
    subject = fetchsubj(artnum,true);
    if (!*subject)
	return nullptr;

    /* Skip leading whitespace and other garbage */
    s = subject;
    while (*s == ' ' || *s == '\t' || *s == '-') s++;
    if (!strncasecmp(s, "repost", 6)) {
	for (s += 6; *s == ' ' || *s == '\t'
	     || *s == ':' || *s == '-'; s++);
    }

    while (!strncasecmp(s, "re:", 3)) {
	s += 3;
	while (isspace(*s)) s++;
    }

    /* Get filename */

    /* Grab the first filename-like string.  Explicitly ignore strings with
     * prefix "v<digit>" ending in ":", since that is a popular volume/issue
     * representation syntax
     */
    end = s + strlen(s);
    do {
	while (*s && !isalnum(*s) && *s != '_') s++;
	filename = t = s;
	while (isalnum(*s) || *s == '-' || *s == '+' || *s == '&'
	      || *s == '_' || *s == '.') {
	    if (*s++ == '.')
		hasdot = 1;
	}
	if (!*s || *s == '\n')
	    return nullptr;
    } while (t == s || (t[0] == 'v' && isdigit(t[1]) && *s == ':'));
    *s++ = '\0';
    
    /* Try looking for a filename with a "." in it later in the subject line.
     * Exclude <digit>.<digit>, since that is usually a version number.
     */
    if (!hasdot) {
    	while (*(t = s) != '\0' && *s != '\n') {
	    while (isspace(*t)) t++;
	    for (s = t; isalnum(*s) || *s == '-' || *s == '+'
		 || *s == '&' || *s == '_' || *s == '.'; s++) {
		if (*s == '.' && 
		    (!isdigit(s[-1]) || !isdigit(s[1]))) {
		    hasdot = 1;
		}
	    }
	    if (hasdot && s > t) {
		filename = t;
		*s++ = '\0';
		break;
	    }
	    while (*s && *s != '\n' && !isalnum(*s)) s++;
    	}
    	s = filename + strlen(filename) + 1;
    }

    if (s >= end)
	return nullptr;

    /* Get part number */
    while (*s && *s != '\n') {
	/* skip over versioning */
	if (*s == 'v' && isdigit(s[1])) {
	    s++;
	    while (isdigit(*s)) s++;
	}
	/* look for "1/6" or "1 / 6" or "1 of 6" or "1-of-6" or "1o6" */
	if (isdigit(*s)
	 && (s[1] == '/'
	  || (s[1] == ' ' && s[2] == '/')
	  || (s[1] == ' ' && s[2] == 'o' && s[3] == 'f')
	  || (s[1] == '-' && s[2] == 'o' && s[3] == 'f')
	  || (s[1] == 'o' && isdigit(s[2])))) {
	    for (t = s; isdigit(t[-1]); t--) ;
	    part = atoi(t);
	    while (*++s != '\0' && *s != '\n' && !isdigit(*s)) ;
	    total = isdigit(*s)? atoi(s) : 0;
	    while (isdigit(*s)) s++;
	    /* We don't break here because we want the last item on the line */
	}

	/* look for "6 parts" or "part 1" */
	if (!strncasecmp("part", s, 4)) {
	    if (s[4] == 's') {
		for (t = s; t >= subject && !isdigit(*t); t--);
		if (t > subject) {
		    while (t > subject && isdigit(t[-1])) t--;
		    total = atoi(t);
		}
	    }
	    else {
		while (*s && *s != '\n' && !isdigit(*s)) s++;
		if (isdigit(*s))
		    part = atoi(s);
		s--;
	    }
	}
	if (*s)
	    s++;
    }

    if (total == 0 || part == -1 || part > total)
	return nullptr;
    *partp = part;
    *totalp = total;
    return filename;
}

/*
 * Handle a piece of a split file.
 */
int decode_piece(MIMECAP_ENTRY *mcp, char *first_line)
{
    char* dir;
    char* filename;
    FILE* fp;
    int part, total, state;
    DECODE_FUNC decoder;

    filename = decode_fix_fname(g_mime_section->filename);
    part = g_mime_section->part;
    total = g_mime_section->total;
    *g_msg = '\0';

    if (!total && g_is_mime)
	total = part = 1;

    if (mcp || total != 1 || part != 1) {
	/* Create directory to store parts and copy this part there. */
	dir = decode_mkdir(filename);
	if (!dir) {
	    strcpy(g_msg, "Failed.");
	    return 0;
	}
    }
    else
	dir = nullptr;

    if (mcp) {
	if (chdir(dir)) {
	    printf(g_nocd,dir) FLUSH;
	    sig_catcher(0);
	}
    }

    if (total != 1 || part != 1) {
	sprintf(g_buf, "Saving part %d ", part);
	if (total)
	    sprintf(g_buf+strlen(g_buf), "of %d ", total);
	strcat(g_buf, filename);
	fputs(g_buf,stdout);
	if (nowait_fork)
	    fflush(stdout);
	else
	    newline();

	sprintf(g_buf, "%s%d", dir, part);
	fp = fopen(g_buf, "w");
	if (!fp) {
	    strcpy(g_msg,"Failed."); /*$$*/
	    return 0;
	}
	while (readart(g_art_line,sizeof g_art_line)) {
	    if (mime_EndOfSection(g_art_line))
		break;
	    fputs(g_art_line,fp);
	    if (total == 0 && *g_art_line == 'e' && g_art_line[1] == 'n'
	     && g_art_line[2] == 'd' && isspace(g_art_line[3])) {
		/* This is the last part. Remember the fact */
		total = part;
		sprintf(g_buf, "%sCT", dir);
		g_tmpfp = fopen(g_buf, "w");
		if (!g_tmpfp)
		    /*os_perror(g_buf)*/;
		else {
		    fprintf(g_tmpfp, "%d\n", total);
		    fclose(g_tmpfp);
		}
	    }
	}
	fclose(fp);

	/* Retrieve any previously saved number of the last part */
	if (total == 0) {
	    sprintf(g_buf, "%sCT", dir);
	    if ((fp = fopen(g_buf, "r")) != nullptr) {
		if (fgets(g_buf, sizeof g_buf, fp)) {
		    total = atoi(g_buf);
		    if (total < 0)
			total = 0;
		}
		fclose(fp);
	    }
	    if (total == 0)
		return 1;
	}

	/* Check to see if we have all parts.  Start from the highest numbers
	 * as we are more likely not to have them.
	 */
	for (part = total; part; part--) {
	    sprintf(g_buf, "%s%d", dir, part);
	    fp = fopen(g_buf, "r");
	    if (!fp)
		return 1;
	    if (part != 1)
		fclose(fp);
	}
    }
    else {
	fp = nullptr;
	total = 1;
    }

    if (g_mime_section->type == MESSAGE_MIME) {
	mime_PushSection();
	mime_ParseSubheader(fp,first_line);
	first_line = nullptr;
    }
    g_mime_getc_line = first_line;
    decoder = decode_function(g_mime_section->encoding);
    if (!decoder) {
	strcpy(g_msg,"Unhandled encoding type -- aborting.");
	if (fp)
	    fclose(fp);
	if (dir)
	    decode_rmdir(dir);
	return 0;
    }

    /* Handle each part in order */
    for (state = DECODE_START, part = 1; part <= total; part++) {
	if (part != 1) {
	    sprintf(g_buf, "%s%d", dir, part);
	    fp = fopen(g_buf, "r");
	    if (!fp) {
		/*os_perror(g_buf);*/
		return 1;
	    }
	}

	state = decoder(fp, state);
	if (fp)
	    fclose(fp);
	if (state == DECODE_ERROR) {
	    strcpy(g_msg,"Failed."); /*$$*/
	    return 0;
	}
    }

    if (state != DECODE_DONE) {
	(void) decoder((FILE*)nullptr, DECODE_DONE);
	if (state != DECODE_MAYBEDONE) {
	    strcpy(g_msg,"Premature EOF.");
	    return 0;
	}
    }

    if (fp) {
	/* Cleanup all the pieces */
	for (part = 0; part <= total; part++) {
	    sprintf(g_buf, "%s%d", dir, part);
	    remove(g_buf);
	}
	sprintf(g_buf, "%sCT", dir);
	remove(g_buf);
    }

    if (mcp) {
	mime_Exec(mcp->command);
	remove(g_decode_filename);
	chdir("..");
    }

    if (dir)
	decode_rmdir(dir);

    return 1;
}

DECODE_FUNC decode_function(int encoding)
{
    switch (encoding) {
      case MENCODE_QPRINT:
	return qp_decode;
      case MENCODE_BASE64:
	return b64_decode;
      case MENCODE_UUE:
	return uudecode;
      case MENCODE_NONE:
	return cat_decode;
      default:
	return nullptr;
    }
}

/* return a directory to use for unpacking the pieces of a given filename */
char *decode_mkdir(const char *filename)
{
    static char dir[LBUFLEN];
    char* s;

#ifdef MSDOS
    interp(dir, sizeof dir, "%Y/parts/");
#else
    interp(dir, sizeof dir, "%Y/m-prts-%L/");
#endif
    strcat(dir, filename);
    s = dir + strlen(dir);
    if (s[-1] == '/')
	return nullptr;
    *s++ = '/';
    *s = '\0';
    if (makedir(dir, MD_FILE) != 0)
	return nullptr;
    return dir;
}

void decode_rmdir(char *dir)
{
    char* s;

    /* Remove trailing slash */
    s = dir + strlen(dir) - 1;
    *s = '\0';

    /*$$ conditionalize this */
    rmdir(dir);
}
