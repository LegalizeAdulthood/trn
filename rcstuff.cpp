/* rcstuff.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "util.h"
#include "util2.h"
#include "hash.h"
#include "cache.h"
#include "bits.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "term.h"
#include "final.h"
#include "trn.h"
#include "env.h"
#include "init.h"
#include "only.h"
#include "rcln.h"
#include "last.h"
#include "autosub.h"
#include "rt-select.h"
#include "rt-page.h"
#include "rcstuff.h"

HASHTABLE *g_newsrc_hash{};
MULTIRC *g_sel_page_mp{};
MULTIRC *g_sel_next_mp{};
LIST *g_multirc_list{}; /* a list of all MULTIRCs */
MULTIRC *g_multirc{};   /* the current MULTIRC */
bool g_paranoid{};      /* did we detect some inconsistency in .newsrc? */
int g_addnewbydefault{};

enum
{
    RI_ID = 1,
    RI_NEWSRC = 2,
    RI_ADDGROUPS = 3
};

static INI_WORDS s_rcgroups_ini[] = {
    { 0, "RCGROUPS", 0 },
    { 0, "ID", 0 },
    { 0, "Newsrc", 0 },
    { 0, "Add Groups", 0 },
    { 0, 0, 0 }
};
static bool s_foundany{};

static bool clear_ngitem(char *cp, int arg);
static bool lock_newsrc(NEWSRC *rp);
static void unlock_newsrc(NEWSRC *rp);
static bool open_newsrc(NEWSRC *rp);
static void init_ngnode(LIST *list, LISTNODE *node);
static void parse_rcline(NGDATA *np);
static NGDATA *add_newsgroup(NEWSRC *rp, char *ngn, char_int c);
static int rcline_cmp(const char *key, int keylen, HASHDATUM data);

bool rcstuff_init()
{
    MULTIRC* mptr = nullptr;
    int i;

    g_multirc_list = new_list(0,0,sizeof(MULTIRC),20,LF_ZERO_MEM|LF_SPARSE,nullptr);

    if (g_trnaccess_mem) {
	NEWSRC* rp;
	char* s;
	char* section;
	char* cond;
	char** vals = prep_ini_words(s_rcgroups_ini);
	s = g_trnaccess_mem;
	while ((s = next_ini_section(s,&section,&cond)) != nullptr) {
	    if (*cond && !check_ini_cond(cond))
		continue;
	    if (strncasecmp(section, "group ", 6))
		continue;
	    i = atoi(section+6);
	    if (i < 0)
		i = 0;
	    s = parse_ini_section(s, s_rcgroups_ini);
	    if (!s)
		break;
	    rp = new_newsrc(vals[RI_ID],vals[RI_NEWSRC],vals[RI_ADDGROUPS]);
	    if (rp) {
		MULTIRC* mp;
		NEWSRC* prev_rp;

		mp = multirc_ptr(i);
		prev_rp = mp->first;
		if (!prev_rp)
		    mp->first = rp;
		else {
		    while (prev_rp->next)
			prev_rp = prev_rp->next;
		    prev_rp->next = rp;
		}
		mp->num = i;
		if (!mptr)
		    mptr = mp;
		/*rp->flags |= RF_USED;*/
	    }
	    /*else
		;$$complain?*/
	}
	free((char*)vals);
	free(g_trnaccess_mem);
    }

    if (UseNewsrcSelector && !checkflag)
	return true;

    s_foundany = false;
    if (mptr && !use_multirc(mptr))
	use_next_multirc(mptr);
    if (!g_multirc) {
	mptr = multirc_ptr(0);
	mptr->first = new_newsrc("default",nullptr,nullptr);
	if (!use_multirc(mptr)) {
	    printf("Couldn't open any newsrc groups.  Is your access file ok?\n");
	    finalize(1);
	}
    }
    if (checkflag)			/* were we just checking? */
	finalize(s_foundany);		/* tell them what we found */
    return s_foundany;
}

NEWSRC *new_newsrc(char *name, char *newsrc, char *add_ok)
{
    char tmpbuf[CBUFLEN];
    NEWSRC* rp;
    DATASRC* dp;

    if (!name || !*name)
	return nullptr;

    if (!newsrc || !*newsrc) {
	newsrc = getenv("NEWSRC");
	if (!newsrc)
	    newsrc = RCNAME;
    }

    dp = get_datasrc(name);
    if (!dp)
	return nullptr;

    rp = (NEWSRC*)safemalloc(sizeof (NEWSRC));
    memset((char*)rp,0,sizeof (NEWSRC));
    rp->datasrc = dp;
    rp->name = savestr(filexp(newsrc));
    sprintf(tmpbuf, RCNAME_OLD, rp->name);
    rp->oldname = savestr(tmpbuf);
    sprintf(tmpbuf, RCNAME_NEW, rp->name);
    rp->newname = savestr(tmpbuf);

    switch (add_ok? *add_ok : 'y') {
    case 'n':
    case 'N':
	break;
    default:
	if (dp->flags & DF_ADD_OK)
	    rp->flags |= RF_ADD_NEWGROUPS;
	/* FALL THROUGH */
    case 'm':
    case 'M':
	rp->flags |= RF_ADD_GROUPS;
	break;
    }
    return rp;
}

bool use_multirc(MULTIRC *mp)
{
    NEWSRC* rp;
    bool had_trouble = false;
    bool had_success = false;

    for (rp = mp->first; rp; rp = rp->next) {
	if ((rp->datasrc->flags & DF_UNAVAILABLE) || !lock_newsrc(rp)
	 || !open_datasrc(rp->datasrc) || !open_newsrc(rp)) {
	    unlock_newsrc(rp);
	    had_trouble = true;
	}
	else {
	    rp->datasrc->flags |= DF_ACTIVE;
	    rp->flags |= RF_ACTIVE;
	    had_success = true;
	}
    }
    if (had_trouble)
	get_anything();
    if (!had_success)
	return false;
    g_multirc = mp;
#ifdef NO_FILELINKS
    if (!write_newsrcs(g_multirc))
	get_anything();
#endif
    return true;
}

