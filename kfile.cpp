/* kfile.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "term.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "hash.h"
#include "cache.h"
#include "artsrch.h"
#include "ng.h"
#include "ngdata.h"
#include "intrp.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "addng.h"
#include "nntp.h"
#include "ngstuff.h"
#include "rcstuff.h"
#include "trn.h"
#include "bits.h"
#include "rthread.h"
#include "rt-process.h"
#include "rt-select.h"
#include "rt-util.h"
#include "color.h"
#include "INTERN.h"
#include "kfile.h"
#include "kfile.ih"

static bool exitcmds = false;

char thread_cmd_ltr[] = "JK,j+S.m";
unsigned short thread_cmd_flag[] = {
    AUTO_KILL_THD, AUTO_KILL_SBJ, AUTO_KILL_FOL, AUTO_KILL_1, 
    AUTO_SEL_THD, AUTO_SEL_SBJ, AUTO_SEL_FOL, AUTO_SEL_1, 
};

static char killglobal[] = KILLGLOBAL;
static char killlocal[] = KILLLOCAL;
static char killthreads[] = KILLTHREADS;

void kfile_init()
{
    char* cp = getenv("KILLTHREADS");
    if (!cp)
	cp = killthreads;
    if (*cp && strNE(cp, "none")) {
	FILE* fp;
	kf_daynum = KF_DAYNUM(0);
	kf_thread_cnt = kf_changethd_cnt = 0;
	if ((fp = fopen(filexp(cp), "r")) != nullptr) {
	    msgid_hash = hashcreate(1999, msgid_cmp);
	    while (fgets(buf, sizeof buf, fp) != nullptr) {
		if (*buf == '<') {
		    int age;
		    cp = index(buf,' ');
		    if (!cp)
			cp = ",";
		    else
			*cp++ = '\0';
		    age = kf_daynum - atol(cp+1);
		    if (age > KF_MAXDAYS) {
			kf_changethd_cnt++;
			continue;
		    }
		    if ((cp = index(thread_cmd_ltr, *cp)) != nullptr) {
			int auto_flag;
			HASHDATUM data;

			auto_flag = thread_cmd_flag[cp-thread_cmd_ltr];
			data = hashfetch(msgid_hash,buf,strlen(buf));
			if (!data.dat_ptr)
			    data.dat_ptr = savestr(buf);
			else
			    kf_changethd_cnt++;
			data.dat_len = auto_flag | age;
			hashstorelast(data);
		    }
		    kf_thread_cnt++;
		}
	    }
	    fclose(fp);
	}
	kf_state |= KFS_GLOBAL_THREADFILE;
	kfs_local_change_clear = KFS_LOCAL_CHANGES;
	kfs_thread_change_set = KFS_THREAD_CHANGES;
    }
    else {
	kfs_local_change_clear = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
	kfs_thread_change_set = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
    }
}

static void mention(char *str)
{
    if (verbose) {
	color_string(COLOR_NOTICE,str);
	newline();
    }
    else
	putchar('.');
    fflush(stdout);
}

static bool kill_mentioned;

int do_kfile(FILE *kfp, int entering)
{
    bool first_time = (entering && !killfirst);
    char last_kill_type = '\0';
    int thread_kill_cnt = 0;
    int thread_select_cnt = 0;
    char* cp;
    char* bp;

    art = lastart+1;
    killfirst = firstart;
    fseek(kfp,0L,0);			/* rewind file */
    while (fgets(buf,LBUFLEN,kfp) != nullptr) {
	if (*(cp = buf + strlen(buf) - 1) == '\n')
	    *cp = '\0';
	for (bp = buf; isspace(*bp); bp++) ;
	if (strnEQ(bp,"THRU",4)) {
	    int len = strlen(ngptr->rc->name);
	    cp = bp + 4;
	    while (isspace(*cp)) cp++;
	    if (strnNE(cp, ngptr->rc->name, len) || !isspace(cp[len]))
		continue;
	    killfirst = atol(cp+len+1)+1;
	    if (killfirst < firstart)
		killfirst = firstart;
	    if (killfirst > lastart)
		killfirst = lastart+1;
	    continue;
	}
	if (*bp == 'I') {
	    FILE* incfile;
	    int ret;
	    for (cp = bp + 1; *cp && !isspace(*cp); cp++) ;
	    while (isspace(*cp)) cp++;
	    if (!*cp)
		continue;
	    cp = filexp(cp);
	    if (!index(cp,'/')) {
		set_ngname(cp);
		cp = filexp(get_val("KILLLOCAL",killlocal));
		set_ngname(ngptr->rcline);
	    }
	    if ((incfile = fopen(cp, "r")) != nullptr) {
		ret = do_kfile(incfile, entering);
		fclose(incfile);
		if (ret)
		    return ret;
	    }
	    continue;
	}
	if (*bp == 'X') {		/* exit command? */
	    if (entering) {
		exitcmds = true;
		continue;
	    }
	    bp++;
	}
	else if (!entering)
	    continue;

	if (*bp == '&') {
	    mention(bp);
	    if (bp > buf)
		strcpy(buf, bp);
	    switcheroo();
	}
	else if (*bp == '/') {
	    kf_state |= KFS_NORMAL_LINES;
	    if (firstart > lastart)
		continue;
	    if (last_kill_type) {
		if (perform_status_end(ngptr->toread,"article")) {
		    kill_mentioned = true;
		    carriage_return();
		    fputs(msg, stdout);
		    newline();
		}
	    }
	    perform_status_init(ngptr->toread);
	    last_kill_type = '/';
	    mention(bp);
	    kill_mentioned = true;
	    switch (art_search(bp, (sizeof buf) - (bp - buf), false)) {
	    case SRCH_ABORT:
		continue;
	    case SRCH_INTR:
		if (verbose)
		    printf("\n(Interrupted at article %ld)\n",(long)art)
		      FLUSH;
		else
		    printf("\n(Intr at %ld)\n",(long)art) FLUSH;
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
	    }
	}
	else if (first_time && *bp == '<') {
	    ARTICLE* ap;
	    if (last_kill_type != '<') {
		if (last_kill_type) {
		    if (perform_status_end(ngptr->toread,"article")) {
			kill_mentioned = true;
			carriage_return();
			fputs(msg, stdout);
			newline();
		    }
		}
		perform_status_init(ngptr->toread);
		last_kill_type = '<';
	    }
	    cp = index(bp,' ');
	    if (!cp)
		cp = "T,";
	    else
		*cp++ = '\0';
	    if ((ap = get_article(bp)) != nullptr) {
		if ((ap->flags & AF_FAKE) && !ap->child1) {
		    if (*cp == 'T')
			cp++;
		    if ((cp = index(thread_cmd_ltr, *cp)) != nullptr) {
			ap->autofl = thread_cmd_flag[cp-thread_cmd_ltr];
			if (ap->autofl & AUTO_KILLS)
			    thread_kill_cnt++;
			else
			    thread_select_cnt++;
		    }
		} else {
		    art = article_num(ap);
		    artp = ap;
		    perform(cp,false);
		    if (ap->autofl & AUTO_SELS)
			thread_select_cnt++;
		    else if (ap->autofl & AUTO_KILLS)
			thread_kill_cnt++;
		}
	    }
	    art = lastart+1;
	    kf_state |= KFS_THREAD_LINES;
	}
	else if (*bp == '<') {
	    kf_state |= KFS_THREAD_LINES;
	}
	else if (*bp == '*') {
	    int killmask = AF_UNREAD;
	    switch (bp[1]) {
	    case 'X':
		killmask |= sel_mask;	/* don't kill selected articles */
		/* FALL THROUGH */
	    case 'j':
		article_walk(kfile_junk, killmask);
		break;
	    }
	    kf_state |= KFS_NORMAL_LINES;
	}
    }
    if (thread_kill_cnt) {
	sprintf(buf,"%ld auto-kill command%s.", (long)thread_kill_cnt,
		PLURAL(thread_kill_cnt));
	mention(buf);
	kill_mentioned = true;
    }
    if (thread_select_cnt) {
	sprintf(buf,"%ld auto-select command%s.", (long)thread_select_cnt,
		PLURAL(thread_select_cnt));
	mention(buf);
	kill_mentioned = true;
    }
    if (last_kill_type) {
	if (perform_status_end(ngptr->toread,"article")) {
	    kill_mentioned = true;
	    carriage_return();
	    fputs(msg, stdout);
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
    else if (ap->flags & sel_mask) {
	ap->flags &= ~sel_mask;
	if (!selected_count--)
	    selected_count = 0;
    }
    return false;
}

void kill_unwanted(ART_NUM starting, char *message, int entering)
{
    bool intr = false;			/* did we get an interrupt? */
    ART_NUM oldfirst;
    char oldmode = mode;
    bool anytokill = (ngptr->toread > 0);

    set_mode('r','k');
    if ((entering || exitcmds) && (localkfp || globkfp)) {
	exitcmds = false;
	oldfirst = firstart;
	firstart = starting;
	clear();
	if (message && (verbose || entering))
	    fputs(message,stdout) FLUSH;

	kill_mentioned = false;
	if (localkfp) {
	    if (entering)
		kf_state |= KFS_LOCAL_CHANGES;
	    intr = do_kfile(localkfp,entering);
	}
	open_kfile(KF_GLOBAL);		/* Just in case the name changed */
	if (globkfp && !intr)
	    intr = do_kfile(globkfp,entering);
	newline();
	if (entering && kill_mentioned && novice_delays) {
	    if (verbose)
		get_anything();
	    else
		pad(just_a_sec);
	}
	if (anytokill)			/* if there was anything to kill */
	    forcelast = false;		/* allow for having killed it all */
	firstart = oldfirst;
    }
    if (!entering && (kf_state & KFS_LOCAL_CHANGES) && !intr)
	rewrite_kfile(lastart);
    set_mode(gmode,oldmode);
}

static FILE* newkfp;

static int write_local_thread_commands(int keylen, HASHDATUM *data, int extra)
{
    ARTICLE* ap = (ARTICLE*)data->dat_ptr;
    int autofl = ap->autofl;
    char ch;

    if (autofl && ((ap->flags & AF_EXISTS) || ap->child1)) {
	int i;
	/* The arrays are in priority order, so find highest priority bit. */
	for (i = 0; thread_cmd_ltr[i]; i++) {
	    if (autofl & thread_cmd_flag[i]) {
		ch = thread_cmd_ltr[i];
		break;
	    }
	}
	fprintf(newkfp,"%s T%c\n", ap->msgid, ch);
    }
    return 0;
}

void rewrite_kfile(ART_NUM thru)
{
    bool has_content = (kf_state & (KFS_THREAD_LINES|KFS_GLOBAL_THREADFILE))
				 == KFS_THREAD_LINES;
    bool has_star_commands = false;
    bool needs_newline = false;
    char* killname = filexp(get_val("KILLLOCAL",killlocal));
    char* bp;

    if (localkfp)
	fseek(localkfp,0L,0);		/* rewind current file */
    else
	makedir(killname,MD_FILE);
    UNLINK(killname);			/* to prevent file reuse */
    kf_state &= ~(kfs_local_change_clear | KFS_NORMAL_LINES);
    if ((newkfp = fopen(killname,"w")) != nullptr) {
	fprintf(newkfp,"THRU %s %ld\n",ngptr->rc->name,(long)thru);
	while (localkfp && fgets(buf,LBUFLEN,localkfp) != nullptr) {
	    if (strnEQ(buf,"THRU",4)) {
		char* cp = buf+4;
		int len = strlen(ngptr->rc->name);
		while (isspace(*cp)) cp++;
		if (isdigit(*cp))
		    continue;
		if (strnNE(cp, ngptr->rc->name, len)
		 || (cp[len] && !isspace(cp[len]))) {
		    fputs(buf,newkfp);
		    needs_newline = !index(buf,'\n');
		}
		continue;
	    }
	    for (bp = buf; isspace(*bp); bp++) ;
	    /* Leave out any outdated thread commands */
	    if (*bp == 'T' || *bp == '<')
		continue;
	    /* Write star commands after other kill commands */
	    if (*bp == '*')
		has_star_commands = true;
	    else {
		fputs(buf,newkfp);
		needs_newline = !index(bp,'\n');
	    }
	    has_content = true;
	}
	if (needs_newline)
	    putc('\n', newkfp);
	if (has_star_commands) {
	    fseek(localkfp,0L,0);			/* rewind file */
	    while (fgets(buf,LBUFLEN,localkfp) != nullptr) {
		for (bp = buf; isspace(*bp); bp++) ;
		if (*bp == '*') {
		    fputs(buf,newkfp);
		    needs_newline = !index(bp,'\n');
		}
	    }
	    if (needs_newline)
		putc('\n', newkfp);
	}
	if (!(kf_state & KFS_GLOBAL_THREADFILE)) {
	    /* Append all the still-valid thread commands */
	    hashwalk(msgid_hash, write_local_thread_commands, 0);
	}
	fclose(newkfp);
	if (!has_content)
	    UNLINK(killname);
	open_kfile(KF_LOCAL);		/* and reopen local file */
    }
    else
	printf(cantcreate,buf) FLUSH;
}

static int write_global_thread_commands(int keylen, HASHDATUM *data, int appending)
{
    int autofl;
    int i, age;
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
    for (i = 0; thread_cmd_ltr[i]; i++) {
	if (autofl & thread_cmd_flag[i]) {
	    ch = thread_cmd_ltr[i];
	    break;
	}
    }
    fprintf(newkfp,"%s %c %ld\n", msgid, ch, kf_daynum - age);
    kf_thread_cnt++;

    return 0;
}

static int age_thread_commands(int keylen, HASHDATUM *data, int elapsed_days)
{
    if (data->dat_len) {
	int age = (data->dat_len & KF_AGE_MASK) + elapsed_days;
	if (age > KF_MAXDAYS) {
	    free(data->dat_ptr);
	    kf_changethd_cnt++;
	    return -1;
	}
	data->dat_len += elapsed_days;
    }
    else {
	ARTICLE* ap = (ARTICLE*)data->dat_ptr;
	if (ap->autofl & AUTO_OLD) {
	    ap->autofl &= ~AUTO_OLD;
	    kf_changethd_cnt++;
	    kf_state |= KFS_THREAD_CHANGES;
	}
    }
    return 0;
}

void update_thread_kfile()
{
    char* cp;
    int elapsed_days;

    if (!(kf_state & KFS_GLOBAL_THREADFILE))
	return;

    elapsed_days = KF_DAYNUM(kf_daynum);
    if (elapsed_days) {
	hashwalk(msgid_hash, age_thread_commands, elapsed_days);
	kf_daynum += elapsed_days;
    }

    if (!(kf_state & KFS_THREAD_CHANGES))
	return;

    cp = filexp(get_val("KILLTHREADS", killthreads));
    makedir(cp,MD_FILE);
    if (kf_changethd_cnt*5 > kf_thread_cnt) {
	UNLINK(cp);			/* to prevent file reuse */
	if ((newkfp = fopen(cp,"w")) == nullptr)
	    return; /*$$ Yikes! */
	kf_thread_cnt = kf_changethd_cnt = 0;
	hashwalk(msgid_hash, write_global_thread_commands, 0); /* Rewrite */
    }
    else {
	if ((newkfp = fopen(cp, "a")) == nullptr)
	    return; /*$$ Yikes! */
	hashwalk(msgid_hash, write_global_thread_commands, 1); /* Append */
    }
    fclose(newkfp);

    kf_state &= ~KFS_THREAD_CHANGES;
}

void change_auto_flags(ARTICLE *ap, int auto_flag)
{
    if (auto_flag > (ap->autofl & (AUTO_KILLS|AUTO_SELS))) {
	if (ap->autofl & AUTO_OLD)
	    kf_changethd_cnt++;
	ap->autofl = auto_flag;
	kf_state |= kfs_thread_change_set;
    }
}

void clear_auto_flags(ARTICLE *ap)
{
    if (ap->autofl) {
	if (ap->autofl & AUTO_OLD)
	    kf_changethd_cnt++;
	ap->autofl = 0;
	kf_state |= kfs_thread_change_set;
    }
}

void perform_auto_flags(ARTICLE *ap, int thread_autofl, int subj_autofl, int chain_autofl)
{
    if (thread_autofl & AUTO_SEL_THD) {
	if (sel_mode == SM_THREAD)
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
	if (sel_mode == SM_THREAD)
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

int edit_kfile()
{
    int r = -1;
    char* bp;

    if (in_ng) {
	if (kf_state & KFS_LOCAL_CHANGES)
	    rewrite_kfile(lastart);
	if (!(kf_state & KFS_GLOBAL_THREADFILE)) {
	    SUBJECT* sp;
	    for (sp = first_subject; sp; sp = sp->next)
		clear_subject(sp);
	}
	strcpy(buf,filexp(get_val("KILLLOCAL",killlocal)));
    } else
	strcpy(buf,filexp(get_val("KILLGLOBAL",killglobal)));
    if ((r = makedir(buf,MD_FILE)) == 0) {
	sprintf(cmd_buf,"%s %s",
	    filexp(get_val("VISUAL",get_val("EDITOR",defeditor))),buf);
	printf("\nEditing %s KILL file:\n%s\n",
	    (in_ng?"local":"global"),cmd_buf) FLUSH;
	termdown(3);
	resetty();			/* make sure tty is friendly */
	r = doshell(sh,cmd_buf);/* invoke the shell */
	noecho();			/* and make terminal */
	crmode();			/*   unfriendly again */
	open_kfile(in_ng);
	if (localkfp) {
	    fseek(localkfp,0L,0);			/* rewind file */
	    kf_state &= ~KFS_NORMAL_LINES;
	    while (fgets(buf,LBUFLEN,localkfp) != nullptr) {
		for (bp = buf; isspace(*bp); bp++) ;
		if (*bp == '/' || *bp == '*')
		    kf_state |= KFS_NORMAL_LINES;
		else if (*bp == '<') {
		    ARTICLE* ap;
		    char* cp = index(bp,' ');
		    if (!cp)
			cp = ",";
		    else
			*cp++ = '\0';
		    if ((ap = get_article(bp)) != nullptr) {
			if (*cp == 'T')
			    cp++;
			if ((cp = index(thread_cmd_ltr, *cp)) != nullptr)
			    ap->autofl |= thread_cmd_flag[cp-thread_cmd_ltr];
		    }
		}
	    }
	}
    }
    else {
	printf("Can't make %s\n",buf) FLUSH;
	termdown(1);
    }
    return r;
}

void open_kfile(int local)
{
    char* kname = filexp(local ?
	get_val("KILLLOCAL",killlocal) :
	get_val("KILLGLOBAL",killglobal)
	);

    /* delete the file if it is empty */
    if (stat(kname,&filestat) >= 0 && !filestat.st_size)
	UNLINK(kname);
    if (local) {
	if (localkfp)
	    fclose(localkfp);
	localkfp = fopen(kname,"r");
    }
    else {
	if (globkfp)
	    fclose(globkfp);
	globkfp = fopen(kname,"r");
    }
}

void kf_append(char *cmd, bool local)
{
    strcpy(cmd_buf, filexp(local? get_val("KILLLOCAL",killlocal)
				: get_val("KILLGLOBAL",killglobal)));
    if (makedir(cmd_buf,MD_FILE) == 0) {
	if (verbose)
	    printf("\nDepositing command in %s...",cmd_buf);
	else
	    printf("\n--> %s...",cmd_buf);
	fflush(stdout);
	if (novice_delays)
	    sleep(2);
	if ((tmpfp = fopen(cmd_buf,"a+")) != nullptr) {
	    char ch;
	    if (fseek(tmpfp,-1L,2) < 0)
		ch = '\n';
	    else
		ch = getc(tmpfp);
	    fseek(tmpfp,0L,2);
	    if (ch != '\n')
		putc('\n', tmpfp);
	    fprintf(tmpfp,"%s\n",cmd);
	    fclose(tmpfp);
	    if (local && !localkfp)
		open_kfile(KF_LOCAL);
	    fputs("done\n",stdout) FLUSH;
	}
	else
	    printf(cantopen,cmd_buf) FLUSH;
	termdown(2);
    }
    kf_state |= KFS_NORMAL_LINES;
}
