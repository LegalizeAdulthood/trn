/* datasrc.c
 * vi: set sw=4 ts=8 ai sm noet :
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "trn.h"
#include "hash.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "term.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "rcstuff.h"
#include "edit_dist.h"
#include "rt-mt.h"
#include "rt-ov.h"
#include "rt-util.h"
#include "nntp.h"
#include "datasrc.h"

#ifdef MSDOS
#include <io.h>
#endif
#ifdef I_UTIME
#include <utime.h>
#endif
#ifdef I_SYS_UTIME
#include <sys/utime.h>
#endif
#if !defined(I_UTIME) && !defined(I_SYS_UTIME)
struct utimbuf
{
    time_t actime;
    time_t modtime;
};
#endif

LIST *g_datasrc_list{}; /* a list of all DATASRCs */
DATASRC *g_datasrc{};   /* the current datasrc */
int g_datasrc_cnt{};
char *g_trnaccess_mem{};
char *g_nntp_auth_file{};

enum
{
    SRCFILE_CHUNK_SIZE = (32 * 1024),

    DI_NNTP_SERVER = 1,
    DI_ACTIVE_FILE = 2,
    DI_ACT_REFETCH = 3,
    DI_SPOOL_DIR = 4,
    DI_THREAD_DIR = 5,
    DI_OVERVIEW_DIR = 6,
    DI_ACTIVE_TIMES = 7,
    DI_GROUP_DESC = 8,
    DI_DESC_REFETCH = 9,
    DI_AUTH_USER = 10,
    DI_AUTH_PASS = 11,
    DI_AUTH_COMMAND = 12,
    DI_XHDR_BROKEN = 13,
    DI_XREFS = 14,
    DI_OVERVIEW_FMT = 15,
    DI_FORCE_AUTH = 16
};

static INI_WORDS s_datasrc_ini[] = {
    { 0, "DATASRC", nullptr },
    { 0, "NNTP Server", nullptr },
    { 0, "Active File", nullptr },
    { 0, "Active File Refetch", nullptr },
    { 0, "Spool Dir", nullptr },
    { 0, "Thread Dir", nullptr },
    { 0, "Overview Dir", nullptr },
    { 0, "Active Times", nullptr },
    { 0, "Group Desc", nullptr },
    { 0, "Group Desc Refetch", nullptr },
    { 0, "Auth User", nullptr },
    { 0, "Auth Password", nullptr },
    { 0, "Auth Command", nullptr },
    { 0, "XHDR Broken", nullptr },
    { 0, "Xrefs", nullptr },
    { 0, "Overview Format File", nullptr },
    { 0, "Force Auth", nullptr },
    { 0, nullptr, nullptr }
};

static char *dir_or_none(DATASRC *dp, char *dir, int flag);
static char *file_or_none(char *fn);
static int srcfile_cmp(const char *key, int keylen, HASHDATUM data);
static int check_distance(int len, HASHDATUM *data, int newsrc_ptr);
static int get_near_miss();

void datasrc_init()
{
    char** vals = prep_ini_words(s_datasrc_ini);
    char* machine = nullptr;
    char* actname = nullptr;
    char* s;

    g_datasrc_list = new_list(0,0,sizeof(DATASRC),20,LF_ZERO_MEM,nullptr);

    g_nntp_auth_file = savestr(filexp(NNTP_AUTH_FILE));

    machine = getenv("NNTPSERVER");
    if (machine && strcmp(machine,"local")) {
	vals[DI_NNTP_SERVER] = machine;
	vals[DI_AUTH_USER] = read_auth_file(g_nntp_auth_file,
					    &vals[DI_AUTH_PASS]);
	vals[DI_FORCE_AUTH] = getenv("NNTP_FORCE_AUTH");
	new_datasrc("default",vals);
    }

    g_trnaccess_mem = read_datasrcs(TRNACCESS);
    s = read_datasrcs(DEFACCESS);
    if (!g_trnaccess_mem)
	g_trnaccess_mem = s;
    else if (s)
	free(s);

    if (!machine) {
	machine = filexp(SERVER_NAME);
	if (FILE_REF(machine))
	    machine = nntp_servername(machine);
	if (!strcmp(machine,"local")) {
	    machine = nullptr;
	    actname = ACTIVE;
	}
	prep_ini_words(s_datasrc_ini);	/* re-zero the values */

	vals[DI_NNTP_SERVER] = machine;
	vals[DI_ACTIVE_FILE] = actname;
	vals[DI_SPOOL_DIR] = NEWSSPOOL;
	vals[DI_THREAD_DIR] = THREAD_DIR;
	vals[DI_OVERVIEW_DIR] = OVERVIEW_DIR;
	vals[DI_OVERVIEW_FMT] = OVERVIEW_FMT;
	vals[DI_ACTIVE_TIMES] = ACTIVE_TIMES;
	vals[DI_GROUP_DESC] = GROUPDESC;
	if (machine) {
	    vals[DI_AUTH_USER] = read_auth_file(g_nntp_auth_file,
						&vals[DI_AUTH_PASS]);
	    vals[DI_FORCE_AUTH] = getenv("NNTP_FORCE_AUTH");
	}
	new_datasrc("default",vals);
    }
    unprep_ini_words(s_datasrc_ini);
}

