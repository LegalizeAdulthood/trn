/* kfile.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "kfile.h"

#include "artsrch.h"
#include "bits.h"
#include "cache.h"
#include "color.h"
#include "env.h"
#include "hash.h"
#include "list.h"
#include "ng.h"
#include "ngdata.h"
#include "ngstuff.h"
#include "rcstuff.h"
#include "rt-process.h"
#include "rt-select.h"
#include "rt-util.h"
#include "rthread.h"
#include "term.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

FILE               *g_globkfp{};                /* global article killer file */
FILE               *g_localkfp{};               /* local (for this newsgroup) file */
killfilestate_flags g_kf_state{};               /* the state of our kill files */
killfilestate_flags g_kfs_local_change_clear{}; /* bits to clear local changes */
killfilestate_flags g_kfs_thread_change_set{};  /* bits to set for thread changes */
int                 g_kf_thread_cnt{};          /* # entries in the thread kfile */
int                 g_kf_changethd_cnt{};       /* # entries changed from old to new */
long                g_kf_daynum{};              /* day number for thread killfile */
ART_NUM             g_killfirst{};              /* used as g_firstart when killing */

static void mention(const char *str);
static bool kfile_junk(char *ptr, int killmask);
static int write_local_thread_commands(int keylen, HASHDATUM *data, int extra);
static int write_global_thread_commands(int keylen, HASHDATUM *data, int appending);
static int age_thread_commands(int keylen, HASHDATUM *data, int elapsed_days);

static bool s_exitcmds{};
static char s_thread_cmd_ltr[] = "JK,j+S.m";
static autokill_flags s_thread_cmd_flag[] = {
    AUTO_KILL_THD, AUTO_KILL_SBJ, AUTO_KILL_FOL, AUTO_KILL_1, AUTO_SEL_THD, AUTO_SEL_SBJ, AUTO_SEL_FOL, AUTO_SEL_1,
};
static char s_killglobal[] = KILLGLOBAL;
static char s_killlocal[] = KILLLOCAL;
static char s_killthreads[] = KILLTHREADS;

void kfile_init()
{
    char* cp = getenv("KILLTHREADS");
    if (!cp)
	cp = s_killthreads;
    if (*cp && strcmp(cp,"none")) {
        g_kf_daynum = KF_DAYNUM(0);
        g_kf_thread_cnt = 0;
        g_kf_changethd_cnt = 0;
        FILE *fp = fopen(filexp(cp), "r");
        if (fp != nullptr)
        {
	    g_msgid_hash = hashcreate(1999, msgid_cmp);
	    while (fgets(g_buf, sizeof g_buf, fp) != nullptr) {
		if (*g_buf == '<') {
                    cp = strchr(g_buf,' ');
		    if (!cp)
			cp = ",";
		    else
			*cp++ = '\0';
		    int age = g_kf_daynum - atol(cp + 1);
		    if (age > KF_MAXDAYS) {
			g_kf_changethd_cnt++;
			continue;
		    }
                    cp = strchr(s_thread_cmd_ltr, *cp);
                    if (cp != nullptr)
                    {
                        int auto_flag = s_thread_cmd_flag[cp - s_thread_cmd_ltr];
			HASHDATUM data = hashfetch(g_msgid_hash, g_buf, strlen(g_buf));
			if (!data.dat_ptr)
			    data.dat_ptr = savestr(g_buf);
			else
			    g_kf_changethd_cnt++;
			data.dat_len = auto_flag | age;
			hashstorelast(data);
		    }
		    g_kf_thread_cnt++;
		}
	    }
	    fclose(fp);
	}
	g_kf_state |= KFS_GLOBAL_THREADFILE;
	g_kfs_local_change_clear = KFS_LOCAL_CHANGES;
	g_kfs_thread_change_set = KFS_THREAD_CHANGES;
    }
    else {
	g_kfs_local_change_clear = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
	g_kfs_thread_change_set = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
    }
}

static void mention(const char *str)
{
    if (g_verbose) {
	color_string(COLOR_NOTICE,str);
	newline();
    }
    else
	putchar('.');
    fflush(stdout);
}

static bool s_kill_mentioned;

