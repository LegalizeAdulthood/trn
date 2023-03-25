/* artio.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "common.h"
#include "artio.h"

#include "art.h"
#include "artstate.h"
#include "cache.h"
#include "color.h"
#include "datasrc.h"
#include "decode.h"
#include "head.h"
#include "list.h"
#include "mime.h"
#include "ngdata.h"
#include "nntp.h"
#include "term.h"
#include "util.h"
#include "util2.h"

ART_POS g_artpos{};   /* byte position in article file */
ART_LINE g_artline{}; /* current line number in article file */
FILE *g_artfp{};      /* current article file pointer */
ART_NUM g_openart{};  /* the article number we have open */
char *g_artbuf{};
long g_artbuf_size{};
long g_artbuf_pos{};
long g_artbuf_seek{};
long g_artbuf_len{};
char g_wrapped_nl{WRAPPED_NL};

void artio_init()
{
    g_artbuf_size = 8 * 1024;
    g_artbuf = safemalloc(g_artbuf_size);
    clear_artbuf();
}

/* open an article, unless it's already open */

FILE *artopen(ART_NUM artnum, ART_NUM pos)
{
    char artname[MAXFILENAME];		/* filename of current article */
    ARTICLE* ap = article_find(artnum);

    if (!ap || !artnum || (ap->flags & (AF_EXISTS|AF_FAKE)) != AF_EXISTS) {
	errno = ENOENT;
	return nullptr;
    }
    if (g_openart == artnum) {		/* this article is already open? */
	seekart(pos);			/* yes: just seek the file */
	return g_artfp;			/* and say we succeeded */
    }
    artclose();
retry_open:
    if (g_datasrc->flags & DF_REMOTE)
	nntp_body(artnum);
    else
    {
	sprintf(artname,"%ld",(long)artnum);
	g_artfp = fopen(artname,"r");
	/*artio_setbuf(g_artfp);$$*/
    }
    if (!g_artfp) {
#ifdef ETIMEDOUT
	if (errno == ETIMEDOUT)
	    goto retry_open;
#endif
	if (errno == EINTR)
	    goto retry_open;
	uncache_article(ap,false);
    } else {
	g_openart = artnum;		/* remember what we did here */
	seekart(pos);
    }
    return g_artfp;			/* and return either fp or nullptr */
}

void artclose()
{
    if (g_artfp != nullptr) {		/* article still open? */
	if (g_datasrc->flags & DF_REMOTE)
	    nntp_finishbody(FB_DISCARD);
	fclose(g_artfp);			/* close it */
	g_artfp = nullptr;			/* and tell the world */
	g_openart = 0;
	clear_artbuf();
    }
}

int seekart(ART_POS pos)
{
    if (g_datasrc->flags & DF_REMOTE)
	return nntp_seekart(pos);
    return fseek(g_artfp,(long)pos,0);
}

ART_POS
tellart()
{
    if (g_datasrc->flags & DF_REMOTE)
	return nntp_tellart();
    return (ART_POS)ftell(g_artfp);
}

char *readart(char *s, int limit)
{
    if (g_datasrc->flags & DF_REMOTE)
	return nntp_readart(s,limit);
    return fgets(s,limit,g_artfp);
}

void clear_artbuf()
{
    *g_artbuf = '\0';
    g_artbuf_pos = g_artbuf_seek = g_artbuf_len = 0;
}

int seekartbuf(ART_POS pos)
{
    if (!g_do_hiding)
	return seekart(pos);

    pos -= g_htype[PAST_HEADER].minpos;
    g_artbuf_pos = g_artbuf_len;

    while (g_artbuf_pos < pos) {
	if (!readartbuf(false))
	    return -1;
    }

    g_artbuf_pos = pos;

    return 0;
}

