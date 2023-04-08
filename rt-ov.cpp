/* rt-ov.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "rt-ov.h"

#include "list.h"
#include "trn.h"
#include "cache.h"
#include "bits.h"
#include "head.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "util.h"
#include "util2.h"
#include "env.h"
#include "ng.h"
#include "term.h"
#include "final.h"
#include "rt-process.h"
#include "rt-util.h"

#include <parsedate/parsedate.h>

/* How many overview lines to read with one NNTP call */
enum
{
    OV_CHUNK_SIZE = 40
};

static header_line_type s_hdrnum[] = {
    PAST_HEADER, SUBJ_LINE, FROM_LINE, DATE_LINE, MSGID_LINE,
    REFS_LINE, BYTES_LINE, LINES_LINE, XREF_LINE
};

static void ov_parse(char *line, ART_NUM artnum, bool remote);
static const char *ov_name(const char *group);
static ov_field_num ov_num(char *hdr, char *end);

bool ov_init()
{
    bool has_overview_fmt;
    ov_field_num *fieldnum = g_datasrc->fieldnum;
    field_flags  *fieldflags = g_datasrc->fieldflags;
    g_datasrc->flags &= ~DF_TRY_OVERVIEW;
    FILE *overview;
    if (!g_datasrc->over_dir) {
        /* Check if the server is XOVER compliant */
	if (nntp_command("XOVER") <= 0)
	    return false;
	if (nntp_check() < 0)
	    return false;/*$$*/
	if (atoi(g_ser_line) == NNTP_BAD_COMMAND_VAL)
	    return false;
	/* Just in case... */
	if (*g_ser_line == NNTP_CLASS_OK)
	    nntp_finish_list();
        int ret = nntp_list("overview.fmt", "", 0);
        if (ret < -1)
	    return false;
	has_overview_fmt = ret > 0;
    }
    else
    {
	has_overview_fmt = g_datasrc->over_fmt != nullptr
			&& (overview = fopen(g_datasrc->over_fmt, "r")) != nullptr;
    }

    if (has_overview_fmt) {
	int i;
	fieldnum[0] = OV_NUM;
	fieldflags[OV_NUM] = FF_HAS_FIELD;
	for (i = 1;;) {
	    if (!g_datasrc->over_dir) {
		if (nntp_gets(g_buf, sizeof g_buf) < 0)
		    break;/*$$*/
		if (nntp_at_list_end(g_buf))
		    break;
	    }
	    else if (!fgets(g_buf, sizeof g_buf, overview)) {
		fclose(overview);
		break;
	    }
	    if (*g_buf == '#')
		continue;
	    if (i < OV_MAX_FIELDS) {
		char *s = strchr(g_buf,':');
		fieldnum[i] = ov_num(g_buf,s);
		fieldflags[fieldnum[i]] = FF_HAS_FIELD |
		    ((s && !strncasecmp("full",s+1,4))? FF_HAS_HDR : FF_NONE);
		i++;
	    }
	}
	if (!fieldflags[OV_SUBJ] || !fieldflags[OV_MSGID]
	 || !fieldflags[OV_FROM] || !fieldflags[OV_DATE])
	    return false;
	if (i < OV_MAX_FIELDS) {
	    int j;
	    for (j = OV_MAX_FIELDS; j--; ) {
		if (!fieldflags[j])
		    break;
	    }
	    while (i < OV_MAX_FIELDS)
		fieldnum[i++] = static_cast<ov_field_num>(j);
	}
    }
    else {
        for (int i = 0; i < OV_MAX_FIELDS; i++) {
	    fieldnum[i] = static_cast<ov_field_num>(i);
	    fieldflags[i] = FF_HAS_FIELD;
	}
	fieldflags[OV_XREF] = FF_CHECK4FIELD | FF_CHECK4HDR;
    }
    g_datasrc->flags |= DF_TRY_OVERVIEW;
    return true;
}

