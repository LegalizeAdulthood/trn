/* rt-util.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "hash.h"
#include "cache.h"
#include "ngdata.h"
#include "artio.h"
#include "rthread.h"
#include "rt-select.h"
#include "term.h"
#include "nntpclient.h"
#include "charsubst.h"
#include "datasrc.h"
#include "nntp.h"
#include "intrp.h"
#include "ng.h"
#include "util.h"
#include "util2.h"
#ifdef USE_TK
#include "tkstuff.h"
#endif
#include "INTERN.h"
#include "rt-util.h"
#include "rt-util.ih"

/* Name-munging routines written by Ross Ridge.
** Enhanced by Wayne Davison.
*/

/* Extract the full-name part of an email address, returning NULL if not
** found.
*/
char*
extract_name(name)
char* name;
{
    char* s;
    char* lparen;
    char* rparen;
    char* langle;

    while (isspace(*name)) name++;

    lparen = index(name, '(');
    rparen = rindex(name, ')');
    langle = index(name, '<');
    if (!lparen && !langle)
	return NULL;
    else if (langle && (!lparen || !rparen || lparen>langle || rparen<langle)) {
	if (langle == name)
	    return NULL;
	*langle = '\0';
    } else {
	name = lparen;
	*name++ = '\0';
	while (isspace(*name)) name++;
	if (name == rparen)
	    return NULL;
	if (rparen != NULL)
	    *rparen = '\0';
    }

    if (*name == '"') {
	name++;
	while (isspace(*name)) name++;
	if ((s = rindex(name, '"')) != NULL)
	    *s = '\0';
    }
    return name;
}

/* If necessary, compress a net user's full name by playing games with
** initials and the middle name(s).  If we start with "Ross Douglas Ridge"
** we try "Ross D Ridge", "Ross Ridge", "R D Ridge" and finally "R Ridge"
** before simply truncating the thing.  We also turn "R. Douglas Ridge"
** into "Douglas Ridge" and "Ross Ridge D.D.S." into "Ross Ridge" as a
** first step of the compaction, if needed.
*/
char*
compress_name(name, max)
char* name;
int max;
{
    register char* s;
    register char* last;
    register char* mid;
    register char* d;
    register int len, namelen, midlen;
    int notlast;

try_again:
    /* First remove white space from both ends. */
    while (isspace(*name)) name++;
    if ((len = strlen(name)) == 0)
	return name;
    s = name + len - 1;
    while (isspace(*s)) s--;
    s[1] = '\0';
    if (s - name + 1 <= max)
	return name;

    /* Look for characters that likely mean the end of the name
    ** and the start of some hopefully uninteresting additional info.
    ** Spliting at a comma is somewhat questionalble, but since
    ** "Ross Ridge, The Great HTMU" comes up much more often than 
    ** "Ridge, Ross" and since "R HTMU" is worse than "Ridge" we do
    ** it anyways.
    */
    for (d = name + 1; *d; d++) {
	if (*d == ',' || *d == ';' || *d == '(' || *d == '@'
	 || (*d == '-' && (d[1] == '-' || d[1] == ' '))) {
	    *d-- = '\0';
	    s = d;
	    break;
	}
    }

    /* Find the last name */
    do {
	notlast = 0;
	while (isspace(*s)) s--;
	s[1] = '\0';
	len = s - name + 1;
	if (len <= max)
	    return name;
	/* If the last name is an abbreviation it's not the one we want. */
	if (*s == '.')
	    notlast = 1;
	while (!isspace(*s)) {
	    if (s == name) {	/* only one name */
		name[max] = '\0';
		return name;
	    }
	    if (isdigit(*s))	/* probably a phone number */
		notlast = 1;	/* so chuck it */
	    s--;
	}
    } while (notlast);

    last = s-- + 1;

    /* Look for a middle name */
    while (isspace(*s)) {	/* get rid of any extra space */
	len--;	
	s--;
    }
    mid = name;
    while (!isspace(*mid)) mid++;
    namelen = mid - name + 1;
    if (mid == s+1) {	/* no middle name */
	mid = 0;
	midlen = 0;
    } else {
	*mid++ = '\0';
	while (isspace(*mid)) {
	    len--;
	    mid++;
	}
	midlen = s - mid + 2;
	/* If first name is an initial and middle isn't and it all fits
	** without the first initial, drop it. */
	if (len > max && mid != s) {
	    if (len - namelen <= max
	     && ((mid[1] != '.' && (!name[1] || (name[1] == '.' && !name[2])))
	      || (*mid == '"' && *s == '"'))) {
		len -= namelen;
		name = mid;
		namelen = midlen;
		mid = 0;
	    }
	    else if (*mid == '"' && *s == '"') {
		if (midlen > max) {
		    name = mid+1;
		    *s = '\0';
		    goto try_again;
		}
		len = midlen;
		last = mid;
		namelen = 0;
		mid = 0;
	    }
	}
    }
    s[1] = '\0';
    if (mid && len > max) {
	/* Turn middle names into intials */
	len -= s - mid + 2;
	d = s = mid;
	while (*s) {
	    if (isalpha(*s)) {
		if (d != mid)
		    *d++ = ' ';
		*d++ = *s++;
	    }
	    while (*s && !isspace(*s)) s++;
	    while (isspace(*s)) s++;
	}
	if (d != mid) {
	    *d = '\0';
	    midlen = d - mid + 1;
	    len += midlen;
	} else
	    mid = 0;
    }
    if (len > max) {
	/* If the first name fits without the middle initials, drop them */
	if (mid && len - midlen <= max) {
	    len -= midlen;
	    mid = 0;
	} else if (namelen > 0) {
	    /* Turn the first name into an initial */
	    len -= namelen - 2;
	    name[1] = '\0';
	    namelen = 2;
	    if (len > max) {
		/* Dump the middle initials (if present) */
		if (mid) {
		    len -= midlen;
		    mid = 0;
		}
		if (len > max) {
		    /* Finally just truncate the last name */
		    last[max - 2] = '\0';
		}
	    }
	} else
	    namelen = 0;
    }

    /* Paste the names back together */
    d = name + namelen;
    if (namelen)
	d[-1] = ' ';
    if (mid) {
	if (d != mid)
	    strcpy(d, mid);
	d += midlen;
	d[-1] = ' ';
    }
    safecpy(d, last, max);	/* "max - (d-name)" would be overkill */
    return name;
}