char *read_datasrcs(char *filename)
{
    int fd;
    char* s;
    char* section;
    char* cond;
    char* filebuf = nullptr;
    char** vals = INI_VALUES(s_datasrc_ini);

    if ((fd = open(filexp(filename),0)) >= 0) {
	fstat(fd,&filestat);
	if (filestat.st_size) {
	    int len;
	    filebuf = safemalloc((MEM_SIZE)filestat.st_size+2);
	    len = read(fd,filebuf,(int)filestat.st_size);
	    (filebuf)[len] = '\0';
	    prep_ini_data(filebuf,filename);
	    s = filebuf;
	    while ((s = next_ini_section(s,&section,&cond)) != nullptr) {
		if (*cond && !check_ini_cond(cond))
		    continue;
		if (!strncasecmp(section, "group ", 6))
		    continue;
		s = parse_ini_section(s, s_datasrc_ini);
		if (!s)
		    break;
		new_datasrc(section,vals);
	    }
	}
	close(fd);
    }
    return filebuf;
}

DATASRC *get_datasrc(const char *name)
{
    DATASRC* dp;
    for (dp = datasrc_first(); dp && dp->name; dp = datasrc_next(dp))
	if (!strcmp(dp->name,name))
	    return dp;
    return nullptr;
}

DATASRC *new_datasrc(const char *name, char **vals)
{
    DATASRC* dp = datasrc_ptr(g_datasrc_cnt++);
    char* v;

    if (vals[DI_NNTP_SERVER]) {
	dp->flags |= DF_REMOTE;
    }
    else if (!vals[DI_ACTIVE_FILE])
	return nullptr; /*$$*/

    dp->name = savestr(name);
    if (!strcmp(name,"default"))
	dp->flags |= DF_DEFAULT;

    if ((v = vals[DI_NNTP_SERVER]) != nullptr) {
	char* cp;
	dp->newsid = savestr(v);
	if ((cp = strchr(dp->newsid, ';')) != nullptr) {
	    *cp = '\0';
	    dp->nntplink.port_number = atoi(cp+1);
	}

	if ((v = vals[DI_ACT_REFETCH]) != nullptr && *v)
	    dp->act_sf.refetch_secs = text2secs(v,defRefetchSecs);
	else if (!vals[DI_ACTIVE_FILE])
	    dp->act_sf.refetch_secs = defRefetchSecs;
    }
    else
	dp->newsid = savestr(filexp(vals[DI_ACTIVE_FILE]));

    if (!(dp->spool_dir = file_or_none(vals[DI_SPOOL_DIR])))
	dp->spool_dir = savestr(g_tmp_dir);

    dp->over_dir = dir_or_none(dp,vals[DI_OVERVIEW_DIR],DF_TRY_OVERVIEW);
    dp->over_fmt = file_or_none(vals[DI_OVERVIEW_FMT]);
    dp->thread_dir = dir_or_none(dp,vals[DI_THREAD_DIR],DF_TRY_THREAD);
    dp->grpdesc = dir_or_none(dp,vals[DI_GROUP_DESC],0);
    dp->extra_name = dir_or_none(dp,vals[DI_ACTIVE_TIMES],DF_ADD_OK);
    if (dp->flags & DF_REMOTE) {
	/* FYI, we know extra_name to be nullptr in this case. */
	if (vals[DI_ACTIVE_FILE]) {
	    dp->extra_name = savestr(filexp(vals[DI_ACTIVE_FILE]));
	    if (stat(dp->extra_name,&filestat) >= 0)
		dp->act_sf.lastfetch = filestat.st_mtime;
	}
	else {
	    dp->extra_name = temp_filename();
	    dp->flags |= DF_TMPACTFILE;
	    if (!dp->act_sf.refetch_secs)
		dp->act_sf.refetch_secs = 1;
	}

	if ((v = vals[DI_DESC_REFETCH]) != nullptr && *v)
	    dp->desc_sf.refetch_secs = text2secs(v,defRefetchSecs);
	else if (!dp->grpdesc)
	    dp->desc_sf.refetch_secs = defRefetchSecs;
	if (dp->grpdesc) {
	    if (stat(dp->grpdesc,&filestat) >= 0)
		dp->desc_sf.lastfetch = filestat.st_mtime;
	}
	else {
	    dp->grpdesc = temp_filename();
	    dp->flags |= DF_TMPGRPDESC;
	    if (!dp->desc_sf.refetch_secs)
		dp->desc_sf.refetch_secs = 1;
	}
    }
    if ((v = vals[DI_FORCE_AUTH]) != nullptr && (*v == 'y' || *v == 'Y'))
	dp->nntplink.flags |= NNTP_FORCE_AUTH_NEEDED;
    if ((v = vals[DI_AUTH_USER]) != nullptr)
	dp->auth_user = savestr(v);
    if ((v = vals[DI_AUTH_PASS]) != nullptr)
	dp->auth_pass = savestr(v);
    if ((v = vals[DI_XHDR_BROKEN]) != nullptr && (*v == 'y' || *v == 'Y'))
	dp->flags |= DF_XHDR_BROKEN;
    if ((v = vals[DI_XREFS]) != nullptr && (*v == 'n' || *v == 'N'))
	dp->flags |= DF_NOXREFS;

    return dp;
}