ov_field_num ov_num(char *hdr, char *end)
{
    if (!end)
	end = hdr + strlen(hdr);

    switch (set_line_type(hdr,end)) {
      case SUBJ_LINE:
	return OV_SUBJ;
      case AUTHOR_LINE:		/* This hack is for the Baen NNTP server */
      case FROM_LINE:
	return OV_FROM;
      case DATE_LINE:
	return OV_DATE;
      case MSGID_LINE:
	return OV_MSGID;
      case REFS_LINE:
	return OV_REFS;
      case BYTES_LINE:
	return OV_BYTES;
      case LINES_LINE:
	return OV_LINES;
      case XREF_LINE:
	return OV_XREF;
    }
    return OV_NUM;
}

/* Process the data in the group's news-overview file.
*/
bool ov_data(ART_NUM first, ART_NUM last, bool cheating)
{
    ART_NUM artnum, an;
    char* line;
    char* last_buf = g_buf;
    MEM_SIZE last_buflen = LBUFLEN;
    bool success = true;
    ART_NUM real_first = first;
    ART_NUM real_last = last;
    int line_cnt;
    int ov_chunk_size = cheating? OV_CHUNK_SIZE : OV_CHUNK_SIZE * 8;
    time_t started_request;
    bool remote = !g_datasrc->over_dir;

beginning:
    for (;;) {
	artnum = article_first(first);
	if (artnum > first || !(article_ptr(artnum)->flags & AF_CACHED))
	    break;
	g_spin_todo--;
	first++;
    }
    if (first > last)
	goto exit;
    if (remote) {
	if (last - first > ov_chunk_size + ov_chunk_size/2 - 1) {
	    last = first + ov_chunk_size - 1;
	    line_cnt = 0;
	}
    }
    started_request = time((time_t*)nullptr);
    for (;;) {
	artnum = article_last(last);
	if (artnum < last || !(article_ptr(artnum)->flags & AF_CACHED))
	    break;
	g_spin_todo--;
	last--;
    }

    if (remote) {
	sprintf(g_ser_line, "XOVER %ld-%ld", (long)first, (long)last);
	if (nntp_command(g_ser_line) <= 0 || nntp_check() <= 0) {
	    success = false;
	    goto exit;
	}
	if (g_verbose && !g_first_subject && !g_datasrc->ov_opened)
        {
            printf("\nGetting overview file.");
	    fflush(stdout);
        }
    }
    else if (g_datasrc->ov_opened < started_request - 60*60) {
	ov_close();
        g_datasrc->ov_in = fopen(ov_name(g_ngname.c_str()), "r");
        if (g_datasrc->ov_in == nullptr)
	    return false;
	if (g_verbose && !g_first_subject)
        {
            printf("\nReading overview file.");
	    fflush(stdout);
        }
    }
    if (!g_datasrc->ov_opened) {
	if (cheating)
	    setspin(SPIN_BACKGROUND);
	else {
	    int lots2do = ((g_datasrc->flags & DF_REMOTE)? g_net_speed : 20) * 100;
	    if (g_spin_estimate > g_spin_todo)
		g_spin_estimate = g_spin_todo;
	    setspin(g_spin_estimate > lots2do? SPIN_BARGRAPH : SPIN_FOREGROUND);
	}
	g_datasrc->ov_opened = started_request;
    }

    artnum = first-1;
    for (;;) {
	if (remote) {
	    line = nntp_get_a_line(last_buf,last_buflen,last_buf!=g_buf);
	    if (nntp_at_list_end(line))
		break;
	    line_cnt++;
	}
	else if (!(line = get_a_line(last_buf,last_buflen,last_buf!=g_buf,g_datasrc->ov_in)))
	    break;

	last_buf = line;
	last_buflen = g_buflen_last_line_got;
	an = atol(line);
	if (an < first)
	    continue;
	if (an > last) {
	    artnum = last;
	    if (remote)
		continue;
	    break;
	}
	g_spin_todo -= an - artnum - 1;
	ov_parse(line, artnum = an, remote);
	if (g_int_count) {
	    g_int_count = 0;
	    success = false;
	    if (!remote)
		break;
	}
	if (!remote && cheating) {
	    if (input_pending()) {
		success = false;
		break;
	    }
	    if (g_curr_artp != g_sentinel_artp) {
		pushchar('\f' | 0200);
		success = false;
		break;
	    }
	}
    }
    if (remote && line_cnt == 0 && last < real_last) {
	an = nntp_find_real_art(last);
	if (an > 0) {
	    last = an - 1;
	    g_spin_todo -= last - artnum;
	    artnum = last;
	}
    }
    if (remote) {
	int cachemask = (g_threaded_group? AF_THREADED : AF_CACHED);
        for (ARTICLE *ap = article_ptr(article_first(real_first));
             ap && article_num(ap) <= artnum;
             ap = article_nextp(ap))
	{
	    if (!(ap->flags & cachemask))
		onemissing(ap);
	}
	g_spin_todo -= last - artnum;
    }
    if (artnum > g_last_cached && artnum >= first)
	g_last_cached = artnum;
  exit:
    if (g_int_count || !success) {
	g_int_count = 0;
	success = false;
    }
    else if (remote) {
	if (cheating && g_curr_artp != g_sentinel_artp) {
	    pushchar('\f' | 0200);
	    success = false;
	} else if (last < real_last) {
	    if (!cheating || !input_pending()) {
		long elapsed_time = time((time_t*)nullptr) - started_request;
		long expected_time = cheating? 2 : 10;
		int max_chunk_size = cheating? 500 : 2000;
		ov_chunk_size += (expected_time - elapsed_time) * OV_CHUNK_SIZE;
		if (ov_chunk_size <= OV_CHUNK_SIZE / 2)
		    ov_chunk_size = OV_CHUNK_SIZE / 2 + 1;
		else if (ov_chunk_size > max_chunk_size)
		    ov_chunk_size = max_chunk_size;
		first = last+1;
		last = real_last;
		goto beginning;
	    }
	    success = false;
	}
    }
    if (!cheating && g_datasrc->ov_in)
	fseek(g_datasrc->ov_in, 0L, 0);	/* rewind it for the cheating phase */
    if (success && real_first <= g_first_cached) {
	g_first_cached = real_first;
	g_cached_all_in_range = true;
    }
    setspin(SPIN_POP);
    if (last_buf != g_buf)
	free(last_buf);
    return success;
}