void unuse_multirc(MULTIRC *mptr)
{
    NEWSRC* rp;

    if (!mptr)
	return;

    write_newsrcs(mptr);

    for (rp = mptr->first; rp; rp = rp->next) {
	unlock_newsrc(rp);
	rp->flags &= ~RF_ACTIVE;
	rp->datasrc->flags &= ~DF_ACTIVE;
    }
    if (g_ngdata_list) {
	close_cache();
	hashdestroy(g_newsrc_hash);
	walk_list(g_ngdata_list, clear_ngitem, 0);
	delete_list(g_ngdata_list);
	g_ngdata_list = nullptr;
	g_first_ng = nullptr;
	g_last_ng = nullptr;
	g_ngptr = nullptr;
	g_current_ng = nullptr;
	g_recent_ng = nullptr;
	g_starthere = nullptr;
	g_sel_page_np = nullptr;
    }
    g_ngdata_cnt = 0;
    g_newsgroup_cnt = 0;
    g_newsgroup_toread = 0;
    g_multirc = nullptr;
}

bool use_next_multirc(MULTIRC *mptr)
{
    MULTIRC* mp = multirc_ptr(mptr->num);

    unuse_multirc(mptr);

    for (;;) {
	mp = multirc_next(mp);
	if (!mp)
	    mp = multirc_low();
	if (mp == mptr) {
	    use_multirc(mptr);
	    return false;
	}
	if (use_multirc(mp))
	    break;
    }
    return true;
}

bool use_prev_multirc(MULTIRC *mptr)
{
    MULTIRC* mp = multirc_ptr(mptr->num);

    unuse_multirc(mptr);

    for (;;) {
	mp = multirc_prev(mp);
	if (!mp)
	    mp = multirc_high();
	if (mp == mptr) {
	    use_multirc(mptr);
	    return false;
	}
	if (use_multirc(mp))
	    break;
    }
    return true;
}

char *multirc_name(MULTIRC *mp)
{
    char* cp;
    if (mp->first->next)
	return "<each-newsrc>";
    if ((cp = strrchr(mp->first->name, '/')) != nullptr)
	return cp+1;
    return mp->first->name;
}

static bool clear_ngitem(char *cp, int arg)
{
    NGDATA* ncp = (NGDATA*)cp;

    if (ncp->rcline != nullptr) {
	if (!checkflag)
	    free(ncp->rcline);
	ncp->rcline = nullptr;
    }
    return false;
}

/* make sure there is no trn out there reading this newsrc */

static bool lock_newsrc(NEWSRC *rp)
{
    long processnum = 0;
    char* runninghost = "(Unknown)";
    char* s;

    if (checkflag)
	return true;

    s = filexp(RCNAME);
    if (!strcmp(rp->name,s))
	rp->lockname = savestr(filexp(LOCKNAME));
    else {
	sprintf(buf, RCNAME_INFO, rp->name);
	rp->infoname = savestr(buf);
	sprintf(buf, RCNAME_LOCK, rp->name);
	rp->lockname = savestr(buf);
    }

    tmpfp = fopen(rp->lockname,"r");
    if (tmpfp != nullptr) {
	if (fgets(buf,LBUFLEN,tmpfp)) {
	    processnum = atol(buf);
	    if (fgets(buf,LBUFLEN,tmpfp) && *buf
	     && *(s = buf + strlen(buf) - 1) == '\n') {
		*s = '\0';
		runninghost = buf;
	    }
	}
	fclose(tmpfp);
    }
    if (processnum) {
#ifndef MSDOS
	if (verbose)
	    printf("\nThe requested newsrc is locked by process %ld on host %s.\n",
		   processnum, runninghost) FLUSH;
	else
	    printf("\nNewsrc locked by %ld on host %s.\n",processnum,runninghost) FLUSH;
	termdown(2);
	if (strcmp(runninghost,g_local_host)) {
	    if (verbose)
		printf("\n\
Since that's not the same host as this one (%s), we must\n\
assume that process still exists.  To override this check, remove\n\
the lock file: %s\n", g_local_host, rp->lockname) FLUSH;
	    else
		printf("\nThis host (%s) doesn't match.\nCan't unlock %s.\n",
		       g_local_host, rp->lockname) FLUSH;
	    termdown(2);
	    if (bizarre)
		resetty();
	    finalize(0);
	}
	if (processnum == g_our_pid) {
	    if (verbose)
		printf("\n\
Hey, that *my* pid!  Your access file is trying to use the same newsrc\n\
more than once.\n") FLUSH;
	    else
		printf("\nAccess file error (our pid detected).\n") FLUSH;
	    termdown(2);
	    return false;
	}
	if (kill(processnum, 0) != 0) {
	    /* Process is apparently gone */
	    sleep(2);
	    if (verbose)
		fputs("\n\
That process does not seem to exist anymore.  The count of read articles\n\
may be incorrect in the last newsgroup accessed by that other (defunct)\n\
process.\n\n",stdout) FLUSH;
	    else
		fputs("\nProcess crashed.\n",stdout) FLUSH;
	    if (g_lastngname) {
		if (verbose)
		    printf("(The last newsgroup accessed was %s.)\n\n",
			   g_lastngname) FLUSH;
		else
		    printf("(In %s.)\n\n",g_lastngname) FLUSH;
	    }
	    termdown(2);
	    get_anything();
	    newline();
	}
	else {
	    if (verbose)
		printf("\n\
It looks like that process still exists.  To override this, remove\n\
the lock file: %s\n", rp->lockname) FLUSH;
	    else
		printf("\nCan't unlock %s.\n", rp->lockname) FLUSH;
	    termdown(2);
	    if (bizarre)
		resetty();
	    finalize(0);
	}
#endif
    }
    tmpfp = fopen(rp->lockname,"w");
    if (tmpfp == nullptr) {
	printf(cantcreate,rp->lockname) FLUSH;
	sig_catcher(0);
    }
    fprintf(tmpfp,"%ld\n%s\n",g_our_pid,g_local_host);
    fclose(tmpfp);
    return true;
}