static char *dir_or_none(DATASRC *dp, char *dir, int flag)
{
    if (!dir || !*dir || !strcmp(dir,"remote")) {
	dp->flags |= flag;
	if (dp->flags & DF_REMOTE)
	    return nullptr;
	if (flag == DF_ADD_OK) {
	    char* cp = safemalloc(strlen(dp->newsid)+6+1);
	    sprintf(cp,"%s.times",dp->newsid);
	    return cp;
	}
	if (flag == 0) {
	    char* cp = strrchr(dp->newsid,'/');
	    int len;
	    if (!cp)
		return nullptr;
	    len = cp - dp->newsid + 1;
	    cp = safemalloc(len+10+1);
	    strcpy(cp,dp->newsid);
	    strcpy(cp+len,"newsgroups");
	    return cp;
	}
	return dp->spool_dir;
    }

    if (!strcmp(dir,"none"))
	return nullptr;

    dp->flags |= flag;
    dir = filexp(dir);
    if (!strcmp(dir,dp->spool_dir))
	return dp->spool_dir;
    return savestr(dir);
}

static char *file_or_none(char *fn)
{
    if (!fn || !*fn || !strcmp(fn,"none") || !strcmp(fn,"remote"))
	return nullptr;
    return savestr(filexp(fn));
}

bool open_datasrc(DATASRC *dp)
{
    bool success;

    if (dp->flags & DF_UNAVAILABLE)
	return false;
    set_datasrc(dp);
    if (dp->flags & DF_OPEN)
	return true;
    if (dp->flags & DF_REMOTE) {
	if (nntp_connect(dp->newsid,true) <= 0) {
	    dp->flags |= DF_UNAVAILABLE;
	    return false;
	}
	nntp_allow_timeout = false;
	dp->nntplink = nntplink;
	if (dp->act_sf.refetch_secs) {
	    switch (nntp_list("active", "control", 7)) {
	    case 1:
		if (strncmp(g_ser_line, "control ", 8)) {
		    strcpy(buf, g_ser_line);
		    dp->act_sf.lastfetch = 0;
		    success = actfile_hash(dp);
		    break;
		}
		if (nntp_gets(buf, sizeof buf - 1) > 0
		 && !nntp_at_list_end(buf)) {
		    nntp_finish_list();
		    success = actfile_hash(dp);
		    break;
		}
		/* FALL THROUGH */
	    case 0:
		dp->flags |= DF_USELISTACT;
		if (dp->flags & DF_TMPACTFILE) {
		    dp->flags &= ~DF_TMPACTFILE;
		    free(dp->extra_name);
		    dp->extra_name = nullptr;
		    dp->act_sf.refetch_secs = 0;
		    success = srcfile_open(&dp->act_sf,nullptr,
                                           nullptr,nullptr);
		}
		else
		    success = actfile_hash(dp);
		break;
	    case -2:
		printf("Failed to open news server %s:\n%s\n", dp->newsid, g_ser_line);
		termdown(2);
		success = false;
		break;
	    default:
		success = actfile_hash(dp);
		break;
	    }
	} else
	    success = actfile_hash(dp);
    }
    else
	success = actfile_hash(dp);
    if (success) {
	dp->flags |= DF_OPEN;
	if (dp->flags & DF_TRY_OVERVIEW)
	    ov_init();
	if (dp->flags & DF_TRY_THREAD)
	    mt_init();
    }
    else
	dp->flags |= DF_UNAVAILABLE;
    if (dp->flags & DF_REMOTE)
	nntp_allow_timeout = true;
    return success;
}