static void ov_parse(char *line, ART_NUM artnum, bool remote)
{
    ov_field_num *fieldnum = g_datasrc->fieldnum;
    field_flags  *fieldflags = g_datasrc->fieldflags;
    char         *fields[OV_MAX_FIELDS];
    char         *tab;

    ARTICLE *article = article_ptr(artnum);
    if (article->flags & AF_THREADED) {
	g_spin_todo--;
	return;
    }

    if (g_len_last_line_got > 0 && line[g_len_last_line_got-1] == '\n') {
	if (g_len_last_line_got > 1 && line[g_len_last_line_got-2] == '\r')
	    line[g_len_last_line_got-2] = '\0';
	else
	    line[g_len_last_line_got-1] = '\0';
    }
    char *cp = line;

    memset((char*)fields,0,sizeof fields);
    for (int i = 0; cp && i < OV_MAX_FIELDS; cp = tab) {
        tab = strchr(cp, '\t');
        if (tab != nullptr)
	    *tab++ = '\0';
	int fn = fieldnum[i];
	if (!(fieldflags[fn] & (FF_HAS_FIELD | FF_CHECK4FIELD)))
	    break;
	if (fieldflags[fn] & (FF_HAS_HDR | FF_CHECK4HDR)) {
	    char* s = strchr(cp, ':');
	    if (fieldflags[fn] & FF_CHECK4HDR) {
		if (s)
		    fieldflags[fn] |= FF_HAS_HDR;
		fieldflags[fn] &= ~FF_CHECK4HDR;
	    }
	    if (fieldflags[fn] & FF_HAS_HDR) {
		if (!s)
		    break;
		if (s - cp != g_htype[s_hdrnum[fn]].length
		 || strncasecmp(cp,g_htype[s_hdrnum[fn]].name,g_htype[s_hdrnum[fn]].length))
		    continue;
		cp = s;
		while (*++cp == ' ') ;
	    }
	}
	fields[fn] = cp;
	i++;
    }
    if (!fields[OV_SUBJ] || !fields[OV_MSGID]
     || !fields[OV_FROM] || !fields[OV_DATE])
	return;		/* skip this line if it's too short */

    if (!article->subj)
	set_subj_line(article, fields[OV_SUBJ], strlen(fields[OV_SUBJ]));
    if (!article->msgid)
	set_cached_line(article, MSGID_LINE, savestr(fields[OV_MSGID]));
    if (!article->from)
	set_cached_line(article, FROM_LINE, savestr(fields[OV_FROM]));
    if (!article->date)
	article->date = parsedate(fields[OV_DATE]);
    if (!article->bytes && fields[OV_BYTES])
	set_cached_line(article, BYTES_LINE, fields[OV_BYTES]);
    if (!article->lines && fields[OV_LINES])
	set_cached_line(article, LINES_LINE, fields[OV_LINES]);

    if (fieldflags[OV_XREF] & (FF_HAS_FIELD | FF_CHECK4FIELD)) {
	if (!article->xrefs && fields[OV_XREF]) {
	    /* Exclude an xref for just this group */
	    cp = strchr(fields[OV_XREF], ':');
	    if (cp && strchr(cp+1, ':'))
		article->xrefs = savestr(fields[OV_XREF]);
	}

	if (fieldflags[OV_XREF] & FF_HAS_FIELD) {
	    if (!article->xrefs)
		article->xrefs = "";
	}
	else if (fields[OV_XREF]) {
            for (ART_NUM an = article_first(g_absfirst); an<artnum; an=article_next(an)) {
		ARTICLE *ap = article_ptr(an);
		if (!ap->xrefs)
		    ap->xrefs = "";
	    }
	    fieldflags[OV_XREF] |= FF_HAS_FIELD;
	}
    }

    if (remote)
	article->flags |= AF_EXISTS;

    if (g_threaded_group) {
	if (valid_article(article))
	    thread_article(article, fields[OV_REFS]);
    } else if (!(article->flags & AF_CACHED))
	cache_article(article);

    if (article->flags & AF_UNREAD)
	check_poster(article);
    spin(100);
}