char *readartbuf(bool view_inline)
{
    char* bp;
    char* s;
    int read_offset, line_offset, filter_offset, extra_offset, len, o;
    int word_wrap, extra_chars = 0;
    int read_something = 0;

    if (!g_do_hiding) {
	bp = readart(g_art_line,(sizeof g_art_line)-1);
	g_artbuf_pos = g_artbuf_seek = tellart() - g_htype[PAST_HEADER].minpos;
	return bp;
    }
    if (g_artbuf_pos == g_artsize - g_htype[PAST_HEADER].minpos)
	return nullptr;
    bp = g_artbuf + g_artbuf_pos;
    if (*bp == '\001' || *bp == '\002') {
	bp++;
	g_artbuf_pos++;
    }
    if (*bp) {
	for (s = bp; *s && !AT_NL(*s); s++) ;
	if (*s) {
	    len = s - bp + 1;
	    goto done;
	}
	read_offset = line_offset = filter_offset = s - bp;
    }
    else
	read_offset = line_offset = filter_offset = 0;

  read_more:
    extra_offset = g_mime_state == HTMLTEXT_MIME? 1024 : 0;
    o = read_offset + extra_offset;
    if (g_artbuf_size < g_artbuf_pos + o + LBUFLEN) {
	g_artbuf_size += LBUFLEN * 4;
	g_artbuf = saferealloc(g_artbuf,g_artbuf_size);
	bp = g_artbuf + g_artbuf_pos;
    }
    switch (g_mime_state) {
      case IMAGE_MIME:
      case AUDIO_MIME:
	break;
      default:
	read_something = 1;
	/* The -1 leaves room for appending a newline, if needed */
	if (!readart(bp+o, g_artbuf_size-g_artbuf_pos-o-1)) {
	    if (!read_offset) {
		*bp = '\0';
		len = 0;
		bp = nullptr;
		goto done;
	    }
	    strcpy(bp+o, "\n");
	    read_something = -1;
	}
	len = strlen(bp+o) + read_offset;
	if (bp[len+extra_offset-1] != '\n') {
	    if (read_something >= 0) {
		read_offset = len;
		goto read_more;
	    }
	    strcpy(bp + len++ + extra_offset, "\n");
	}
	if (!g_is_mime)
	    goto done;
	o = line_offset + extra_offset;
	mime_SetState(bp+o);
	if (bp[o] == '\0') {
	    strcpy(bp+o, "\n");
	    len = line_offset+1;
	}
	break;
    }
  mime_switch:
    switch (g_mime_state) {
      case ISOTEXT_MIME:
	g_mime_state = TEXT_MIME;
	/* FALL THROUGH */
      case TEXT_MIME:
      case HTMLTEXT_MIME:
	if (g_mime_section->encoding == MENCODE_QPRINT) {
	    o = line_offset + extra_offset;
	    len = qp_decodestring(bp+o, bp+o, false) + line_offset;
	    if (len == line_offset || bp[len+extra_offset-1] != '\n') {
		if (read_something >= 0) {
		    read_offset = line_offset = len;
		    goto read_more;
		}
		strcpy(bp + len++ + extra_offset, "\n");
	    }
	}
	else if (g_mime_section->encoding == MENCODE_BASE64) {
	    o = line_offset + extra_offset;
	    len = b64_decodestring(bp+o, bp+o) + line_offset;
            s = strchr(bp + o, '\n');
            if (s == nullptr)
            {
		if (read_something >= 0) {
		    read_offset = line_offset = len;
		    goto read_more;
		}
		strcpy(bp + len++ + extra_offset, "\n");
	    }
	    else {
		extra_chars += len;
		len = s - bp - extra_offset + 1;
		extra_chars -= len;
	    }
	}
	if (g_mime_state != HTMLTEXT_MIME)
	    break;
	o = filter_offset + extra_offset;
	len = filter_html(bp+filter_offset, bp+o) + filter_offset;
	if (len == filter_offset || (s = strchr(bp,'\n')) == nullptr) {
	    if (read_something >= 0) {
		read_offset = line_offset = filter_offset = len;
		goto read_more;
	    }
	    strcpy(bp + len++, "\n");
	    extra_chars = 0;
	}
	else {
	    extra_chars = len;
	    len = s - bp + 1;
	    extra_chars -= len;
	}
	break;
      case DECODE_MIME: {
	MIMECAP_ENTRY* mcp;
	mcp = mime_FindMimecapEntry(g_mime_section->type_name,
                                    MCF_NEEDSTERMINAL |MCF_COPIOUSOUTPUT);
	if (mcp) {
	    int save_term_line = g_term_line;
	    g_nowait_fork = true;
	    color_object(COLOR_MIMEDESC, true);
	    if (decode_piece(mcp,bp)) {
		strcpy(bp = g_artbuf + g_artbuf_pos, g_art_line);
		mime_SetState(bp);
		if (g_mime_state == DECODE_MIME)
		    g_mime_state = SKIP_MIME;
	    }
	    else
		g_mime_state = SKIP_MIME;
	    color_pop();
	    chdir_newsdir();
	    erase_line(false);
	    g_nowait_fork = false;
	    g_first_view = g_artline;
	    g_term_line = save_term_line;
	    if (g_mime_state != SKIP_MIME)
		goto mime_switch;
	}
	/* FALL THROUGH */
      }
      case SKIP_MIME: {
	MIME_SECT* mp = g_mime_section;
	while ((mp = mp->prev) != nullptr && !mp->boundary_len) ;
	if (!mp) {
	    g_artbuf_len = g_artbuf_pos;
	    g_artsize = g_artbuf_len + g_htype[PAST_HEADER].minpos;
	    read_something = 0;
	    bp = nullptr;
	}
	else if (read_something >= 0) {
	    *bp = '\0';
	    read_offset = line_offset = filter_offset = 0;
	    goto read_more;
	}
	else
	    *bp = '\0';
	len = 0;
	break;
      }
    case END_OF_MIME:
	if (g_mime_section->prev)
	    g_mime_state = SKIP_MIME;
	else {
	    if (g_datasrc->flags & DF_REMOTE) {
		nntp_finishbody(FB_SILENT);
		g_raw_artsize = nntp_artsize();
	    }
	    seekart(g_raw_artsize);
	}
	/* FALL THROUGH */
      case BETWEEN_MIME:
	len = strlen(g_multipart_separator) + 1;
	if (extra_offset && filter_offset) {
	    extra_chars = len + 1;
	    len = o = read_offset + 1;
	    bp[o-1] = '\n';
	}
	else {
	    o = -1;
	    g_artbuf_pos++;
	    bp++;
	}
	sprintf(bp+o,"\002%s\n",g_multipart_separator);
	break;
      case UNHANDLED_MIME:
	g_mime_state = SKIP_MIME;
	*bp++ = '\001';
	g_artbuf_pos++;
	mime_Description(g_mime_section,bp,g_tc_COLS);
	len = strlen(bp);
	break;
      case ALTERNATE_MIME:
	g_mime_state = SKIP_MIME;
	*bp++ = '\001';
	g_artbuf_pos++;
	sprintf(bp,"[Alternative: %s]\n", g_mime_section->type_name);
	len = strlen(bp);
	break;
      case IMAGE_MIME:
      case AUDIO_MIME:
	if (!g_mime_article.total && !g_multimedia_mime)
	    g_multimedia_mime = true;
	/* FALL THROUGH */
      default:
	if (view_inline && g_first_view < g_artline
	 && (g_mime_section->flags & MSF_INLINE))
	    g_mime_state = DECODE_MIME;
	else
	    g_mime_state = SKIP_MIME;
	*bp++ = '\001';
	g_artbuf_pos++;
	mime_Description(g_mime_section,bp,g_tc_COLS);
	len = strlen(bp);
	break;
    }

  done:
    word_wrap = g_tc_COLS - g_word_wrap_offset;
    if (read_something && g_word_wrap_offset >= 0 && word_wrap > 20 && bp) {
	char* cp;
	for (cp = bp; *cp && (s = strchr(cp, '\n')) != nullptr; cp = s+1) {
	    if (s - cp > g_tc_COLS) {
		char* t;
		do {
		    for (t = cp+word_wrap; *t!=' ' && *t!='\t' && t > cp; t--) ;
		    if (t == cp) {
			for (t = cp+word_wrap; *t!=' ' && *t!='\t' && t<=cp+g_tc_COLS; t++) ;
			if (t > cp+g_tc_COLS) {
			    t = cp + g_tc_COLS - 1;
			    continue;
			}
		    }
		    if (cp == bp) {
			extra_chars += len;
			len = t - bp + 1;
			extra_chars -= len;
		    }
		    *t = g_wrapped_nl;
		    if (t[1] == ' ' || t[1] == '\t') {
			int spaces = 1;
			for (t++; *++t == ' ' || *t == '\t'; spaces++) ;
			safecpy(t-spaces,t,extra_chars);
			extra_chars -= spaces;
			t -= spaces + 1;
		    }
		} while (s - (cp = t+1) > word_wrap);
	    }
	}
    }
    g_artbuf_pos += len;
    if (read_something) {
    	g_artbuf_seek = tellart();
	g_artbuf_len = g_artbuf_pos + extra_chars;
	if (g_artsize >= 0)
	    g_artsize = g_raw_artsize-g_artbuf_seek+g_artbuf_len+g_htype[PAST_HEADER].minpos;
    }

    return bp;
}