void set_datasrc(DATASRC *dp)
{
    if (g_datasrc)
	g_datasrc->nntplink = nntplink;
    if (dp)
	nntplink = dp->nntplink;
    g_datasrc = dp;
}

void check_datasrcs()
{
    DATASRC* dp;
    time_t now = time((time_t*)nullptr);
    time_t limit;

    if (g_datasrc_list) {
	for (dp = datasrc_first(); dp && dp->name; dp = datasrc_next(dp)) {
	    if ((dp->flags & DF_OPEN) && dp->nntplink.rd_fp != nullptr) {
		limit = ((dp->flags & DF_ACTIVE)? 30*60 : 10*60);
		if (now - dp->nntplink.last_command > limit) {
		    DATASRC* save_datasrc = g_datasrc;
		    /*printf("\n*** Closing %s ***\n", dp->name); $$*/
		    set_datasrc(dp);
		    nntp_close(true);
		    dp->nntplink = nntplink;
		    set_datasrc(save_datasrc);
		}
	    }
	}
    }
}

void close_datasrc(DATASRC *dp)
{
    if (dp->flags & DF_REMOTE) {
	if (dp->flags & DF_TMPACTFILE)
	    remove(dp->extra_name);
	else
	    srcfile_end_append(&dp->act_sf, dp->extra_name);
	if (dp->grpdesc) {
	    if (dp->flags & DF_TMPGRPDESC)
		remove(dp->grpdesc);
	    else
		srcfile_end_append(&dp->desc_sf, dp->grpdesc);
	}
    }

    if (!(dp->flags & DF_OPEN))
	return;

    if (dp->flags & DF_REMOTE) {
	DATASRC* save_datasrc = g_datasrc;
	set_datasrc(dp);
	nntp_close(true);
	dp->nntplink = nntplink;
	set_datasrc(save_datasrc);
    }
    srcfile_close(&dp->act_sf);
    srcfile_close(&dp->desc_sf);
    dp->flags &= ~DF_OPEN;
    if (g_datasrc == dp)
	g_datasrc = nullptr;
}

bool actfile_hash(DATASRC *dp)
{
    int ret;
    if (dp->flags & DF_REMOTE) {
	DATASRC* save_datasrc = g_datasrc;
	set_datasrc(dp);
	g_spin_todo = dp->act_sf.recent_cnt;
	ret = srcfile_open(&dp->act_sf, dp->extra_name, "active", dp->newsid);
	if (g_spin_count > 0)
	    dp->act_sf.recent_cnt = g_spin_count;
	set_datasrc(save_datasrc);
    }
    else
	ret = srcfile_open(&dp->act_sf, dp->newsid, nullptr, nullptr);
    return ret;
}

bool find_actgrp(DATASRC *dp, char *outbuf, const char *nam, int len, ART_NUM high)
{
    HASHDATUM data;
    ACT_POS act_pos;
    FILE* fp = dp->act_sf.fp;
    char* lbp;
    int lbp_len;

    /* Do a quick, hashed lookup */

    outbuf[0] = '\0';
    data = hashfetch(dp->act_sf.hp, nam, len);
    if (data.dat_ptr) {
	LISTNODE* node = (LISTNODE*)data.dat_ptr;
	/*dp->act_sf.lp->recent = node;*/
	act_pos = node->low + data.dat_len;
	lbp = node->data + data.dat_len;
	lbp_len = strchr(lbp, '\n') - lbp + 1;
    }
    else {
	lbp = nullptr;
	lbp_len = 0;
    }
    if ((dp->flags & DF_USELISTACT)
     && (DATASRC_NNTP_FLAGS(dp) & NNTP_NEW_CMD_OK)) {
	DATASRC* save_datasrc = g_datasrc;
	set_datasrc(dp);
	switch (nntp_list("active", nam, len)) {
	case 0:
	    set_datasrc(save_datasrc);
	    return false;
	case 1:
	    sprintf(outbuf, "%s\n", g_ser_line);
	    nntp_finish_list();
	    break;
	case -2:
	    /*$$$$*/
	    break;
	}
	set_datasrc(save_datasrc);
	if (!lbp_len) {
	    if (fp)
		(void) srcfile_append(&dp->act_sf, outbuf, len);
	    return true;
	}
# ifndef ANCIENT_NEWS
	/* Safely update the low-water mark */
	{
	    char* f = strrchr(outbuf, ' ');
	    char* t = lbp + lbp_len;
	    while (*--t != ' ') ;
	    while (t > lbp) {
		if (*--t == ' ')
		    break;
		if (f[-1] == ' ')
		    *t = '0';
		else
		    *t = *--f;
	    }
	}
# endif
	high = (ART_NUM)atol(outbuf+len+1);
    }

    if (lbp_len) {
	if ((dp->flags & DF_REMOTE) && dp->act_sf.refetch_secs) {
	    int num;
	    char* cp;
	    if (high && high != (ART_NUM)atol(cp = lbp+len+1)) {
		while (isdigit(*cp)) cp++;
		while (*--cp != ' ') {
		    num = high % 10;
		    high = high / 10;
		    *cp = '0' + (char)num;
		}
		fseek(fp, act_pos, 0);
		fwrite(lbp, 1, lbp_len, fp);
	    }
	    goto use_cache;
	}

	/* hopefully this forces a reread */
	fseek(fp,2000000000L,1);

	/*$$ if line has changed length or is not there, we should
	 * discard/close the active file, and re-open it. $$*/
	if (fseek(fp, act_pos, 0) >= 0
	 && fgets(outbuf, LBUFLEN, fp) != nullptr
	 && !strncmp(outbuf, nam, len) && outbuf[len] == ' ') {
	    /* Remember the latest info in our cache. */
	    (void) memcpy(lbp,outbuf,lbp_len);
	    return true;
	}
      use_cache:
	/* Return our cached version */
	(void) memcpy(outbuf,lbp,lbp_len);
	outbuf[lbp_len] = '\0';
	return true;
    }
    return false;	/* no such group */
}