/* Change a newsgroup name into the name of the overview data file.  We
** subsitute any '.'s in the group name into '/'s, prepend the path, and
** append the '/.overview' or '.ov') on to the end.
*/
static const char *ov_name(const char *group)
{
    strcpy(g_buf, g_datasrc->over_dir);
    char *cp = g_buf + strlen(g_buf);
    *cp++ = '/';
    strcpy(cp, group);
    while ((cp = strchr(cp, '.')))
	*cp = '/';
    strcat(g_buf, OV_FILE_NAME);
    return g_buf;
}

void ov_close()
{
    if (g_datasrc && g_datasrc->ov_opened) {
	if (g_datasrc->ov_in) {
	    (void) fclose(g_datasrc->ov_in);
	    g_datasrc->ov_in = nullptr;
	}
	g_datasrc->ov_opened = 0;
    }
}

char *ov_fieldname(int num)
{
    return g_htype[s_hdrnum[num]].name;
}

char *ov_field(ARTICLE *ap, int num)
{
    int fn = g_datasrc->fieldnum[num];
    if (!(g_datasrc->fieldflags[fn] & (FF_HAS_FIELD | FF_CHECK4FIELD)))
	return nullptr;

    if (fn == OV_NUM) {
	sprintf(g_cmd_buf, "%ld", (long)ap->num);
	return g_cmd_buf;
    }

    if (fn == OV_DATE) {
	sprintf(g_cmd_buf, "%ld", (long)ap->date);
	return g_cmd_buf;
    }

    char *s = get_cached_line(ap, s_hdrnum[fn], true);
    return s? s : "";
}