/* Compress an email address, trying to keep as much of the local part of
** the addresses as possible.  The order of precence is @ ! %, but
** @ % ! may be better...
*/
char*
compress_address(name, max)
char* name;
int max;
{
    char* s;
    char* at;
    char* bang;
    char* hack;
    char* start;
    int len;

    /* Remove white space from both ends. */
    while (isspace(*name)) name++;
    if ((len = strlen(name)) == 0)
	return name;
    s = name + len - 1;
    while (isspace(*s)) s--;
    s[1] = '\0';
    if (*name == '<') {
	name++;
	if (*s == '>')
	    *s-- = '\0';
    }
    if ((len = s - name + 1) <= max)
	return name;

    at = bang = hack = NULL;
    for (s = name + 1; *s; s++) {
	/* If there's whitespace in the middle then it's probably not
	** really an email address. */
	if (isspace(*s)) {
	    name[max] = '\0';
	    return name;
	}
	switch (*s) {
	  case '@':
	    if (at == NULL) {
		at = s;
	    }
	    break;
	  case '!':
	    if (at == NULL) {
		bang = s;
		hack = NULL;
	    }
	    break;
	  case '%':
	    if (at == NULL && hack == NULL)
		hack = s;
	    break;
	}
    }
    if (at == NULL)
	at = name + len;

    if (hack != NULL) {
	if (bang != NULL) {
	    if (at - bang - 1 >= max)
		start = bang + 1;
	    else if (at - name >= max)
		start = at - max;
	    else
		start = name;
	} else
	    start = name;
    } else if (bang != NULL) {
	if (at - name >= max)
	    start = at - max;
	else
	    start = name;
    } else
	start = name;
    if (len - (start - name) > max)
	start[max] = '\0';
    return start;
}