char *find_grpdesc(DATASRC *dp, char *groupname)
{
    HASHDATUM data;
    int grouplen;
    int ret;

    if (!dp->grpdesc)
	return "";

    if (!dp->desc_sf.hp) {
	if ((dp->flags & DF_REMOTE) && dp->desc_sf.refetch_secs) {
	    /*DATASRC* save_datasrc = g_datasrc;*/
	    set_datasrc(dp);
	    if ((dp->flags & (DF_TMPGRPDESC|DF_NOXGTITLE)) == DF_TMPGRPDESC
	     && g_net_speed < 5) {
		(void)srcfile_open(&dp->desc_sf,nullptr, /*$$check return?*/
                                   nullptr,nullptr);
		grouplen = strlen(groupname);
		goto try_xgtitle;
	    }
	    g_spin_todo = dp->desc_sf.recent_cnt;
	    ret = srcfile_open(&dp->desc_sf, dp->grpdesc,
                               "newsgroups", dp->newsid);
	    if (g_spin_count > 0)
		dp->desc_sf.recent_cnt = g_spin_count;
	    /*set_datasrc(save_datasrc);*/
	}
	else
	    ret = srcfile_open(&dp->desc_sf, dp->grpdesc,
                               nullptr, nullptr);
	if (!ret) {
	    if (dp->flags & DF_TMPGRPDESC) {
		dp->flags &= ~DF_TMPGRPDESC;
		remove(dp->grpdesc);
	    }
	    free(dp->grpdesc);
	    dp->grpdesc = nullptr;
	    return "";
	}
	if (ret == 2 || !dp->desc_sf.refetch_secs)
	    dp->flags |= DF_NOXGTITLE;
    }

    grouplen = strlen(groupname);
    data = hashfetch(dp->desc_sf.hp, groupname, grouplen);
    if (data.dat_ptr) {
	LISTNODE* node = (LISTNODE*)data.dat_ptr;
	/*dp->act_sf.lp->recent = node;*/
	return node->data + data.dat_len + grouplen + 1;
    }

  try_xgtitle:

    if ((dp->flags & (DF_REMOTE|DF_NOXGTITLE)) == DF_REMOTE) {
	set_datasrc(dp);
	sprintf(g_ser_line, "XGTITLE %s", groupname);
	if (nntp_command(g_ser_line) > 0 && nntp_check() > 0) {
	    nntp_gets(buf, sizeof buf - 1);
	    if (nntp_at_list_end(buf))
		sprintf(buf, "%s \n", groupname);
	    else {
		nntp_finish_list();
		strcat(buf, "\n");
	    }
	    groupname = srcfile_append(&dp->desc_sf, buf, grouplen);
	    return groupname+grouplen+1;
	}
	dp->flags |= DF_NOXGTITLE;
	if (dp->desc_sf.lp->high == -1) {
	    srcfile_close(&dp->desc_sf);
	    if (dp->flags & DF_TMPGRPDESC)
		return find_grpdesc(dp, groupname);
	    free(dp->grpdesc);
	    dp->grpdesc = nullptr;
	}
    }
    return "";
}

