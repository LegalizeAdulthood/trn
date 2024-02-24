/* rt-wumpus.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "rt-wumpus.h"

#include "artio.h"
#include "artstate.h"
#include "backpage.h"
#include "cache.h"
#include "charsubst.h"
#include "color.h"
#include "head.h"
#include "ng.h"
#include "ngdata.h"
#include "rt-select.h"
#include "rthread.h"
#include "string-algos.h"
#include "terminal.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

int g_max_tree_lines{6};

char g_letters[] = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+";

// clang-format off
static char s_tree_indent[] = {
    ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0,
    ' ', ' ', ' ', ' ', 0,   ' ', ' ', ' ', ' ', 0
};
// clang-format on
static ARTICLE *s_tree_article{};
static int      s_max_depth{};
static int      s_max_line{-1};
static int      s_first_depth{};
static int      s_first_line{};
static int      s_my_depth{};
static int      s_my_line{};
static bool     s_node_on_line{};
static int      s_node_line_cnt{};
static int      s_line_num{};
static int      s_header_indent{};
static char    *s_tree_lines[11]{};
static char     s_tree_buff[128]{};
static char    *s_str{};

static void find_depth(ARTICLE *article, int depth);
static void cache_tree(ARTICLE *ap, int depth, char *cp);
static ARTICLE *find_artp(ARTICLE *article, int x);
static void display_tree(ARTICLE *article, char *cp);

/* Prepare tree display for inclusion in the article header.
*/
void init_tree()
{
    ARTICLE*thread;

    while (s_max_line >= 0)		/* free any previous tree data */
	free(s_tree_lines[s_max_line--]);

    if (!(s_tree_article = g_curr_artp) || !s_tree_article->subj)
	return;
    if (!(thread = s_tree_article->subj->thread))
	return;
    /* Enumerate our subjects for display */
    SUBJECT *sp = thread->subj;
    int      num = 0;
    do {
	sp->misc = num++;
	sp = sp->thread_link;
    } while (sp != thread->subj);

    s_max_depth = 0;
    s_max_line = 0;
    s_my_depth = 0;
    s_my_line = 0;
    s_node_line_cnt = 0;
    find_depth(thread, 0);

    if (s_max_depth <= 5)
	s_first_depth = 0;
    else {
	if (s_my_depth+2 > s_max_depth)
	    s_first_depth = s_max_depth - 5;
	else
	{
            s_first_depth = s_my_depth - 3;
            if (s_first_depth < 0)
	        s_first_depth = 0;
	}
	s_max_depth = s_first_depth + 5;
    }
    if (--s_max_line < g_max_tree_lines)
	s_first_line = 0;
    else {
	if (s_my_line + g_max_tree_lines/2 > s_max_line)
	    s_first_line = s_max_line - (g_max_tree_lines-1);
	else
	{
            s_first_line = s_my_line - (g_max_tree_lines - 1) / 2;
            if (s_first_line < 0)
	        s_first_line = 0;
	}
	s_max_line = s_first_line + g_max_tree_lines-1;
    }

    s_str = s_tree_buff;		/* initialize first line's data */
    *s_str++ = ' ';
    s_node_on_line = false;
    s_line_num = 0;
    /* cache our portion of the tree */
    cache_tree(thread, 0, s_tree_indent);

    s_max_depth = (s_max_depth-s_first_depth+1) * 5;	/* turn depth into char width */
    s_max_line -= s_first_line;			/* turn s_max_line into count */
    /* shorten tree if lower lines aren't visible */
    if (s_node_line_cnt < s_max_line)
	s_max_line = s_node_line_cnt + 1;
}

/* A recursive routine to find the maximum tree extents and where we are.
*/
static void find_depth(ARTICLE *article, int depth)
{
    if (depth > s_max_depth)
	s_max_depth = depth;
    for (;;) {
	if (article == s_tree_article) {
	    s_my_depth = depth;
	    s_my_line = s_max_line;
	}
	if (article->child1)
	    find_depth(article->child1, depth+1);
	else
	    s_max_line++;
	if (!(article = article->sibling))
	    break;
    }
}