/* Fit the author name in <max> chars.  Uses the comment portion if present
** and pads with spaces.
*/
char*
compress_from(from, size)
char* from;
int size;
{
    static char lbuf[LBUFLEN];
    char* s = from? from : nullstr;
    int len;

#ifdef CHARSUBST
    strcharsubst(lbuf, s, sizeof lbuf, *charsubst);
#else
    safecpy(lbuf, s, sizeof lbuf);
#endif
    if ((s = extract_name(lbuf)) != NULL)
	s = compress_name(s, size);
    else
	s = compress_address(lbuf, size);
    len = strlen(s);
    if (!len) {
	strcpy(s,"NO NAME");
	len = 7;
    }
    while (len < size) s[len++] = ' ';
    s[size] = '\0';
    return s;
}

/* Fit the date in <max> chars. */
char*
compress_date(ap, size)
ARTICLE* ap;
int size;
{
    char* s;
    char* t;

    strncpy(t = cmd_buf, ctime(&ap->date), size);
    if ((s = index(t, '\n')) != NULL)
	*s = '\0';
    t[size] = '\0';
    return t;
}

#define EQ(x,y) ((isupper(x) ? tolower(x) : (x)) == (y))

/* Parse the subject to look for any "Re[:^]"s at the start.
** Returns TRUE if a Re was found.  If strp is non-NULL, it
** will be set to the start of the interesting characters.
*/
bool
subject_has_Re(str, strp)
register char* str;
char** strp;
{
    bool has_Re = 0;

    while (*str && AT_GREY_SPACE(str)) str++;
    while (EQ(str[0], 'r') && EQ(str[1], 'e')) {	/* check for Re: */
      register char* cp = str + 2;
	if (*cp == '^') {				/* allow Re^2: */
	    while (*++cp <= '9' && *cp >= '0')
		;
	}
	if (*cp != ':')
	    break;
	while (*++cp && AT_GREY_SPACE(cp)) ;
	str = cp;
	has_Re = 1;
    }
    if (strp)
	*strp = str;
    return has_Re;
}

/* Output a subject in <max> chars.  Does intelligent trimming that tries to
** save the last two words on the line, excluding "(was: blah)" if needed.
*/
char*
compress_subj(ap, max)
ARTICLE* ap;
int max;
{
    register char* cp;
    register int len;
    ARTICLE* first;

    if (!ap)
	return "<MISSING>";

    /* Put a preceeding '>' on subjects that are replies to other articles */
    cp = buf;
    first = (ThreadedGroup? ap->subj->thread : ap->subj->articles);
    if (ap != first || (ap->flags & AF_HAS_RE)
     || (!(ap->flags&AF_UNREAD) ^ sel_rereading))
	*cp++ = '>';
#ifdef CHARSUBST
    strcharsubst(cp, ap->subj->str + 4, (sizeof buf) - (cp-buf), *charsubst);
#else
    safecpy(cp, ap->subj->str + 4, (sizeof buf) - (cp-buf));
#endif

    /* Remove "(was: oldsubject)", because we already know the old subjects.
    ** Also match "(Re: oldsubject)".  Allow possible spaces after the ('s.
    */
    for (cp = buf; (cp = index(cp+1, '(')) != NULL;) {
	while (*++cp == ' ') ;
	if (EQ(cp[0], 'w') && EQ(cp[1], 'a') && EQ(cp[2], 's')
	 && (cp[3] == ':' || cp[3] == ' ')) {
	    *--cp = '\0';
	    break;
	}
	if (EQ(cp[0], 'r') && EQ(cp[1], 'e')
	 && ((cp[2]==':' && cp[3]==' ') || (cp[2]=='^' && cp[4]==':'))) {
	    *--cp = '\0';
	    break;
	}
    }
    len = strlen(buf);
    if (!unbroken_subjects && len > max) {
	char* last_word;
	/* Try to include the last two words on the line while trimming */ 
	if ((last_word = rindex(buf, ' ')) != NULL) {
	    char* next_to_last;
	    *last_word = '\0';
	    if ((next_to_last = rindex(buf, ' ')) != NULL) {
		if (next_to_last-buf >= len - max + 3 + 10-1)
		    cp = next_to_last;
		else
		    cp = last_word;
	    } else
		cp = last_word;
	    *last_word = ' ';
	    if (cp-buf >= len - max + 3 + 10-1) {
		char* s = buf + max - (len-(cp-buf)+3);
		*s++ = '.'; *s++ = '.'; *s++ = '.';
		safecpy(s, cp + 1, max);
		len = max;
	    }
	}
    }
    if (len > max)
	buf[max] = '\0';
    return buf;
}

