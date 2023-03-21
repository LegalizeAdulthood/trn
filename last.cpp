/* last.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "trn.h"
#include "util.h"
#include "util2.h"
#include "init.h"
#include "last.h"

char *g_lastngname{}; /* last newsgroup read */
long g_lasttime{};    /* time last we ran */
long g_lastactsiz{};  /* last known size of active file */
long g_lastnewtime{}; /* time of last newgroup request */
long g_lastextranum{};

static char *s_lastfile{}; /* path name of .rnlast file */
static long s_starttime{};

void last_init()
{
    s_lastfile = savestr(filexp(LASTNAME));

    s_starttime = (long)time((time_t*)nullptr);
    readlast();
}

void readlast()
{
    if ((tmpfp = fopen(s_lastfile,"r")) != nullptr) {
	if (fgets(buf,sizeof buf,tmpfp) != nullptr) {
	    long old_last = g_lasttime;
	    buf[strlen(buf)-1] = '\0';
	    if (*buf) {
		safefree0(g_lastngname);
		g_lastngname = savestr(buf);
	    }
	    fscanf(tmpfp,"%ld %ld %ld %ld",&g_lasttime,&g_lastactsiz,
					   &g_lastnewtime,&g_lastextranum);
	    if (!g_lastnewtime)
		g_lastnewtime = s_starttime;
	    if (old_last > g_lasttime)
		g_lasttime = old_last;
	}
	fclose(tmpfp);
    }
}

/* Put out certain values for next run of trn */

void writelast()
{
    sprintf(buf,"%s.%ld", s_lastfile, g_our_pid);
    if ((tmpfp = fopen(buf,"w")) != nullptr) {
	if (g_lasttime < s_starttime)
	    g_lasttime = s_starttime;
	fprintf(tmpfp,"%s\n%ld\n%ld\n%ld\n%ld\n",
		ngname? ngname : "",g_lasttime,
		g_lastactsiz,g_lastnewtime,g_lastextranum);
	fclose(tmpfp);
	remove(s_lastfile);
	rename(buf,s_lastfile);
    }
    else {
	printf(cantcreate,buf) FLUSH;
	/*termdown(1);*/
    }
}