/* Place the tree display in a maximum of 11 lines x 6 nodes.
*/
static void cache_tree(ARTICLE *ap, int depth, char *cp)
{
    int depth_mode;

    cp[1] = ' ';
    if (depth >= s_first_depth && depth <= s_max_depth) {
	cp += 5;
	depth_mode = 1;
    } else if (depth+1 == s_first_depth)
	depth_mode = 2;
    else {
	cp = s_tree_indent;
	depth_mode = 0;
    }
    for (;;) {
	switch (depth_mode) {
	case 1: {
	    char ch;

	    *s_str++ = ((ap->flags & AF_HAS_RE) || ap->parent) ? '-' : ' ';
	    if (ap == s_tree_article)
		*s_str++ = '*';
	    if (!(ap->flags & AF_UNREAD)) {
		*s_str++ = '(';
		ch = ')';
	    } else if (!g_selected_only || (ap->flags & AF_SEL)) {
		*s_str++ = '[';
		ch = ']';
	    } else {
		*s_str++ = '<';
		ch = '>';
	    }
	    if (ap == g_recent_artp && ap != s_tree_article)
		*s_str++ = '@';
	    *s_str++ = thread_letter(ap);
	    *s_str++ = ch;
	    if (ap->child1)
		*s_str++ = (ap->child1->sibling? '+' : '-');
	    if (ap->sibling)
		*cp = '|';
	    else
		*cp = ' ';
	    s_node_on_line = true;
	    break;
	}
	case 2:
	    *s_tree_buff = (!ap->child1)? ' ' :
		(ap->child1->sibling)? '+' : '-';
	    break;
	default:
	    break;
	}
	if (ap->child1) {
	    cache_tree(ap->child1, depth+1, cp);
	    cp[1] = '\0';
	} else {
	    if (!s_node_on_line && s_first_line == s_line_num)
		s_first_line++;
	    if (s_line_num >= s_first_line) {
		if (s_str[-1] == ' ')
		    s_str--;
		*s_str = '\0';
		s_tree_lines[s_line_num-s_first_line]
			= safemalloc(s_str-s_tree_buff + 1);
		strcpy(s_tree_lines[s_line_num - s_first_line], s_tree_buff);
		if (s_node_on_line) {
		    s_node_line_cnt = s_line_num - s_first_line;
		}
	    }
	    s_line_num++;
	    s_node_on_line = false;
	}
	if (!(ap = ap->sibling) || s_line_num > s_max_line)
	    break;
	if (!ap->sibling)
	    *cp = '\\';
	if (!s_first_depth)
	    s_tree_indent[5] = ' ';
	strcpy(s_tree_buff, s_tree_indent+5);
	s_str = s_tree_buff + strlen(s_tree_buff);
    }
}

static int s_find_artp_y{};

ARTICLE *get_tree_artp(int x, int y)
{
    if (!s_tree_article || !s_tree_article->subj)
	return nullptr;
    ARTICLE *ap = s_tree_article->subj->thread;
    x -= g_tc_COLS-1 - s_max_depth;
    if (x < 0 || y > s_max_line || !ap)
	return nullptr;
    x = (x-(x==s_max_depth))/5 + s_first_depth;
    s_find_artp_y = y + s_first_line;
    ap = find_artp(ap, x);
    return ap;
}

/* A recursive routine to find the maximum tree extents and where we are.
*/
static ARTICLE *find_artp(ARTICLE *article, int x)
{
    for (;;) {
	if (!x && !s_find_artp_y)
	    return article;
	if (article->child1) {
	    ARTICLE* ap = find_artp(article->child1, x-1);
	    if (ap)
		return ap;
	}
	else if (!s_find_artp_y--)
	    return nullptr;
	if (!(article = article->sibling))
	    break;
    }
    return nullptr;
}

inline bool header_conv()
{
    return g_charsubst[0] == 'a' || g_charsubst[0] == 'm';
}