/* Modified version of a spinner originally found in Clifford Adams' strn. */

static char *spinchars;
static int spin_level INIT(0);	/* used to allow non-interfering nested spins */
static int spin_mode;
static int spin_place;		/* represents place in spinchars array */
static int spin_pos;		/* the last spinbar position we drew */
static ART_NUM spin_art;
static ART_POS spin_tell;

void
setspin(mode)
int mode;
{
    switch (mode) {
      case SPIN_FOREGROUND:
      case SPIN_BACKGROUND:
      case SPIN_BARGRAPH:
	if (!spin_level++) {
	    if ((spin_art = openart) != 0 && artfp)
		spin_tell = tellart();
	    spin_count = 0;
	    spin_place = 0;
	}
	if (spin_mode == SPIN_BARGRAPH)
	    mode = SPIN_BARGRAPH;
	if (mode == SPIN_BARGRAPH) {
	    if (spin_mode != SPIN_BARGRAPH) {
		int i;
#ifdef VERBOSE
		spin_marks = (verbose? 25 : 10);
#else
		spin_marks = 25;
#endif
		printf(" [%*s]", spin_marks, nullstr);
		for (i = spin_marks + 1; i--; ) backspace();
		fflush(stdout);
	    }
	    spin_pos = 0;
	}
	spinchars = "|/-\\";
	spin_mode = mode;
	break;
      case SPIN_POP:
      case SPIN_OFF:
	if (spin_mode == SPIN_BARGRAPH) {
	    spin_level = 1;
	    spin(10000);
	    if (spin_count >= spin_todo)
		spin_char = ']';
	    spin_count--;
	    spin_mode = SPIN_FOREGROUND;
	}
	if (mode == SPIN_POP && --spin_level > 0)
	    break;
	spin_level = 0;
	if (spin_place) {	/* we have spun at least once */
	    putchar(spin_char); /* get rid of spin character */
	    backspace();
	    fflush(stdout);
	    spin_place = 0;
	}
	if (spin_art) {
	    artopen(spin_art,spin_tell);   /* do not screw up the pager */
	    spin_art = 0;
	}
	spin_mode = SPIN_OFF;
	spin_char = ' ';
	break;
    }
}

void
spin(count)
int count;		/* modulus for the spin... */
{
    if (!spin_level)
	return;
    switch (spin_mode) {
      case SPIN_BACKGROUND:
	if (!bkgnd_spinner)
	    return;
	if (!(++spin_count % count)) {
	    putchar(spinchars[++spin_place % 4]);
	    backspace();
	    fflush(stdout);
#ifdef USE_TK
	    if (ttk_running)
		ttk_do_waiting_events();
#endif
	}
	break;
      case SPIN_FOREGROUND:
	if (!(++spin_count % count)) {
	    putchar('.');
	    fflush(stdout);
#ifdef USE_TK
	    if (ttk_running)
		ttk_do_waiting_events();
#endif
	}
	break;
      case SPIN_BARGRAPH: {
	int new_pos;

	if (spin_todo == 0)
	    break;		/* bail out rather than crash */
	new_pos = (int)((long)spin_marks * ++spin_count / spin_todo);
	if (spin_pos < new_pos && spin_count <= spin_todo+1) {
	    do {
		putchar('*');
	    } while (++spin_pos < new_pos);
	    spin_place = 0;
	    fflush(stdout);
#ifdef USE_TK
	    if (ttk_running)
		ttk_do_waiting_events();
#endif
	}
	else if (!(spin_count % count)) {
	    putchar(spinchars[++spin_place % 4]);
	    backspace();
	    fflush(stdout);
#ifdef USE_TK
	    if (ttk_running)
		ttk_do_waiting_events();
#endif
	}
	break;
      }
    }
}

bool
inbackground()
{
    return spin_mode == SPIN_BACKGROUND;
}

static int	prior_perform_cnt;
static time_t	prior_now;
static long	ps_sel;
static long	ps_cnt;
static long	ps_missing;