int do_kfile(FILE *kfp, int entering)
{
    bool first_time = (entering && !g_killfirst);
    char last_kill_type = '\0';
    int thread_kill_cnt = 0;
    int thread_select_cnt = 0;
    char* cp;
    char* bp;

    g_art = g_lastart+1;
    g_killfirst = g_firstart;
    fseek(kfp,0L,0);			/* rewind file */
    while (fgets(g_buf,LBUFLEN,kfp) != nullptr) {
	if (*(cp = g_buf + strlen(g_buf) - 1) == '\n')
	    *cp = '\0';
	for (bp = g_buf; isspace(*bp); bp++) ;
	if (!strncmp(bp,"THRU",4)) {
	    int len = strlen(g_ngptr->rc->name);
	    cp = bp + 4;
	    while (isspace(*cp)) cp++;
	    if (strncmp(cp, g_ngptr->rc->name, len) || !isspace(cp[len]))
		continue;
	    g_killfirst = atol(cp+len+1)+1;
	    if (g_killfirst < g_firstart)
		g_killfirst = g_firstart;
	    if (g_killfirst > g_lastart)
		g_killfirst = g_lastart+1;
	    continue;
	}
	if (*bp == 'I') {
            for (cp = bp + 1; *cp && !isspace(*cp); cp++) ;
	    while (isspace(*cp)) cp++;
	    if (!*cp)
		continue;
	    cp = filexp(cp);
	    if (!strchr(cp,'/')) {
		set_ngname(cp);
		cp = filexp(get_val("KILLLOCAL",s_killlocal));
		set_ngname(g_ngptr->rcline);
	    }
            FILE *incfile = fopen(cp, "r");
            if (incfile != nullptr)
            {
		int ret = do_kfile(incfile, entering);
		fclose(incfile);
		if (ret)
		    return ret;
	    }
	    continue;
	}
	if (*bp == 'X') {		/* exit command? */
	    if (entering) {
		s_exitcmds = true;
		continue;
	    }
	    bp++;
	}
	else if (!entering)
	    continue;

	if (*bp == '&') {
	    mention(bp);
	    if (bp > g_buf)
		strcpy(g_buf, bp);
	    switcheroo();
	}
	else if (*bp == '/') {
	    g_kf_state |= KFS_NORMAL_LINES;
	    if (g_firstart > g_lastart)
		continue;
	    if (last_kill_type) {
		if (perform_status_end(g_ngptr->toread,"article")) {
		    s_kill_mentioned = true;
		    carriage_return();
		    fputs(g_msg, stdout);
		    newline();
		}
	    }
	    perform_status_init(g_ngptr->toread);
	    last_kill_type = '/';
	    mention(bp);
	    s_kill_mentioned = true;
	    switch (art_search(bp, (sizeof g_buf) - (bp - g_buf), false)) {
	    case SRCH_ABORT:
		continue;
	    case SRCH_INTR:
		if (g_verbose)
		    printf("\n(Interrupted at article %ld)\n",(long)g_art)
		      FLUSH;
		else
		    printf("\n(Intr at %ld)\n",(long)g_art) FLUSH;
		termdown(2);
		return -1;
	    case SRCH_DONE:
		break;
	    case SRCH_SUBJDONE:
		/*fputs("\tsubject not found (?)\n",stdout) FLUSH;*/
		break;
	    case SRCH_NOTFOUND:
		/*fputs("\tnot found\n",stdout) FLUSH;*/
		break;
	    case SRCH_FOUND:
		/*fputs("\tfound\n",stdout) FLUSH;*/
		break;
            case SRCH_ERROR:
                break;
            }
	}
	else if (first_time && *bp == '<') {
            if (last_kill_type != '<') {
		if (last_kill_type) {
		    if (perform_status_end(g_ngptr->toread,"article")) {
			s_kill_mentioned = true;
			carriage_return();
			fputs(g_msg, stdout);
			newline();
		    }
		}
		perform_status_init(g_ngptr->toread);
		last_kill_type = '<';
	    }
	    cp = strchr(bp,' ');
	    if (!cp)
		cp = "T,";
	    else
		*cp++ = '\0';
            ARTICLE *ap = get_article(bp);
            if (ap != nullptr)
            {
		if ((ap->flags & AF_FAKE) && !ap->child1) {
		    if (*cp == 'T')
			cp++;
                    cp = strchr(s_thread_cmd_ltr, *cp);
                    if (cp != nullptr)
                    {
			ap->autofl = s_thread_cmd_flag[cp-s_thread_cmd_ltr];
			if (ap->autofl & AUTO_KILL_MASK)
			    thread_kill_cnt++;
			else
			    thread_select_cnt++;
		    }
		} else {
		    g_art = article_num(ap);
		    g_artp = ap;
		    perform(cp,false);
		    if (ap->autofl & AUTO_SEL_MASK)
			thread_select_cnt++;
		    else if (ap->autofl & AUTO_KILL_MASK)
			thread_kill_cnt++;
		}
	    }
	    g_art = g_lastart+1;
	    g_kf_state |= KFS_THREAD_LINES;
	}
	else if (*bp == '<') {
	    g_kf_state |= KFS_THREAD_LINES;
	}
	else if (*bp == '*') {
	    int killmask = AF_UNREAD;
	    switch (bp[1]) {
	    case 'X':
		killmask |= g_sel_mask;	/* don't kill selected articles */
		/* FALL THROUGH */
	    case 'j':
		article_walk(kfile_junk, killmask);
		break;
	    }
	    g_kf_state |= KFS_NORMAL_LINES;
	}
    }
    if (thread_kill_cnt) {
	sprintf(g_buf,"%ld auto-kill command%s.", (long)thread_kill_cnt,
		plural(thread_kill_cnt));
	mention(g_buf);
	s_kill_mentioned = true;
    }
    if (thread_select_cnt) {
	sprintf(g_buf,"%ld auto-select command%s.", (long)thread_select_cnt,
		plural(thread_select_cnt));
	mention(g_buf);
	s_kill_mentioned = true;
    }
    if (last_kill_type) {
	if (perform_status_end(g_ngptr->toread,"article")) {
	    s_kill_mentioned = true;
	    carriage_return();
	    fputs(g_msg, stdout);
	    newline();
	}
    }
    return 0;
}