static void unlock_newsrc(NEWSRC *rp)
{
    safefree0(rp->infoname);
    if (rp->lockname) {
 	remove(rp->lockname);
	free(rp->lockname);
	rp->lockname = nullptr;
    }
}

static bool open_newsrc(NEWSRC *rp)
{
    NGDATA* np;
    NGDATA* prev_np;
    char* some_buf;
    long length;
    FILE* rcfp;
    HASHDATUM data;

    /* make sure the .newsrc file exists */

    if ((rcfp = fopen(rp->name,"r")) == nullptr) {
	rcfp = fopen(rp->name,"w+");
	if (rcfp == nullptr) {
	    printf("\nCan't create %s.\n", rp->name) FLUSH;
	    termdown(2);
	    return false;
	}
	some_buf = SUBSCRIPTIONS;
	if ((rp->datasrc->flags & DF_REMOTE)
	 && nntp_list("SUBSCRIPTIONS","",0) == 1) {
	    do {
		fputs(g_ser_line,rcfp);
		fputc('\n',rcfp);
		if (nntp_gets(g_ser_line, sizeof g_ser_line) < 0)
		    break;
	    } while (!nntp_at_list_end(g_ser_line));
	}
	else if (*some_buf && (tmpfp = fopen(filexp(some_buf),"r")) != nullptr) {
	    while (fgets(buf,sizeof buf,tmpfp))
		fputs(buf,rcfp);
	    fclose(tmpfp);
	}
	fseek(rcfp, 0L, 0);
    }
    else {
	/* File exists; if zero length and backup isn't, complain */
	if (fstat(fileno(rcfp),&filestat) < 0) {
	    perror(rp->name);
	    return false;
	}
	if (filestat.st_size == 0
	 && stat(rp->oldname,&filestat) >= 0 && filestat.st_size > 0) {
	    printf("Warning: %s is zero length but %s is not.\n",
		   rp->name,rp->oldname);
	    printf("Either recover your newsrc or else remove the backup copy.\n");
	    termdown(2);
	    return false;
	}
	/* unlink backup file name and backup current name */
	remove(rp->oldname);
#ifndef NO_FILELINKS
	safelink(rp->name,rp->oldname);
#endif
    }

    if (!g_ngdata_list) {
	/* allocate memory for rc file globals */
	g_ngdata_list = new_list(0, 0, sizeof (NGDATA), 200, 0, init_ngnode);
	g_newsrc_hash = hashcreate(3001, rcline_cmp);
    }

    if (g_ngdata_cnt)
	prev_np = ngdata_ptr(g_ngdata_cnt-1);
    else
	prev_np = nullptr;

    /* read in the .newsrc file */

    while ((some_buf = get_a_line(buf, LBUFLEN,false,rcfp)) != nullptr) {
	length = len_last_line_got;	/* side effect of get_a_line */
	if (length <= 1)		/* only a newline??? */
	    continue;
	np = ngdata_ptr(g_ngdata_cnt++);
	if (prev_np)
	    prev_np->next = np;
	else
	    g_first_ng = np;
	np->prev = prev_np;
	prev_np = np;
	np->rc = rp;
	g_newsgroup_cnt++;
	if (some_buf[length-1] == '\n')
	    some_buf[--length] = '\0';	/* wipe out newline */
	if (some_buf == buf)
	    np->rcline = savestr(some_buf);  /* make semipermanent copy */
	else {
	    /*NOSTRICT*/
#ifndef lint
	    some_buf = saferealloc(some_buf,(MEM_SIZE)(length+1));
#endif
	    np->rcline = some_buf;
	}
	if (*some_buf == ' ' || *some_buf == '\t'
	 || !strncmp(some_buf,"options",7)) {	/* non-useful line? */
	    np->toread = TR_JUNK;
	    np->subscribechar = ' ';
	    np->numoffset = 0;
	    continue;
	}
	parse_rcline(np);
	data = hashfetch(g_newsrc_hash, np->rcline, np->numoffset - 1);
	if (data.dat_ptr) {
	    np->toread = TR_IGNORE;
	    continue;
	}
	if (np->subscribechar == NEGCHAR) {
	    np->toread = TR_UNSUB;
	    sethash(np);
	    continue;
	}
	g_newsgroup_toread++;

	/* now find out how much there is to read */

	if (!inlist(buf) || (suppress_cn && s_foundany && !g_paranoid))
	    np->toread = TR_NONE;	/* no need to calculate now */
	else
	    set_toread(np, ST_LAX);
	if (np->toread > TR_NONE) {	/* anything unread? */
	    if (!s_foundany) {
		g_starthere = np;
		s_foundany = true;	/* remember that fact*/
	    }
	    if (suppress_cn) {		/* if no listing desired */
		if (checkflag)		/* if that is all they wanted */
		    finalize(1);	/* then bomb out */
	    }
	    else {
		if (verbose)
		    printf("Unread news in %-40s %5ld article%s\n",
			np->rcline,(long)np->toread,PLURAL(np->toread)) FLUSH;
		else
		    printf("%s: %ld article%s\n",
			np->rcline,(long)np->toread,PLURAL(np->toread)) FLUSH;
		termdown(1);
		if (g_int_count) {
		    countdown = 1;
		    g_int_count = 0;
		}
		if (countdown) {
		    if (!--countdown) {
			fputs("etc.\n",stdout) FLUSH;
			if (checkflag)
			    finalize(1);
			suppress_cn = true;
		    }
		}
	    }
	}
	sethash(np);
    }
    if (prev_np) {
	prev_np->next = nullptr;
	g_last_ng = prev_np;
    }
    fclose(rcfp);			/* close .newsrc */
#ifdef NO_FILELINKS
    remove(rp->oldname);
    rename(rp->name,rp->oldname);
    rp->flags |= RF_RCCHANGED;
#endif
    if (rp->infoname) {
	if ((tmpfp = fopen(rp->infoname,"r")) != nullptr) {
	    if (fgets(buf,sizeof buf,tmpfp) != nullptr) {
		long actnum, descnum;
		char* s;
		buf[strlen(buf)-1] = '\0';
		if ((s = strchr(buf, ':')) != nullptr && s[1] == ' ' && s[2]) {
		    safefree0(g_lastngname);
		    g_lastngname = savestr(s+2);
		}
		if (fscanf(tmpfp,"New-Group-State: %ld,%ld,%ld",
			   &g_lastnewtime,&actnum,&descnum) == 3) {
		    rp->datasrc->act_sf.recent_cnt = actnum;
		    rp->datasrc->desc_sf.recent_cnt = descnum;
		}
	    }
	}
    }
    else {
	readlast();
	if (rp->datasrc->flags & DF_REMOTE) {
	    rp->datasrc->act_sf.recent_cnt = g_lastactsiz;
	    rp->datasrc->desc_sf.recent_cnt = g_lastextranum;
	}
	else
	{
	    rp->datasrc->act_sf.recent_cnt = g_lastextranum;
	    rp->datasrc->desc_sf.recent_cnt = 0;
	}
    }
    rp->datasrc->lastnewgrp = g_lastnewtime;

    if (g_paranoid && !checkflag)
	cleanup_newsrc(rp);
    return true;
}