/* Output a header line with possible tree display on the right hand side.
** Does automatic wrapping of lines that are too long.
*/
int tree_puts(char *orig_line, ART_LINE header_line, int is_subject)
{
    char*    tmpbuf;
    char*    line;
    char*    end;
    int      wrap_at;
    ART_LINE start_line = header_line;
    int      i, len;
    char     ch;

    /* Make a modifiable copy of the line */
    char *cp = strchr(orig_line, '\n');
    if (cp != nullptr)
	len = cp - orig_line;
    else
	len = strlen(orig_line);

    /* Copy line, filtering encoded and control characters. */
    if (header_conv()) {
	tmpbuf = safemalloc(len * 2 + 2);
	line = tmpbuf + len + 1;
    }
    else
    {
        tmpbuf = safemalloc(len + 2); /* yes, I mean "2" */
        line = tmpbuf;
    }
    if (g_do_hiding)
	end = line + decode_header(line, orig_line, len);
    else {
	safecpy(line, orig_line, len+1);
	dectrl(line);
	end = line + len;
    }
    if (header_conv()) {
	end = tmpbuf + strcharsubst(tmpbuf, line, len*2+2, *g_charsubst);
	line = tmpbuf;
    }

    if (!*line) {
	strcpy(line, " ");
	end = line+1;
    }

    color_object(COLOR_HEADER, true);
    /* If this is the first subject line, output it with a preceeding [1] */
    if (is_subject && !isspace(*line)) {
	if (g_threaded_group) {
	    color_object(COLOR_TREE_MARK, true);
	    putchar('[');
	    putchar(thread_letter(g_curr_artp));
	    putchar(']');
	    color_pop();
	    putchar(' ');
	    s_header_indent = 4;
	}
	else {
	    fputs("Subject: ", stdout);
	    s_header_indent = 9;
	}
	i = 0;
    } else {
	if (*line != ' ') {
	    /* A "normal" header line -- output keyword and set s_header_indent
	    ** _except_ for the first line, which is a non-standard header.
	    */
	    if (!header_line || !(cp = strchr(line, ':')) || *++cp != ' ')
		s_header_indent = 0;
	    else {
		*cp = '\0';
		fputs(line, stdout);
		putchar(' ');
		s_header_indent = ++cp - line;
		line = cp;
		if (!*line)
		    *--line = ' ';
	    }
	    i = 0;
	} else {
	    /* Skip whitespace of continuation lines and prepare to indent */
	    line = skip_eq(++line, ' ');
	    i = s_header_indent;
	}
    }
    for ( ; *line; i = s_header_indent) {
	maybe_eol();
	if (i) {
	    putchar('+');
	    while (--i)
		putchar(' ');
	}
	g_term_col = s_header_indent;
	/* If no (more) tree lines, wrap at g_tc_COLS-1 */
	if (s_max_line < 0 || header_line > s_max_line+1)
	    wrap_at = g_tc_COLS-1;
	else
	    wrap_at = g_tc_COLS - s_max_depth - 3;
	/* Figure padding between header and tree output, wrapping long lines */
	int pad_cnt = wrap_at - (end - line + s_header_indent);
	if (pad_cnt <= 0) {
	    cp = line + (int)(wrap_at - s_header_indent - 1);
	    pad_cnt = 1;
	    while (cp > line && *cp != ' ') {
		if (*--cp == ',' || *cp == '.' || *cp == '-' || *cp == '!') {
		    cp++;
		    break;
		}
		pad_cnt++;
	    }
	    if (cp == line) {
		cp += wrap_at - s_header_indent;
		pad_cnt = 0;
	    }
	    ch = *cp;
	    *cp = '\0';
	    /* keep rn's backpager happy */
	    vwtary(g_artline, vrdary(g_artline - 1));
	    g_artline++;
	} else {
	    cp = end;
	    ch = '\0';
	}
	if (is_subject)
	    color_string(COLOR_SUBJECT, line);
	else if (s_header_indent == 0 && *line != '+')
	    color_string(COLOR_ARTLINE1, line);
	else
	    fputs(line, stdout);
	*cp = ch;
	/* Skip whitespace in wrapped line */
	while (*cp == ' ')
	    cp++;
	line = cp;
	/* Check if we've got any tree lines to output */
	if (wrap_at != g_tc_COLS-1 && header_line <= s_max_line) {
            do {
		putchar(' ');
	    } while (pad_cnt--);
	    g_term_col = wrap_at;
	    /* Check string for the '*' flagging our current node
	    ** and the '@' flagging our prior node.
	    */
	    cp = s_tree_lines[header_line];
	    char *cp1 = strchr(cp, '*');
	    char *cp2 = strchr(cp, '@');
	    if (cp1 != nullptr)
		*cp1 = '\0';
	    if (cp2 != nullptr)
		*cp2 = '\0';
	    color_object(COLOR_TREE, true);
	    fputs(cp, stdout);
	    /* Handle standout output for '*' and '@' marked nodes, then
	    ** continue with the rest of the line.
	    */
	    while (cp1 || cp2) {
		color_object(COLOR_TREE_MARK, true);
		if (cp1 && (!cp2 || cp1 < cp2)) {
		    cp = cp1;
		    cp1 = nullptr;
		    *cp++ = '*';
		    putchar(*cp++);
		    putchar(*cp++);
		} else {
		    cp = cp2;
		    cp2 = nullptr;
		    *cp++ = '@';
		}
		putchar(*cp++);
		color_pop();	/* of COLOR_TREE_MARK */
		if (*cp)
		    fputs(cp, stdout);
	    }/* while */
	    color_pop();	/* of COLOR_TREE */
	}/* if */
	newline();
	header_line++;
    }/* for remainder of line */

    /* free allocated copy of line */
    free(tmpbuf);

    color_pop();	/* of COLOR_HEADER */
    /* return number of lines displayed */
    return header_line - start_line;
}

