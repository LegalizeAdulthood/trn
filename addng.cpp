/* addng.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "hash.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "util.h"
#include "util2.h"
#include "only.h"
#include "term.h"
#include "rcstuff.h"
#include "autosub.h"
#include "rt-select.h"
#include "addng.h"
#include "addng.ih"

#include <stdlib.h>

ADDGROUP* g_first_addgroup{};
ADDGROUP* g_last_addgroup{};

ADDGROUP* g_sel_page_gp{};
ADDGROUP* g_sel_next_gp{};


static int addng_cmp(const char *key, int keylen, HASHDATUM data)
{
    return memcmp(key, ((ADDGROUP *)data.dat_ptr)->name, keylen);
}

static int build_addgroup_list(int keylen, HASHDATUM *data, int extra)
{
    ADDGROUP* node = (ADDGROUP*)data->dat_ptr;

    node->num = addgroup_cnt++;
    node->next = nullptr;
    node->prev = g_last_addgroup;
    if (g_last_addgroup)
	g_last_addgroup->next = node;
    else
	g_first_addgroup = node;
    g_last_addgroup = node;
    return 0;
}

void addng_init()
{
    //
}

bool find_new_groups()
{
    NEWSRC* rp;
    NG_NUM oldcnt = newsgroup_cnt;	/* remember # newsgroups */

    /* Skip this check if the -q flag was given. */
    if (quickstart)
	return false
	;

    for (rp = multirc->first; rp; rp = rp->next) {
	if (ALLBITS(rp->flags, RF_ADD_NEWGROUPS | RF_ACTIVE)) {
	    if (rp->datasrc->flags & DF_REMOTE)
		new_nntp_groups(rp->datasrc);
	    else
		new_local_groups(rp->datasrc);
	}
    }
    addnewbydefault = 0;

    process_list(GNG_RELOC);

    return oldcnt != newsgroup_cnt;
}

static void process_list(int flag)
{
    ADDGROUP* node;
    ADDGROUP* prevnode;

    if (!flag) {
	sprintf(cmd_buf,"\nUnsubscribed but mentioned in your current newsrc%s:\n",
		multirc->first->next? "s" : "");
	print_lines(cmd_buf, STANDOUT);
    }
    if ((node = g_first_addgroup) != nullptr && flag && UseAddSelector)
	addgroup_selector(flag);
    while (node) {
	if (!flag) {
	    sprintf(cmd_buf, "%s\n", node->name);
	    print_lines(cmd_buf, NOMARKING);
	}
	else if (!UseAddSelector)
	    get_ng(node->name,flag);	/* add newsgroup -- maybe */
	prevnode = node;
	node = node->next;
	free((char*)prevnode);
    }
    g_first_addgroup = nullptr;
    g_last_addgroup = nullptr;
    addgroup_cnt = 0;
}

static void new_nntp_groups(DATASRC *dp)
{
    char* s;
    int len;
    time_t server_time;
    NGDATA* np;
    bool foundSomething = false;
    long high, low;
    HASHTABLE* newngs;

    set_datasrc(dp);

    server_time = nntp_time();
    if (server_time == -2)
	return; /*$$*/
    if (nntp_newgroups(dp->lastnewgrp) < 1) { /*$$*/
	printf("Can't get new groups from server:\n%s\n", g_ser_line);
	return;
    }
    newngs = hashcreate(33, addng_cmp);

    while (true) {
	high = 0, low = 1;
	if (nntp_gets(g_ser_line, sizeof g_ser_line) < 0)
	    break;
#ifdef DEBUG
	if (debug & DEB_NNTP)
	    printf("<%s\n", g_ser_line) FLUSH;
#endif
	if (nntp_at_list_end(g_ser_line))
	    break;
	foundSomething = true;
	if ((s = strchr(g_ser_line, ' ')) != nullptr)
	    len = s - g_ser_line;
	else
	    len = strlen(g_ser_line);
	if (dp->act_sf.fp) {
	    if (find_actgrp(dp, buf, g_ser_line, len, (ART_NUM)0)) {
		if (!s)
		    s = buf + len + 1;
	    }
	    else {
		char ch = 'y';
		if (s)
		    sscanf(s+1, "%ld %ld %c", &high, &low, &ch);
		else
		    s = g_ser_line + len;
		sprintf(s, " %010ld %05ld %c\n", high, low, ch);
		(void) srcfile_append(&dp->act_sf, g_ser_line, len);
	    }
	}
	if (s) {
	    *s++ = '\0';
	    while (isdigit(*s) || isspace(*s)) s++;
	    if (*s == 'x' || *s == '=')
		continue;
	}
	if ((np = find_ng(g_ser_line)) != nullptr && np->toread > TR_UNSUB)
	    continue;
	add_to_hash(newngs, g_ser_line, high-low, auto_subscribe(g_ser_line));
    }
    if (foundSomething) {
	hashwalk(newngs, build_addgroup_list, 0);
	srcfile_end_append(&dp->act_sf, dp->extra_name);
	dp->lastnewgrp = server_time;
    }
    hashdestroy(newngs);
}