/* NOTE: This was factored from srcfile_open and srcfile_append and is
 * basically same as dectrl() except the s++, *s != '\n' and return s.
 * Because we need to keep track of s we can't really reuse dectrl()
 * from cache.c; if we want to factor further we need a new function.
 */
char *adv_then_find_next_nl_and_dectrl(char *s)
{
    if (s == NULL)
	return s;

    for (s++; *s && *s != '\n';) {
	int w = byte_length_at(s);
	if (at_grey_space(s)) {
	    int i;
	    for (i = 0; i < w; i += 1) {
		s[i] = ' ';
	    }
	}
	s += w;
    }
    return s;
}

int srcfile_open(SRCFILE *sfp, const char *filename, const char *fetchcmd, const char *server)
{
    unsigned offset;
    char* s;
    HASHDATUM data;
    long node_low;
    int keylen, linelen;
    FILE* fp;
    char* lbp;
    time_t now = time((time_t*)nullptr);
    bool use_buffered_nntp_gets = false;

    if (!filename)
	fp = nullptr;
    else if (server) {
	if (!sfp->refetch_secs) {
	    server = nullptr;
	    fp = fopen(filename, "r");
	    g_spin_todo = 0;
	}
        else if (now - sfp->lastfetch > sfp->refetch_secs && (sfp->refetch_secs != 2 || !sfp->lastfetch))
        {
	    fp = fopen(filename, "w+");
	    if (fp) {
		printf("Getting %s file from %s.", fetchcmd, server);
		fflush(stdout);
		/* tell server we want the file */
		if (!(nntplink.flags & NNTP_NEW_CMD_OK))
		    use_buffered_nntp_gets = true;
		else if (nntp_list(fetchcmd, "", 0) < 0) {
		    printf("\nCan't get %s file from server: \n%s\n",
			   fetchcmd, g_ser_line) FLUSH;
		    termdown(2);
		    fclose(fp);
		    return 0;
		}
		sfp->lastfetch = now;
		if (g_net_speed > 8)
		    g_spin_todo = 0;
	    }
	}
	else {
	    server = nullptr;
	    fp = fopen(filename, "r+");
	    if (!fp) {
		sfp->refetch_secs = 0;
		fp = fopen(filename, "r");
	    }
	    g_spin_todo = 0;
	}
	if (sfp->refetch_secs & 3)
	    sfp->refetch_secs += 365L*24*60*60;
    }
    else
    {
	fp = fopen(filename, "r");
	g_spin_todo = 0;
    }

    if (filename && fp == nullptr) {
	printf(cantopen, filename) FLUSH;
	termdown(1);
	return 0;
    }
    setspin(g_spin_todo > 0? SPIN_BARGRAPH : SPIN_FOREGROUND);

    srcfile_close(sfp);

    /* Create a list with one character per item using a large chunk size. */
    sfp->lp = new_list(0, 0, 1, SRCFILE_CHUNK_SIZE, 0, nullptr);
    sfp->hp = hashcreate(3001, srcfile_cmp);
    sfp->fp = fp;

    if (!filename) {
	(void) listnum2listitem(sfp->lp, 0);
	sfp->lp->high = -1;
	setspin(SPIN_OFF);
	return 1;
    }

    lbp = listnum2listitem(sfp->lp, 0);
    data.dat_ptr = (char*)sfp->lp->first;

    for (offset = 0, node_low = 0; ; offset += linelen, lbp += linelen) {
	if (server) {
	    if (use_buffered_nntp_gets)
		use_buffered_nntp_gets = false;
	    else if (nntp_gets(buf, sizeof buf - 1) < 0) {
		printf("\nError getting %s file.\n", fetchcmd) FLUSH;
		termdown(2);
		srcfile_close(sfp);
		setspin(SPIN_OFF);
		return 0;
	    }
	    if (nntp_at_list_end(buf))
		break;
	    strcat(buf,"\n");
	    fputs(buf, fp);
	    spin(200 * g_net_speed);
	}
	else if (!fgets(buf, sizeof buf, fp))
	    break;

	for (s = buf; *s && !isspace(*s); s++) ;
	if (!*s) {
	    linelen = 0;
	    continue;
	}
	keylen = s - buf;
	if (*++s != '\n' && isspace(*s)) {
	    while (*++s != '\n' && isspace(*s)) ;
	    strcpy(buf+keylen+1, s);
	    s = buf+keylen+1;
	}
	s = adv_then_find_next_nl_and_dectrl(s);
	linelen = s - buf + 1;
	if (*s != '\n') {
	    if (linelen == sizeof buf) {
		linelen = 0;
		continue;
	    }
	    *s++ = '\n';
	    *s = '\0';
	}
	if (offset + linelen > SRCFILE_CHUNK_SIZE) {
	    LISTNODE* node = sfp->lp->recent;
	    node_low += offset;
	    node->high = node_low - 1;
	    node->data_high = node->data + offset - 1;
	    offset = 0;
	    lbp = listnum2listitem(sfp->lp, node_low);
	    data.dat_ptr = (char*)sfp->lp->recent;
	}
	data.dat_len = offset;
	(void) memcpy(lbp,buf,linelen);
	hashstore(sfp->hp, buf, keylen, data);
    }
    sfp->lp->high = node_low + offset - 1;
    setspin(SPIN_OFF);

    if (server) {
	fflush(fp);
	if (ferror(fp)) {
	    printf("\nError writing the %s file %s.\n",fetchcmd,filename) FLUSH;
	    termdown(2);
	    srcfile_close(sfp);
	    return 0;
	}
	newline();
    }
    fseek(fp,0L,0);

    return server? 2 : 1;
}