void
perform_status_init(cnt)
long cnt;
{
    perform_cnt = 0;
    error_occurred = FALSE;
    subjline = NULL;
    page_line = 1;
    performed_article_loop = TRUE;

    prior_perform_cnt = 0;
    prior_now = 0;
    ps_sel = selected_count;
    ps_cnt = cnt;
    ps_missing = missing_count;

    spin_count = 0;
    spin_place = 0;
    spinchars = "v>^<";
}

void
perform_status(cnt, spin)
long cnt;
int spin;
{
    long kills, sels, missing;
    time_t now;

    if (!(++spin_count % spin)) {
	putchar(spinchars[++spin_place % 4]);
	backspace();
	fflush(stdout);
    }

    if (perform_cnt == prior_perform_cnt)
	return;

    now = time((time_t*)NULL);
    if (now - prior_now < 2)
	return;

    prior_now = now;
    prior_perform_cnt = perform_cnt;

    missing = missing_count - ps_missing;
    kills = ps_cnt - cnt - missing;
    sels = selected_count - ps_sel;

    if (!(kills | sels))
	return;

    carriage_return();
    if (perform_cnt != sels  && perform_cnt != -sels
     && perform_cnt != kills && perform_cnt != -kills)
	printf("M:%d ", perform_cnt);
    if (kills)
	printf("K:%ld ", kills);
    if (sels)
	printf("S:%ld ", sels);
#if 0
    if (missing > 0)
	printf("(M: %ld) ", missing);
#endif
    erase_eol();
    fflush(stdout);
}

static char*
output_change(cp, num, obj_type, modifier, action)
char* cp;
long num;
char* obj_type;
char* modifier;
char* action;
{
    bool neg;
    char* s;

    if (num < 0) {
	num *= -1;
	neg = 1;
    }
    else
	neg = 0;

    if (cp != msg) {
	*cp++ = ',';
	*cp++ = ' ';
    }
    sprintf(cp, "%ld ", num);
    if (obj_type)
	sprintf(cp+=strlen(cp), "%s%s ", obj_type, PLURAL(num));
    cp += strlen(cp);
    if ((s = modifier) != NULL) {
	*cp++ = ' ';
	if (num != 1)
	    while (*s++ != '|') ;
	while (*s && *s != '|') *cp++ = *s++;
	*cp++ = ' ';
    }
    s = action;
    if (!neg)
	while (*s++ != '|') ;
    while (*s && *s != '|') *cp++ = *s++;
    s++;
    if (neg)
	while (*s++ != '|') ;
    while (*s) *cp++ = *s++;

    *cp = '\0';
    return cp;
}

int
perform_status_end(cnt, obj_type)
long cnt;
char* obj_type;
{
    long kills, sels, missing;
    char* cp = msg;
    bool article_status = (*obj_type == 'a');

    if (perform_cnt == 0) {
	sprintf(msg, "No %ss affected.", obj_type);
	return 0;
    }

    missing = missing_count - ps_missing;
    kills = ps_cnt - cnt - missing;
    sels = selected_count - ps_sel;

    if (!performed_article_loop)
	cp = output_change(cp, (long)perform_cnt,
			   sel_mode == SM_THREAD? "thread" : "subject",
			   (char*)NULL, "ERR|match|ed");
    else if (perform_cnt != sels  && perform_cnt != -sels
	  && perform_cnt != kills && perform_cnt != -kills) {
	cp = output_change(cp, (long)perform_cnt, obj_type, (char*)NULL,
			   "ERR|match|ed");
	obj_type = NULL;
    }
    if (kills) {
	cp = output_change(cp, kills, obj_type, (char*)NULL,
			   article_status? "un||killed" : "more|less|");
	obj_type = NULL;
    }
    if (sels) {
	cp = output_change(cp, sels, obj_type, (char*)NULL, "de||selected");
	obj_type = NULL;
    }
    if (article_status && missing > 0) {
	*cp++ = '(';
	cp = output_change(cp, missing, obj_type, "was|were", "ERR|missing|");
	*cp++ = ')';
    }

    strcpy(cp, ".");

    /* If we only selected/deselected things, return 1, else 2 */
    return (kills | missing) == 0? 1 : 2;
}