static bool kfile_junk(char *ptr, int killmask)
{
    ARTICLE* ap = (ARTICLE*)ptr;
    if ((ap->flags & killmask) == AF_UNREAD)
	set_read(ap);
    else if (ap->flags & g_sel_mask) {
	ap->flags &= ~static_cast<article_flags>(g_sel_mask);
	if (!g_selected_count--)
	    g_selected_count = 0;
    }
    return false;
}

void kill_unwanted(ART_NUM starting, const char *message, int entering)
{
    bool intr = false;			/* did we get an interrupt? */
    char oldmode = g_mode;
    bool anytokill = (g_ngptr->toread > 0);

    set_mode('r','k');
    if ((entering || s_exitcmds) && (g_localkfp || g_globkfp)) {
	s_exitcmds = false;
	ART_NUM oldfirst = g_firstart;
	g_firstart = starting;
	clear();
	if (message && (g_verbose || entering))
	    fputs(message,stdout) FLUSH;

	s_kill_mentioned = false;
	if (g_localkfp) {
	    if (entering)
		g_kf_state |= KFS_LOCAL_CHANGES;
	    intr = do_kfile(g_localkfp,entering);
	}
	open_kfile(KF_GLOBAL);		/* Just in case the name changed */
	if (g_globkfp && !intr)
	    intr = do_kfile(g_globkfp,entering);
	newline();
	if (entering && s_kill_mentioned && g_novice_delays) {
	    if (g_verbose)
		get_anything();
	    else
		pad(g_just_a_sec);
	}
	if (anytokill)			/* if there was anything to kill */
	    g_forcelast = false;		/* allow for having killed it all */
	g_firstart = oldfirst;
    }
    if (!entering && (g_kf_state & KFS_LOCAL_CHANGES) && !intr)
	rewrite_kfile(g_lastart);
    set_mode(g_general_mode,oldmode);
}

static FILE *s_newkfp{};

static int write_local_thread_commands(int keylen, HASHDATUM *data, int extra)
{
    ARTICLE* ap = (ARTICLE*)data->dat_ptr;
    int autofl = ap->autofl;
    char ch;

    if (autofl && ((ap->flags & AF_EXISTS) || ap->child1)) {
        /* The arrays are in priority order, so find highest priority bit. */
	for (int i = 0; s_thread_cmd_ltr[i]; i++) {
	    if (autofl & s_thread_cmd_flag[i]) {
		ch = s_thread_cmd_ltr[i];
		break;
	    }
	}
	fprintf(s_newkfp,"%s T%c\n", ap->msgid, ch);
    }
    return 0;
}