static void new_local_groups(DATASRC *dp)
{
    char* s;
    time_t lastone;
    NGDATA* np;
    char tmpbuf[LBUFLEN];
    long high, low;
    char ch;
    HASHTABLE* newngs;

    datasrc = dp;

    /* did active.times file grow? */
    stat(dp->extra_name,&filestat);
    if (filestat.st_size == dp->act_sf.recent_cnt)
	return;

    tmpfp = fopen(dp->extra_name,"r");
    if (tmpfp == nullptr) {
	printf(cantopen,dp->extra_name) FLUSH;
	termdown(1);
	return;
    }
    lastone = time((time_t*)nullptr) - 24L * 60 * 60 - 1;
    newngs = hashcreate(33, addng_cmp);

    while (fgets(buf,LBUFLEN,tmpfp) != nullptr) {
	if ((s = strchr(buf, ' ')) == nullptr
	 || (lastone = atol(s+1)) < dp->lastnewgrp)
	    continue;
	*s = '\0';
	if (!find_actgrp(datasrc, tmpbuf, buf, s - buf, (ART_NUM)0))
	    continue;
	high = 0, low = 1, ch = 'y';
	sscanf(tmpbuf + (s-buf) + 1, "%ld %ld %c", &high, &low, &ch);
	if (ch == 'x' || ch == '=')
	    continue;
	if ((np = find_ng(buf)) != nullptr)
	    continue;
	add_to_hash(newngs, buf, high-low, auto_subscribe(buf));
    }
    fclose(tmpfp);

    hashwalk(newngs, build_addgroup_list, 0);
    hashdestroy(newngs);
    dp->lastnewgrp = lastone+1;
    dp->act_sf.recent_cnt = filestat.st_size;
}

static void add_to_hash(HASHTABLE *ng, const char *name, int toread, char_int ch)
{
    HASHDATUM data;
    ADDGROUP* node;
    unsigned namelen = strlen(name);
    
    data.dat_len = namelen + sizeof (ADDGROUP);
    node = (ADDGROUP*)safemalloc(data.dat_len);
    data.dat_ptr = (char *)node;
    switch (ch) {
      case ':':
	node->flags = AGF_SEL;
	break;
      case '!':
	node->flags = AGF_DEL;
	break;
      default:
	node->flags = 0;
	break;
    }
    node->toread = (toread < 0)? 0 : toread;
    strcpy(node->name, name);
    node->datasrc = datasrc;
    node->next = node->prev = nullptr;
    hashstore(ng, name, namelen, data);
}

static void add_to_list(const char *name, int toread, char_int ch)
{
    ADDGROUP* node = g_first_addgroup;

    while (node) {
	if (!strcmp(node->name,name))
	    return;
	node = node->next;
    }

    node = (ADDGROUP*)safemalloc(strlen(name) + sizeof (ADDGROUP));
    switch (ch) {
      case ':':
	node->flags = AGF_SEL;
	break;
      case '!':
	node->flags = AGF_DEL;
	break;
      default:
	node->flags = 0;
	break;
    }
    node->toread = (toread < 0)? 0 : toread;
    node->num = addgroup_cnt++;
    strcpy(node->name, name);
    node->datasrc = datasrc;
    node->next = nullptr;
    node->prev = g_last_addgroup;
    if (g_last_addgroup)
	g_last_addgroup->next = node;
    else
	g_first_addgroup = node;
    g_last_addgroup = node;
}