/* Initialize the memory for an entire node's worth of article's */
static void init_ngnode(LIST *list, LISTNODE *node)
{
    ART_NUM i;
    NGDATA* np;
    memset(node->data,0,list->items_per_node * list->item_size);
    for (i = node->low, np = (NGDATA*)node->data; i <= node->high; i++, np++)
	np->num = i;
}

static void parse_rcline(NGDATA *np)
{
    char* s;
    int len;

    for (s=np->rcline; *s && *s!=':' && *s!=NEGCHAR && !isspace(*s); s++) ;
    len = s - np->rcline;
    if ((!*s || isspace(*s)) && !checkflag) {
#ifndef lint
	np->rcline = saferealloc(np->rcline,(MEM_SIZE)len + 3);
#endif
	s = np->rcline + len;
	strcpy(s, ": ");
    }
    if (*s == ':' && s[1] && s[2] == '0') {
	np->flags |= NF_UNTHREADED;
	s[2] = '1';
    }
    np->subscribechar = *s;		/* salt away the : or ! */
    np->numoffset = len + 1;		/* remember where the numbers are */
    *s = '\0';			/* null terminate newsgroup name */
}

void abandon_ng(NGDATA *np)
{
    char* some_buf = nullptr;
    FILE* rcfp;

    /* open newsrc backup copy and try to find the prior value for the group. */
    if ((rcfp = fopen(np->rc->oldname, "r")) != nullptr) {
	int length = np->numoffset - 1;

	while ((some_buf = get_a_line(buf, LBUFLEN,false,rcfp)) != nullptr) {
	    if (len_last_line_got <= 0)
		continue;
	    some_buf[len_last_line_got-1] = '\0';	/* wipe out newline */
	    if ((some_buf[length] == ':' || some_buf[length] == NEGCHAR)
	     && !strncmp(np->rcline, some_buf, length)) {
		break;
	    }
	    if (some_buf != buf)
		free(some_buf);
	}
	fclose(rcfp);
    } else if (errno != ENOENT) {
	printf("Unable to open %s.\n", np->rc->oldname) FLUSH;
	termdown(1);
	return;
    }
    if (some_buf == nullptr) {
	some_buf = np->rcline + np->numoffset;
	if (*some_buf == ' ')
	    some_buf++;
	*some_buf = '\0';
	np->abs1st = 0;		/* force group to be re-calculated */
    }
    else {
	free(np->rcline);
	if (some_buf == buf)
	    np->rcline = savestr(some_buf);
	else {
	    /*NOSTRICT*/
#ifndef lint
	    some_buf = saferealloc(some_buf, (MEM_SIZE)(len_last_line_got));
#endif /* lint */
	    np->rcline = some_buf;
	}
    }
    parse_rcline(np);
    if (np->subscribechar == NEGCHAR)
	np->subscribechar = ':';
    np->rc->flags |= RF_RCCHANGED;
    set_toread(np, ST_LAX);
}

/* try to find or add an explicitly specified newsgroup */
/* returns true if found or added, false if not. */
/* assumes that we are chdir'ed to NEWSSPOOL */