void rewrite_kfile(ART_NUM thru)
{
    bool has_content = (g_kf_state & (KFS_THREAD_LINES|KFS_GLOBAL_THREADFILE))
				 == KFS_THREAD_LINES;
    bool has_star_commands = false;
    bool needs_newline = false;
    char* killname = filexp(get_val("KILLLOCAL",s_killlocal));
    char* bp;

    if (g_localkfp)
	fseek(g_localkfp,0L,0);		/* rewind current file */
    else
	makedir(killname,MD_FILE);
    remove(killname);			/* to prevent file reuse */
    g_kf_state &= ~(g_kfs_local_change_clear | KFS_NORMAL_LINES);
    s_newkfp = fopen(killname, "w");
    if (s_newkfp != nullptr)
    {
	fprintf(s_newkfp,"THRU %s %ld\n",g_ngptr->rc->name,(long)thru);
	while (g_localkfp && fgets(g_buf,LBUFLEN,g_localkfp) != nullptr) {
	    if (!strncmp(g_buf,"THRU",4)) {
		char* cp = g_buf+4;
		int len = strlen(g_ngptr->rc->name);
		while (isspace(*cp)) cp++;
		if (isdigit(*cp))
		    continue;
		if (strncmp(cp, g_ngptr->rc->name, len)
		 || (cp[len] && !isspace(cp[len]))) {
		    fputs(g_buf,s_newkfp);
		    needs_newline = !strchr(g_buf,'\n');
		}
		continue;
	    }
	    for (bp = g_buf; isspace(*bp); bp++) ;
	    /* Leave out any outdated thread commands */
	    if (*bp == 'T' || *bp == '<')
		continue;
	    /* Write star commands after other kill commands */
	    if (*bp == '*')
		has_star_commands = true;
	    else {
		fputs(g_buf,s_newkfp);
		needs_newline = !strchr(bp,'\n');
	    }
	    has_content = true;
	}
	if (needs_newline)
	    putc('\n', s_newkfp);
	if (has_star_commands) {
	    fseek(g_localkfp,0L,0);			/* rewind file */
	    while (fgets(g_buf,LBUFLEN,g_localkfp) != nullptr) {
		for (bp = g_buf; isspace(*bp); bp++) ;
		if (*bp == '*') {
		    fputs(g_buf,s_newkfp);
		    needs_newline = !strchr(bp,'\n');
		}
	    }
	    if (needs_newline)
		putc('\n', s_newkfp);
	}
	if (!(g_kf_state & KFS_GLOBAL_THREADFILE)) {
	    /* Append all the still-valid thread commands */
	    hashwalk(g_msgid_hash, write_local_thread_commands, 0);
	}
	fclose(s_newkfp);
	if (!has_content)
	    remove(killname);
	open_kfile(KF_LOCAL);		/* and reopen local file */
    }
    else
	printf(g_cantcreate,g_buf) FLUSH;
}

static int write_global_thread_commands(int keylen, HASHDATUM *data, int appending)
{
    int autofl;
    int age;
    char* msgid;
    char ch;

    if (data->dat_len) {
	if (appending)
	    return 0;
	autofl = data->dat_len;
	age = autofl & KF_AGE_MASK;
	msgid = data->dat_ptr;
    }
    else {
	ARTICLE* ap = (ARTICLE*)data->dat_ptr;
	autofl = ap->autofl;
	if (!autofl || (appending && (autofl & AUTO_OLD)))
	    return 0;
	ap->autofl |= AUTO_OLD;
	age = 0;
	msgid = ap->msgid;
    }

    /* The arrays are in priority order, so find highest priority bit. */
    for (int i = 0; s_thread_cmd_ltr[i]; i++) {
	if (autofl & s_thread_cmd_flag[i]) {
	    ch = s_thread_cmd_ltr[i];
	    break;
	}
    }
    fprintf(s_newkfp,"%s %c %ld\n", msgid, ch, g_kf_daynum - age);
    g_kf_thread_cnt++;

    return 0;
}