char *srcfile_append(SRCFILE *sfp, char *bp, int keylen)
{
    LISTNODE* node;
    long pos;
    HASHDATUM data;
    char* s;
    char* lbp;
    int linelen;

    pos = sfp->lp->high + 1;
    lbp = listnum2listitem(sfp->lp, pos);
    node = sfp->lp->recent;
    data.dat_len = pos - node->low;

    s = bp + keylen + 1;
    if (sfp->fp && sfp->refetch_secs && *s != '\n') {
	fseek(sfp->fp, 0, 2);
	fputs(bp, sfp->fp);
    }

    if (*s != '\n' && isspace(*s)) {
	while (*++s != '\n' && isspace(*s)) ;
	strcpy(bp+keylen+1, s);
	s = bp+keylen+1;
    }
    s = adv_then_find_next_nl_and_dectrl(s);
    linelen = s - bp + 1;
    if (*s != '\n') {
	*s++ = '\n';
	*s = '\0';
    }
    if (data.dat_len + linelen > SRCFILE_CHUNK_SIZE) {
	node->high = pos - 1;
	node->data_high = node->data + data.dat_len - 1;
	lbp = listnum2listitem(sfp->lp, pos);
	node = sfp->lp->recent;
	data.dat_len = 0;
    }
    data.dat_ptr = (char*)node;
    (void) memcpy(lbp,bp,linelen);
    hashstore(sfp->hp, bp, keylen, data);
    sfp->lp->high = pos + linelen - 1;

    return lbp;
}

void srcfile_end_append(SRCFILE *sfp, const char *filename)
{
    if (sfp->fp && sfp->refetch_secs) {
	fflush(sfp->fp);

	if (sfp->lastfetch) {
	    struct utimbuf ut;
	    time(&ut.actime);
	    ut.modtime = sfp->lastfetch;
	    (void) utime(filename, &ut);
	}
    }
}

void srcfile_close(SRCFILE *sfp)
{
    if (sfp->fp) {
	fclose(sfp->fp);
	sfp->fp = nullptr;
    }
    if (sfp->lp) {
	delete_list(sfp->lp);
	sfp->lp = nullptr;
    }
    if (sfp->hp) {
	hashdestroy(sfp->hp);
	sfp->hp = nullptr;
    }
}

static int srcfile_cmp(const char *key, int keylen, HASHDATUM data)
{
    /* We already know that the lengths are equal, just compare the strings */
    return memcmp(key, ((LISTNODE*)data.dat_ptr)->data + data.dat_len, keylen);
}

/* Edit Distance extension to trn
 *
 *	Mark Maimone (mwm@cmu.edu)
 *	Carnegie Mellon Computer Science
 *	9 May 1993
 *
 *	This code helps trn handle typos in newsgroup names much more
 *   gracefully.  Instead of "... does not exist!!", it will pick the
 *   nearest one, or offer you a choice if there are several options.
 */

/* find_close_match -- Finds the closest match for the string given in
 * global ngname.  If found, the result will be the corrected string
 * returned in that global.
 *
 * We compare the (presumably misspelled) newsgroup name with all legal
 * newsgroups, using the Edit Distance metric.  The edit distance between
 * two strings is the minimum number of simple operations required to
 * convert one string to another (the implementation here supports INSERT,
 * DELETE, CHANGE and SWAP).  This gives every legal newsgroup an integer
 * rank.
 *
 * You might want to present all of the closest matches, and let the user
 * choose among them.  But because I'm lazy I chose to only keep track of
 * all with newsgroups with the *single* smallest error, in array ngptrs[].
 * A more flexible approach would keep around the 10 best matches, whether
 * or not they had precisely the same edit distance, but oh well.
 */