bool get_ng(const char *what, int flags)
{
    char* ntoforget;
    char promptbuf[128];
    int autosub;

    if (verbose)
	ntoforget = "Type n to forget about this newsgroup.\n";
    else
	ntoforget = "n to forget it.\n";
    if (strchr(what,'/')) {
	dingaling();
	printf("\nBad newsgroup name.\n") FLUSH;
	termdown(2);
      check_fuzzy_match:
	if (fuzzyGet && (flags & GNG_FUZZY)) {
	    flags &= ~GNG_FUZZY;
	    if (find_close_match())
		what = ngname;
	    else
		return false;
	} else
	    return false;
    }
    set_ngname(what);
    g_ngptr = find_ng(ngname);
    if (g_ngptr == nullptr) {		/* not in .newsrc? */
	NEWSRC* rp;
	for (rp = g_multirc->first; rp; rp = rp->next) {
	    if (!ALLBITS(rp->flags, RF_ADD_GROUPS | RF_ACTIVE))
		continue;
	    /*$$ this may scan a datasrc multiple times... */
	    if (find_actgrp(rp->datasrc,buf,ngname,ngname_len,(ART_NUM)0))
		break;  /*$$ let them choose which server */
	}
	if (!rp) {
	    dingaling();
	    if (verbose)
		printf("\nNewsgroup %s does not exist!\n",ngname) FLUSH;
	    else
		printf("\nNo %s!\n",ngname) FLUSH;
	    termdown(2);
	    if (novice_delays)
		sleep(2);
	    goto check_fuzzy_match;
	}
	if (mode != 'i' || !(autosub = auto_subscribe(ngname)))
	    autosub = g_addnewbydefault;
	if (autosub) {
	    if (append_unsub) {
		printf("(Adding %s to end of your .newsrc %ssubscribed)\n",
		       ngname, (autosub == ADDNEW_SUB)? "" : "un") FLUSH;
		termdown(1);
		g_ngptr = add_newsgroup(rp, ngname, autosub);
	    } else {
		if (autosub == ADDNEW_SUB) {
		    printf("(Subscribing to %s)\n", ngname) FLUSH;
		    termdown(1);
		    g_ngptr = add_newsgroup(rp, ngname, autosub);
		} else {
		    printf("(Ignoring %s)\n", ngname) FLUSH;
		    termdown(1);
		    return false;
		}
	    }
	    flags &= ~GNG_RELOC;
	} else {
	    if (verbose)
		sprintf(promptbuf,"\nNewsgroup %s not in .newsrc -- subscribe?",ngname);
	    else
		sprintf(promptbuf,"\nSubscribe %s?",ngname);
reask_add:
	    in_char(promptbuf,'A',"ynYN");
	    printcmd();
	    newline();
	    if (*buf == 'h') {
		if (verbose) {
		    printf("Type y or SP to subscribe to %s.\n\
Type Y to subscribe to this and all remaining new groups.\n\
Type N to leave all remaining new groups unsubscribed.\n", ngname) FLUSH;
		    termdown(3);
		}
		else
		{
		    fputs("\
y or SP to subscribe, Y to subscribe all new groups, N to unsubscribe all\n",
			  stdout) FLUSH;
		    termdown(1);
		}
		fputs(ntoforget,stdout) FLUSH;
		termdown(1);
		goto reask_add;
	    }
	    else if (*buf == 'n' || *buf == 'q') {
		if (append_unsub)
		    g_ngptr = add_newsgroup(rp, ngname, NEGCHAR);
		return false;
	    }
	    else if (*buf == 'y') {
		g_ngptr = add_newsgroup(rp, ngname, ':');
		flags |= GNG_RELOC;
	    }
	    else if (*buf == 'Y') {
		g_addnewbydefault = ADDNEW_SUB;
		if (append_unsub)
		    printf("(Adding %s to end of your .newsrc subscribed)\n",
			   ngname) FLUSH;
		else
		    printf("(Subscribing to %s)\n", ngname) FLUSH;
		termdown(1);
		g_ngptr = add_newsgroup(rp, ngname, ':');
		flags &= ~GNG_RELOC;
	    }
	    else if (*buf == 'N') {
		g_addnewbydefault = ADDNEW_UNSUB;
		if (append_unsub) {
		    printf("(Adding %s to end of your .newsrc unsubscribed)\n",
			   ngname) FLUSH;
		    termdown(1);
		    g_ngptr = add_newsgroup(rp, ngname, NEGCHAR);
		    flags &= ~GNG_RELOC;
		} else {
		    printf("(Ignoring %s)\n", ngname) FLUSH;
		    termdown(1);
		    return false;
		}
	    }
	    else {
		fputs(hforhelp,stdout) FLUSH;
		termdown(1);
		settle_down();
		goto reask_add;
	    }
	}
    }
    else if (mode == 'i')		/* adding new groups during init? */
	return false;
    else if (g_ngptr->subscribechar == NEGCHAR) {/* unsubscribed? */
	if (verbose)
	    sprintf(promptbuf,
"\nNewsgroup %s is unsubscribed -- resubscribe?",ngname)
  FLUSH;
	else
	    sprintf(promptbuf,"\nResubscribe %s?",ngname)
	      FLUSH;
reask_unsub:
	in_char(promptbuf,'R',"yn");
	printcmd();
	newline();
	if (*buf == 'h') {
	    if (verbose)
		printf("Type y or SP to resubscribe to %s.\n", ngname) FLUSH;
	    else
		fputs("y or SP to resubscribe.\n",stdout) FLUSH;
	    fputs(ntoforget,stdout) FLUSH;
	    termdown(2);
	    goto reask_unsub;
	}
	else if (*buf == 'n' || *buf == 'q') {
	    return false;
	}
	else if (*buf == 'y') {
	    char* cp;
	    cp = g_ngptr->rcline + g_ngptr->numoffset;
	    g_ngptr->flags = (*cp && cp[1] == '0' ? NF_UNTHREADED : 0);
	    g_ngptr->subscribechar = ':';
	    g_ngptr->rc->flags |= RF_RCCHANGED;
	    flags &= ~GNG_RELOC;
	}
	else {
	    fputs(hforhelp,stdout) FLUSH;
	    termdown(1);
	    settle_down();
	    goto reask_unsub;
	}
    }

    /* now calculate how many unread articles in newsgroup */

    set_toread(g_ngptr, ST_STRICT);
    if (flags & GNG_RELOC) {
	if (!relocate_newsgroup(g_ngptr,-1))
	    return false;
    }
    return g_ngptr->toread >= TR_NONE;
}