static int age_thread_commands(int keylen, HASHDATUM *data, int elapsed_days)
{
    if (data->dat_len) {
	int age = (data->dat_len & KF_AGE_MASK) + elapsed_days;
	if (age > KF_MAXDAYS) {
	    free(data->dat_ptr);
	    g_kf_changethd_cnt++;
	    return -1;
	}
	data->dat_len += elapsed_days;
    }
    else {
	ARTICLE* ap = (ARTICLE*)data->dat_ptr;
	if (ap->autofl & AUTO_OLD) {
	    ap->autofl &= ~AUTO_OLD;
	    g_kf_changethd_cnt++;
	    g_kf_state |= KFS_THREAD_CHANGES;
	}
    }
    return 0;
}

void update_thread_kfile()
{
    if (!(g_kf_state & KFS_GLOBAL_THREADFILE))
	return;

    int elapsed_days = KF_DAYNUM(g_kf_daynum);
    if (elapsed_days) {
	hashwalk(g_msgid_hash, age_thread_commands, elapsed_days);
	g_kf_daynum += elapsed_days;
    }

    if (!(g_kf_state & KFS_THREAD_CHANGES))
	return;

    char *cp = filexp(get_val("KILLTHREADS", s_killthreads));
    makedir(cp,MD_FILE);
    if (g_kf_changethd_cnt*5 > g_kf_thread_cnt) {
	remove(cp);			/* to prevent file reuse */
        s_newkfp = fopen(cp, "w");
        if (s_newkfp == nullptr)
	    return; /*$$ Yikes! */
        g_kf_thread_cnt = 0;
        g_kf_changethd_cnt = 0;
	hashwalk(g_msgid_hash, write_global_thread_commands, 0); /* Rewrite */
    }
    else {
        s_newkfp = fopen(cp, "a");
        if (s_newkfp == nullptr)
	    return; /*$$ Yikes! */
	hashwalk(g_msgid_hash, write_global_thread_commands, 1); /* Append */
    }
    fclose(s_newkfp);

    g_kf_state &= ~KFS_THREAD_CHANGES;
}

void change_auto_flags(ARTICLE *ap, autokill_flags auto_flag)
{
    if (auto_flag > (ap->autofl & (AUTO_KILL_MASK|AUTO_SEL_MASK))) {
	if (ap->autofl & AUTO_OLD)
	    g_kf_changethd_cnt++;
	ap->autofl = auto_flag;
	g_kf_state |= g_kfs_thread_change_set;
    }
}

void clear_auto_flags(ARTICLE *ap)
{
    if (ap->autofl) {
	if (ap->autofl & AUTO_OLD)
	    g_kf_changethd_cnt++;
	ap->autofl = AUTO_KILL_NONE;
	g_kf_state |= g_kfs_thread_change_set;
    }
}

void perform_auto_flags(ARTICLE *ap, autokill_flags thread_autofl, autokill_flags subj_autofl, autokill_flags chain_autofl)
{
    if (thread_autofl & AUTO_SEL_THD) {
	if (g_sel_mode == SM_THREAD)
	    select_arts_thread(ap, AUTO_SEL_THD);
	else
	    select_arts_subject(ap, AUTO_SEL_THD);
    }
    else if (subj_autofl & AUTO_SEL_SBJ)
	select_arts_subject(ap, AUTO_SEL_SBJ);
    else if (chain_autofl & AUTO_SEL_FOL)
	select_subthread(ap, AUTO_SEL_FOL);
    else if (ap->autofl & AUTO_SEL_1)
	select_article(ap, AUTO_SEL_1);

    if (thread_autofl & AUTO_KILL_THD) {
	if (g_sel_mode == SM_THREAD)
	    kill_arts_thread(ap, AFFECT_ALL|AUTO_KILL_THD);
	else
	    kill_arts_subject(ap, AFFECT_ALL|AUTO_KILL_THD);
    }
    else if (subj_autofl & AUTO_KILL_SBJ)
	kill_arts_subject(ap, AFFECT_ALL|AUTO_KILL_SBJ);
    else if (chain_autofl & AUTO_KILL_FOL)
	kill_subthread(ap, AFFECT_ALL|AUTO_KILL_FOL);
    else if (ap->autofl & AUTO_KILL_1)
	mark_as_read(ap);
}