/* Output any parts of the tree that are left to display.  Called at the
** end of each header.
*/
int  finish_tree(ART_LINE last_line)
{
    ART_LINE start_line = last_line;

    while (last_line <= s_max_line) {
	g_artline++;
	last_line += tree_puts("+", last_line, 0);
	vwtary(g_artline, g_artpos);	/* keep rn's backpager happy */
    }
    return last_line - start_line;
}

/* Output the entire article tree for the user.
*/
void entire_tree(ARTICLE* ap)
{
    if (!ap) {
	if (g_verbose)
	    fputs("\nNo article tree to display.\n", stdout) FLUSH;
	else
	    fputs("\nNo tree.\n", stdout) FLUSH;
	termdown(2);
	return;
    }

    if (!g_threaded_group) {
	g_threaded_group = true;
	printf("Threading the group. ");
	fflush(stdout);
	thread_open();
	if (!g_threaded_group) {
	    printf("*failed*\n") FLUSH;
	    termdown(1);
	    return;
	}
	count_subjects(CS_NORM);
	newline();
    }
    if (!(ap->flags & AF_THREADED))
	parseheader(article_num(ap));
    if (check_page_line())
	return;
    newline();
    ARTICLE *thread = ap->subj->thread;
    /* Enumerate our subjects for display */
    SUBJECT *sp = thread->subj;
    int      num = 0;
    do {
	if (check_page_line())
	    return;
	printf("[%c] %s\n",g_letters[num>9+26+26? 9+26+26:num],sp->str+4) FLUSH;
	termdown(1);
	sp->misc = num++;
	sp = sp->thread_link;
    } while (sp != thread->subj);
    if (check_page_line())
	return;
    newline();
    if (check_page_line())
	return;
    putchar(' ');
    g_buf[3] = '\0';
    display_tree(thread, s_tree_indent);

    if (check_page_line())
	return;
    newline();
}

/* A recursive routine to output the entire article tree.
*/
static void display_tree(ARTICLE *article, char *cp)
{
    if (cp - s_tree_indent > g_tc_COLS || g_page_line < 0)
	return;
    cp[1] = ' ';
    cp += 5;
    color_object(COLOR_TREE, true);
    for (;;) {
	putchar(((article->flags&AF_HAS_RE) || article->parent) ? '-' : ' ');
	if (!(article->flags & AF_UNREAD)) {
	    g_buf[0] = '(';
	    g_buf[2] = ')';
	} else if (!g_selected_only || (article->flags & AF_SEL)) {
	    g_buf[0] = '[';
	    g_buf[2] = ']';
	} else {
	    g_buf[0] = '<';
	    g_buf[2] = '>';
	}
	g_buf[1] = thread_letter(article);
	if (article == g_curr_artp) {
	    color_string(COLOR_TREE_MARK,g_buf);
	} else if (article == g_recent_artp) {
	    putchar(g_buf[0]);
	    color_object(COLOR_TREE_MARK, true);
	    putchar(g_buf[1]);
	    color_pop();	/* of COLOR_TREE_MARK */
	    putchar(g_buf[2]);
	} else
	    fputs(g_buf, stdout);

	if (article->sibling) {
	    *cp = '|';
	} else
	    *cp = ' ';
	if (article->child1) {
	    putchar((article->child1->sibling)? '+' : '-');
	    color_pop();	/* of COLOR_TREE */
	    display_tree(article->child1, cp);
	    color_object(COLOR_TREE, true);
	    cp[1] = '\0';
	} else
	    newline();
	if (!(article = article->sibling))
	    break;
	if (!article->sibling)
	    *cp = '\\';
	s_tree_indent[5] = ' ';
	if (check_page_line()) {
	    color_pop();
	    return;
	}
	fputs(s_tree_indent+5, stdout);
    }
    color_pop();	/* of COLOR_TREE */
}

/* Calculate the subject letter representation.  "Place-holder" nodes
** are marked with a ' ', others get a letter in the sequence:
**	' ', '1'-'9', 'A'-'Z', 'a'-'z', '+'
*/
char thread_letter(ARTICLE *ap)
{
    int subj = ap->subj->misc;

    if (!(ap->flags & AF_CACHED)
     && (g_absfirst < g_first_cached || g_last_cached < g_lastart
      || !g_cached_all_in_range))
	return '?';
    if (!(ap->flags & AF_EXISTS))
	return ' ';
    return g_letters[subj > 9+26+26 ? 9+26+26 : subj];
}