/* add a newsgroup to the newsrc file (eventually) */

static NGDATA *add_newsgroup(NEWSRC *rp, char *ngn, char_int c)
{
    NGDATA* np;

    np = ngdata_ptr(g_ngdata_cnt++);
    np->prev = g_last_ng;
    if (g_last_ng)
	g_last_ng->next = np;
    else
	g_first_ng = np;
    np->next = nullptr;
    g_last_ng = np;
    g_newsgroup_cnt++;

    np->rc = rp;
    np->numoffset = strlen(ngn) + 1;
    np->rcline = safemalloc((MEM_SIZE)(np->numoffset + 2));
    strcpy(np->rcline,ngn);		/* and copy over the name */
    strcpy(np->rcline + np->numoffset, " ");
    np->subscribechar = c;		/* subscribe or unsubscribe */
    if (c != NEGCHAR)
	g_newsgroup_toread++;
    np->toread = TR_NONE;		/* just for prettiness */
    sethash(np);			/* so we can find it again */
    rp->flags |= RF_RCCHANGED;
    return np;
}

bool relocate_newsgroup(NGDATA *move_np, NG_NUM newnum)
{
    NGDATA* np;
    int i;
    const char* dflt = (move_np!=g_current_ng ? "$^.Lq" : "$^Lq");
    int save_sort = sel_sort;

    if (sel_newsgroupsort != SS_NATURAL) {
	if (newnum < 0) {
	    /* ask if they want to keep the current order */
	    in_char("Sort newsrc(s) using current sort order?", 'D', "yn"); /*$$ !'D' */
	    printcmd();
	    newline();
	    if (*buf == 'y')
		set_selector(SM_NEWSGROUP, SS_NATURAL);
	    else {
		sel_sort = SS_NATURAL;
		sel_direction = 1;
		sort_newsgroups();
	    }
	}
	else {
	    sel_sort = SS_NATURAL;
	    sel_direction = 1;
	    sort_newsgroups();
	}
    }

    g_starthere = nullptr;			/* Disable this optimization */
    if (move_np != g_last_ng) {
	if (move_np->prev)
	    move_np->prev->next = move_np->next;
	else
	    g_first_ng = move_np->next;
	move_np->next->prev = move_np->prev;

	move_np->prev = g_last_ng;
	move_np->next = nullptr;
	g_last_ng->next = move_np;
	g_last_ng = move_np;
    }

    /* Renumber the groups according to current order */
    for (np = g_first_ng, i = 0; np; np = np->next, i++)
	np->num = i;
    move_np->rc->flags |= RF_RCCHANGED;

    if (newnum < 0) {
      reask_reloc:
	unflush_output();		/* disable any ^O in effect */
	if (verbose)
	    printf("\nPut newsgroup where? [%s] ", dflt);
	else
	    printf("\nPut where? [%s] ", dflt);
	fflush(stdout);
	termdown(1);
      reinp_reloc:
	eat_typeahead();
	getcmd(buf);
	if (errno || *buf == '\f')	/* if return from stop signal */
	    goto reask_reloc;		/* give them a prompt again */
	setdef(buf,dflt);
	printcmd();
	if (*buf == 'h') {
	    if (verbose) {
		printf("\n\n\
Type ^ to put the newsgroup first (position 0).\n\
Type $ to put the newsgroup last (position %d).\n", g_newsgroup_cnt-1);
		printf("\
Type . to put it before the current newsgroup.\n");
		printf("\
Type -newsgroup name to put it before that newsgroup.\n\
Type +newsgroup name to put it after that newsgroup.\n\
Type a number between 0 and %d to put it at that position.\n", g_newsgroup_cnt-1);
		printf("\
Type L for a listing of newsgroups and their positions.\n\
Type q to abort the current action.\n") FLUSH;
	    }
	    else
	    {
		printf("\n\n\
^ to put newsgroup first (pos 0).\n\
$ to put last (pos %d).\n", g_newsgroup_cnt-1);
		printf("\
. to put before current newsgroup.\n");
		printf("\
-newsgroup to put before newsgroup.\n\
+newsgroup to put after.\n\
number in 0-%d to put at that pos.\n", g_newsgroup_cnt-1);
		printf("\
L for list of newsrc.\n\
q to abort\n") FLUSH;
	    }
	    termdown(10);
	    goto reask_reloc;
	}
	else if (*buf == 'q')
	    return false;
	else if (*buf == 'L') {
	    newline();
	    list_newsgroups();
	    goto reask_reloc;
	}
	else if (isdigit(*buf)) {
	    if (!finish_command(true))	/* get rest of command */
		goto reinp_reloc;
	    newnum = atol(buf);
	    if (newnum < 0)
		newnum = 0;
	    if (newnum >= g_newsgroup_cnt)
		newnum = g_newsgroup_cnt-1;
	}
	else if (*buf == '^') {
	    newline();
	    newnum = 0;
	}
	else if (*buf == '$') {
	    newnum = g_newsgroup_cnt-1;
	}
	else if (*buf == '.') {
	    newline();
	    newnum = g_current_ng->num;
	}
	else if (*buf == '-' || *buf == '+') {
	    if (!finish_command(true))	/* get rest of command */
		goto reinp_reloc;
	    np = find_ng(buf+1);
	    if (np == nullptr) {
		fputs("Not found.",stdout) FLUSH;
		goto reask_reloc;
	    }
	    newnum = np->num;
	    if (*buf == '+')
		newnum++;
	}
	else {
	    printf("\n%s",hforhelp) FLUSH;
	    termdown(2);
	    settle_down();
	    goto reask_reloc;
	}
    }
    if (newnum < g_newsgroup_cnt-1) {
	for (np = g_first_ng; np; np = np->next)
	    if (np->num >= newnum)
		break;
	if (!np || np == move_np)
	    return false;		/* This can't happen... */

	g_last_ng = move_np->prev;
	g_last_ng->next = nullptr;

	move_np->prev = np->prev;
	move_np->next = np;

	if (np->prev)
	    np->prev->next = move_np;
	else
	    g_first_ng = move_np;
	np->prev = move_np;

	move_np->num = newnum++;
	for (; np; np = np->next, newnum++)
	    np->num = newnum;
    }
    if (sel_newsgroupsort != SS_NATURAL) {
	sel_sort = sel_newsgroupsort;
	sort_newsgroups();
	sel_sort = save_sort;
    }
    return true;
}