/* edit KILL file for newsgroup */

void edit_kfile()
{
    char* bp;

    if (g_in_ng) {
	if (g_kf_state & KFS_LOCAL_CHANGES)
	    rewrite_kfile(g_lastart);
	if (!(g_kf_state & KFS_GLOBAL_THREADFILE)) {
            for (SUBJECT *sp = g_first_subject; sp; sp = sp->next)
		clear_subject(sp);
	}
	strcpy(g_buf,filexp(get_val("KILLLOCAL",s_killlocal)));
    } else
	strcpy(g_buf,filexp(get_val("KILLGLOBAL",s_killglobal)));
    if (!makedir(g_buf, MD_FILE)) {
	sprintf(g_cmd_buf,"%s %s",
	    filexp(get_val("VISUAL",get_val("EDITOR",DEFEDITOR))),g_buf);
	printf("\nEditing %s KILL file:\n%s\n",
	    (g_in_ng?"local":"global"),g_cmd_buf) FLUSH;
	termdown(3);
	resetty();			/* make sure tty is friendly */
        doshell(SH, g_cmd_buf);       /* invoke the shell */
	noecho();			/* and make terminal */
	crmode();			/*   unfriendly again */
	open_kfile(g_in_ng);
	if (g_localkfp) {
	    fseek(g_localkfp,0L,0);			/* rewind file */
	    g_kf_state &= ~KFS_NORMAL_LINES;
	    while (fgets(g_buf,LBUFLEN,g_localkfp) != nullptr) {
		for (bp = g_buf; isspace(*bp); bp++) ;
		if (*bp == '/' || *bp == '*')
		    g_kf_state |= KFS_NORMAL_LINES;
		else if (*bp == '<') {
                    char* cp = strchr(bp,' ');
		    if (!cp)
			cp = ",";
		    else
			*cp++ = '\0';
                    ARTICLE *ap = get_article(bp);
                    if (ap != nullptr)
                    {
			if (*cp == 'T')
			    cp++;
                        cp = strchr(s_thread_cmd_ltr, *cp);
                        if (cp != nullptr)
			    ap->autofl |= s_thread_cmd_flag[cp-s_thread_cmd_ltr];
		    }
		}
	    }
	}
    }
    else {
	printf("Can't make %s\n",g_buf) FLUSH;
	termdown(1);
    }
}

void open_kfile(int local)
{
    const char *kname = filexp(local ? get_val("KILLLOCAL", s_killlocal) : get_val("KILLGLOBAL", s_killglobal));

    /* delete the file if it is empty */
    if (stat(kname,&g_filestat) >= 0 && !g_filestat.st_size)
	remove(kname);
    if (local) {
	if (g_localkfp)
	    fclose(g_localkfp);
	g_localkfp = fopen(kname,"r");
    }
    else {
	if (g_globkfp)
	    fclose(g_globkfp);
	g_globkfp = fopen(kname,"r");
    }
}

void kf_append(const char *cmd, bool local)
{
    strcpy(g_cmd_buf, filexp(local ? get_val("KILLLOCAL", s_killlocal) : get_val("KILLGLOBAL", s_killglobal)));
    if (!makedir(g_cmd_buf,MD_FILE)) {
	if (g_verbose)
	    printf("\nDepositing command in %s...",g_cmd_buf);
	else
	    printf("\n--> %s...",g_cmd_buf);
	fflush(stdout);
	if (g_novice_delays)
	    sleep(2);
        if (FILE *fp = fopen(g_cmd_buf, "a+"))
        {
	    char ch;
	    if (fseek(fp,-1L,2) < 0)
		ch = '\n';
	    else
		ch = getc(fp);
	    fseek(fp,0L,2);
	    if (ch != '\n')
		putc('\n', fp);
	    fprintf(fp,"%s\n",cmd);
	    fclose(fp);
	    if (local && !g_localkfp)
		open_kfile(KF_LOCAL);
	    fputs("done\n",stdout) FLUSH;
	}
	else
	    printf(g_cantopen,g_cmd_buf) FLUSH;
	termdown(2);
    }
    g_kf_state |= KFS_NORMAL_LINES;
}