static char** ngptrs;		/* List of potential matches */
static int ngn;			/* Length of list in ngptrs[] */
static int best_match;		/* Value of best match */

int find_close_match()
{
    DATASRC* dp;
    int ret = 0;

    best_match = -1;
    ngptrs = (char**)safemalloc(MAX_NG * sizeof (char*));
    ngn = 0;

    /* Iterate over all legal newsgroups */
    for (dp = datasrc_first(); dp && dp->name; dp = datasrc_next(dp)) {
	if (dp->flags & DF_OPEN) {
	    if (dp->act_sf.hp)
		hashwalk(dp->act_sf.hp, check_distance, 0);
	    else
		ret = -1;
	}
    }

    if (ret < 0) {
	hashwalk(g_newsrc_hash, check_distance, 1);
	ret = 0;
    }

    /* ngn is the number of possibilities.  If there's just one, go with it. */

    switch (ngn) {
        case 0:
	    break;
	case 1: {
	    char* cp = strchr(ngptrs[0], ' ');
	    if (cp)
		*cp = '\0';
	    if (verbose)
		printf("(I assume you meant %s)\n", ngptrs[0]) FLUSH;
	    else
		printf("(Using %s)\n", ngptrs[0]) FLUSH;
	    set_ngname(ngptrs[0]);
	    if (cp)
		*cp = ' ';
	    ret = 1;
	    break;
	}
	default:
	    ret = get_near_miss();
	    break;
    }
    free((char*)ngptrs);
    return ret;
}

static int check_distance(int len, HASHDATUM *data, int newsrc_ptr)
{
    int value;
    char* name;

    if (newsrc_ptr)
	name = ((NGDATA*)data->dat_ptr)->rcline;
    else
	name = ((LISTNODE*)data->dat_ptr)->data + data->dat_len;

    /* Efficiency: don't call edit_dist when the lengths are too different. */
    if (len < ngname_len) {
	if (ngname_len - len > LENGTH_HACK)
	    return 0;
    }
    else {
	if (len - ngname_len > LENGTH_HACK)
	    return 0;
    }

    value = edit_distn(ngname, ngname_len, name, len);
    if (value > MIN_DIST)
	return 0;

    if (value < best_match)
	ngn = 0;
    if (best_match < 0 || value <= best_match) {
	int i;
	for (i = 0; i < ngn; i++) {
	    if (!strcmp(name,ngptrs[i]))
		return 0;
	}
	best_match = value;
	if (ngn < MAX_NG)
	    ngptrs[ngn++] = name;
    }
    return 0;
}

/* Now we've got several potential matches, and have to choose between them
** somehow.  Again, results will be returned in global ngname.
*/
static int get_near_miss()
{
    char promptbuf[256];
    char options[MAX_NG+10];
    char* op = options;
    int i;

    if (verbose)
	printf("However, here are some close matches:\n") FLUSH;
    if (ngn > 9)
	ngn = 9;	/* Since we're using single digits.... */
    for (i = 0; i < ngn; i++) {
	char* cp = strchr(ngptrs[i], ' ');
	if (cp)
	    *cp = '\0';
	printf("  %d.  %s\n", i+1, ngptrs[i]);
	sprintf(op++, "%d", i+1);	/* Expensive, but avoids ASCII deps */
	if (cp)
	    *cp = ' ';
    }
    *op++ = 'n';
    *op = '\0';

    if (verbose)
	sprintf(promptbuf, "Which of these would you like?");
    else
	sprintf(promptbuf, "Which?");
reask:
    in_char(promptbuf, 'A', options);
    printcmd();
    putchar('\n') FLUSH;
    switch (*buf) {
        case 'n':
	case 'N':
	case 'q':
	case 'Q':
	case 'x':
	case 'X':
	    return 0;
	case 'h':
	case 'H':
	    if (verbose)
		fputs("  You entered an illegal newsgroup name, and these are the nearest possible\n  matches.  If you want one of these, then enter its number.  Otherwise\n  just say 'n'.\n", stdout) FLUSH;
	    else
		fputs("Illegal newsgroup, enter a number or 'n'.\n", stdout) FLUSH;
	    goto reask;
	default:
	    if (isdigit(*buf)) {
		char* s = strchr(options, *buf);

		i = s ? (s - options) : ngn;
		if (i >= 0 && i < ngn) {
		    char* cp = strchr(ngptrs[i], ' ');
		    if (cp)
			*cp = '\0';
		    set_ngname(ngptrs[i]);
		    if (cp)
			*cp = ' ';
		    return 1;
		}
	    }
	    fputs(hforhelp, stdout) FLUSH;
	    break;
    }

    settle_down();
    goto reask;
}