/* List out the newsrc with annotations */

void list_newsgroups()
{
    NGDATA* np;
    NG_NUM i;
    char tmpbuf[2048];
    static char* status[] = {"(READ)","(UNSUB)","(DUP)","(BOGUS)","(JUNK)"};

    page_start();
    print_lines("  #  Status  Newsgroup\n", STANDOUT);
    for (np = g_first_ng, i = 0; np && !g_int_count; np = np->next, i++) {
	if (np->toread >= 0)
	    set_toread(np, ST_LAX);
	*(np->rcline + np->numoffset - 1) = np->subscribechar;
	if (np->toread > 0)
	    sprintf(tmpbuf,"%3d %6ld   ",i,(long)np->toread);
	else
	    sprintf(tmpbuf,"%3d %7s  ",i,status[-np->toread]);
	safecpy(tmpbuf+13, np->rcline, sizeof tmpbuf - 13);
	*(np->rcline + np->numoffset - 1) = '\0';
	if (print_lines(tmpbuf, NOMARKING) != 0)
	    break;
    }
    g_int_count = 0;
}

/* find a newsgroup in any newsrc */

NGDATA *find_ng(const char *ngnam)
{
    HASHDATUM data;

    data = hashfetch(g_newsrc_hash, ngnam, strlen(ngnam));
    return (NGDATA*)data.dat_ptr;
}

void cleanup_newsrc(NEWSRC *rp)
{
    NGDATA* np;
    NG_NUM bogosity = 0;

    if (verbose)
	printf("Checking out '%s' -- hang on a second...\n",rp->name) FLUSH;
    else
	printf("Checking '%s' -- hang on...\n",rp->name) FLUSH;
    termdown(1);
    for (np = g_first_ng; np; np = np->next) {
/*#ifdef CHECK_ALL_BOGUS $$ what is this? */
	if (np->toread >= TR_UNSUB)
	    set_toread(np, ST_LAX); /* this may reset the group or declare it bogus */
/*#endif*/
	if (np->toread == TR_BOGUS)
	    bogosity++;
    }
    for (np = g_last_ng; np && np->toread == TR_BOGUS; np = np->prev)
	bogosity--;			/* discount already moved ones */
    if (g_newsgroup_cnt > 5 && bogosity > g_newsgroup_cnt / 2) {
	fputs(
"It looks like the active file is messed up.  Contact your news administrator,\n\
",stdout);
	fputs(
"leave the \"bogus\" groups alone, and they may come back to normal.  Maybe.\n\
",stdout) FLUSH;
	termdown(2);
    }
    else if (bogosity) {
	NGDATA* prev_np;
	if (verbose)
	    printf("Moving bogus newsgroups to the end of '%s'.\n",rp->name) FLUSH;
	else
	    fputs("Moving boguses to the end.\n",stdout) FLUSH;
	termdown(1);
	while (np) {
	    prev_np = np->prev;
	    if (np->toread == TR_BOGUS)
		relocate_newsgroup(np, g_newsgroup_cnt-1);
	    np = prev_np;
	}
	rp->flags |= RF_RCCHANGED;
reask_bogus:
	in_char("Delete bogus newsgroups?", 'D', "ny");
	printcmd();
	newline();
	if (*buf == 'h') {
	    if (verbose) {
		fputs("\
Type y to delete bogus newsgroups.\n\
Type n or SP to leave them at the end in case they return.\n\
",stdout) FLUSH;
		termdown(2);
	    }
	    else
	    {
		fputs("y to delete, n to keep\n",stdout) FLUSH;
		termdown(1);
	    }
	    goto reask_bogus;
	}
	else if (*buf == 'n' || *buf == 'q')
	    ;
	else if (*buf == 'y') {
	    for (np = g_last_ng; np && np->toread == TR_BOGUS; np = np->prev) {
		hashdelete(g_newsrc_hash, np->rcline, np->numoffset - 1);
		clear_ngitem((char*)np,0);
		g_newsgroup_cnt--;
	    }
	    rp->flags |= RF_RCCHANGED; /*$$ needed? */
	    g_last_ng = np;
	    if (np)
		np->next = nullptr;
	    else
		g_first_ng = nullptr;
	    if (g_current_ng && !g_current_ng->rcline)
		g_current_ng = g_first_ng;
	    if (g_recent_ng && !g_recent_ng->rcline)
		g_recent_ng = g_first_ng;
	    if (g_ngptr && !g_ngptr->rcline)
		g_ngptr = g_first_ng;
	    if (g_sel_page_np && !g_sel_page_np->rcline)
		g_sel_page_np = nullptr;
	}
	else {
	    fputs(hforhelp,stdout) FLUSH;
	    termdown(1);
	    settle_down();
	    goto reask_bogus;
	}
    }
    g_paranoid = false;
}