bool scanactive(bool add_matching)
{
    DATASRC* dp;
    NG_NUM oldcnt = newsgroup_cnt;	/* remember # of newsgroups */

    if (!add_matching)
	print_lines("Completely unsubscribed newsgroups:\n", STANDOUT);

    for (dp = datasrc_first(); dp && dp->name; dp = datasrc_next(dp)) {
	if (!(dp->flags & DF_OPEN))
	    continue;
	set_datasrc(dp);
	if (dp->act_sf.fp)
	    hashwalk(dp->act_sf.hp, list_groups, add_matching);
	else {
	    if (maxngtodo != 1)
		strcpy(buf, "*");
	    else {
		if (ngtodo[0][0] == '^')
		    sprintf(buf,"%s*", &ngtodo[0][1]);
		else
		    sprintf(buf,"*%s*", ngtodo[0]);
		if (buf[strlen(buf)-2] == '$')
		    buf[strlen(buf)-2] = '\0';
	    }
	    if (nntp_list("active", buf, strlen(buf)) == 1) {
		while (!nntp_at_list_end(g_ser_line)) {
		    scanline(g_ser_line,add_matching);
		    if (nntp_gets(g_ser_line, sizeof g_ser_line) < 0)
			break; /*$$*/
		}
	    }
	}
    }

    process_list(add_matching);

    if (in_ng) /*$$*/
	set_datasrc(ngptr->rc->datasrc);

    return oldcnt != newsgroup_cnt;
}

static int list_groups(int keylen, HASHDATUM *data, int add_matching)
{
    char* bp = ((LISTNODE*)data->dat_ptr)->data + data->dat_len;
    int linelen = strchr(bp, '\n') - bp + 1;
    (void) memcpy(buf,bp,linelen);
    buf[linelen] = '\0';
    scanline(buf,add_matching);
    return 0;
}

static void scanline(char *actline, bool add_matching)
{
    char* s;
    NGDATA* np;
    long high, low;
    char ch;

    if ((s = strchr(actline,' ')) == nullptr)
	return;
    *s++ = '\0';		/* this buffer is expendable */
    high = 0, low = 1, ch = 'y';
    sscanf(s, "%ld %ld %c", &high, &low, &ch);
    if (ch == 'x' || !strncmp(actline,"to.",3))
	return;
    if (!inlist(actline))
	return;
    if ((np = find_ng(actline)) != nullptr && np->toread > TR_UNSUB)
	return;
    if (add_matching || np) {
	/* it's not in a newsrc */
	add_to_list(actline, high-low, 0);
    }
    else {
	strcat(actline,"\n");
	print_lines(actline, NOMARKING);
    }
}

static int agorder_number(const ADDGROUP **app1, const ADDGROUP **app2)
{
    ART_NUM eq = (*app1)->num - (*app2)->num;
    return eq > 0? sel_direction : -sel_direction;
}

static int agorder_groupname(const ADDGROUP **app1, const ADDGROUP **app2)
{
    return strcasecmp((*app1)->name, (*app2)->name) * sel_direction;
}

static int agorder_count(const ADDGROUP **app1, const ADDGROUP **app2)
{
    long eq = (*app1)->toread - (*app2)->toread;
    if (eq)
	return eq > 0? sel_direction : -sel_direction;
    return agorder_groupname(app1, app2);
}

/* Sort the newsgroups into the chosen order.
*/
void sort_addgroups()
{
    ADDGROUP* ap;
    int i;
    ADDGROUP** lp;
    ADDGROUP** ag_list;
    int (*sort_procedure)(const ADDGROUP**, const ADDGROUP**);

    switch (sel_sort) {
      case SS_NATURAL:
      default:
	sort_procedure = agorder_number;
	break;
      case SS_STRING:
	sort_procedure = agorder_groupname;
	break;
      case SS_COUNT:
	sort_procedure = agorder_count;
	break;
    }

    ag_list = (ADDGROUP**)safemalloc(addgroup_cnt * sizeof(ADDGROUP*));
    for (lp = ag_list, ap = g_first_addgroup; ap; ap = ap->next)
	*lp++ = ap;
    assert(lp - ag_list == addgroup_cnt);

    qsort(ag_list, addgroup_cnt, sizeof(ADDGROUP*), (int(*)(void const *, void const *)) sort_procedure);

    g_first_addgroup = ap = ag_list[0];
    ap->prev = nullptr;
    for (i = addgroup_cnt, lp = ag_list; --i; lp++) {
	lp[0]->next = lp[1];
	lp[1]->prev = lp[0];
    }
    g_last_addgroup = lp[0];
    g_last_addgroup->next = nullptr;
    free((char*)ag_list);
}