/* make an entry in the hash table for the current newsgroup */

void sethash(NGDATA *np)
{
    HASHDATUM data;
    data.dat_ptr = (char*)np;
    data.dat_len = np->numoffset - 1;
    hashstore(g_newsrc_hash, np->rcline, data.dat_len, data);
}

static int rcline_cmp(const char *key, int keylen, HASHDATUM data)
{
    /* We already know that the lengths are equal, just compare the strings */
    return memcmp(key, ((NGDATA*)data.dat_ptr)->rcline, keylen);
}

/* checkpoint the newsrc(s) */

void checkpoint_newsrcs()
{
#ifdef DEBUG
    if (debug & DEB_CHECKPOINTING) {
	fputs("(ckpt)",stdout);
	fflush(stdout);
    }
#endif
    if (g_doing_ng)
	bits_to_rc();			/* do not restore M articles */
    if (!write_newsrcs(g_multirc))
	get_anything();
#ifdef DEBUG
    if (debug & DEB_CHECKPOINTING) {
	fputs("(done)",stdout);
	fflush(stdout);
    }
#endif
}

/* write out the (presumably) revised newsrc(s) */

bool write_newsrcs(MULTIRC *mptr)
{
    NEWSRC* rp;
    NGDATA* np;
    int save_sort = sel_sort;
    FILE* rcfp;
    bool total_success = true;

    if (!mptr)
	return true;

    if (sel_newsgroupsort != SS_NATURAL) {
	sel_sort = SS_NATURAL;
	sel_direction = 1;
	sort_newsgroups();
    }

    for (rp = mptr->first; rp; rp = rp->next) {
	if (!(rp->flags & RF_ACTIVE))
	    continue;

	if (rp->infoname) {
	    if ((tmpfp = fopen(rp->infoname, "w")) != nullptr) {
		fprintf(tmpfp,"Last-Group: %s\nNew-Group-State: %ld,%ld,%ld\n",
			ngname? ngname : "",rp->datasrc->lastnewgrp,
			rp->datasrc->act_sf.recent_cnt,
			rp->datasrc->desc_sf.recent_cnt);
		fclose(tmpfp);
	    }
	}
	else {
	    readlast();
	    if (rp->datasrc->flags & DF_REMOTE) {
		g_lastactsiz = rp->datasrc->act_sf.recent_cnt;
		g_lastextranum = rp->datasrc->desc_sf.recent_cnt;
	    }
	    else
		g_lastextranum = rp->datasrc->act_sf.recent_cnt;
	    g_lastnewtime = rp->datasrc->lastnewgrp;
	    writelast();
	}

	if (!(rp->flags & RF_RCCHANGED))
	    continue;

	rcfp = fopen(rp->newname, "w");
	if (rcfp == nullptr) {
	    printf(cantrecreate,rp->name) FLUSH;
	    total_success = false;
	    continue;
	}
#ifndef MSDOS
	if (stat(rp->name,&filestat)>=0) { /* preserve permissions */
	    chmod(rp->newname,filestat.st_mode&0666);
	    chown(rp->newname,filestat.st_uid,filestat.st_gid);
	}
#endif
	/* write out each line*/

	for (np = g_first_ng; np; np = np->next) {
	    char* delim;
	    if (np->rc != rp)
		continue;
	    if (np->numoffset) {
		delim = np->rcline + np->numoffset - 1;
		*delim = np->subscribechar;
		if ((np->flags & NF_UNTHREADED) && delim[2] == '1')
		    delim[2] = '0';
	    }
	    else
		delim = nullptr;
#ifdef DEBUG
	    if (debug & DEB_NEWSRC_LINE) {
		printf("%s\n",np->rcline) FLUSH;
		termdown(1);
	    }
#endif
	    if (fprintf(rcfp,"%s\n",np->rcline) < 0) {
		fclose(rcfp);		/* close new newsrc */
		goto write_error;
	    }
	    if (delim) {
		*delim = '\0';		/* might still need this line */
		if ((np->flags & NF_UNTHREADED) && delim[2] == '0')
		    delim[2] = '1';
	    }
	}
	fflush(rcfp);
	/* fclose is the only sure test for full disks via NFS */
	if (ferror(rcfp)) {
	    fclose(rcfp);
	    goto write_error;
	}
	if (fclose(rcfp) == EOF) {
	  write_error:
	    printf(cantrecreate,rp->name) FLUSH;
	    remove(rp->newname);
	    total_success = false;
	    continue;
	}
	rp->flags &= ~RF_RCCHANGED;

	remove(rp->name);
	rename(rp->newname,rp->name);
    }

    if (sel_newsgroupsort != SS_NATURAL) {
	sel_sort = sel_newsgroupsort;
	sort_newsgroups();
	sel_sort = save_sort;
    }
    return total_success;
}

void get_old_newsrcs(MULTIRC *mptr)
{
    NEWSRC* rp;

    if (mptr) {
	for (rp = mptr->first; rp; rp = rp->next) {
	    if (rp->flags & RF_ACTIVE) {
		remove(rp->newname);
		rename(rp->name,rp->newname);
		rename(rp->oldname,rp->name);
	    }
	}
    }
}
