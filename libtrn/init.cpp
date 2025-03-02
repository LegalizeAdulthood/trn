/* init.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "init.h"

#include "addng.h"
#include "art.h"
#include "artio.h"
#include "artsrch.h"
#include "backpage.h"
#include "bits.h"
#include "cache.h"
#include "color.h"
#include "datasrc.h"
#include "decode.h"
#include "env.h"
#include "final.h"
#include "head.h"
#include "help.h"
#include "intrp.h"
#include "kfile.h"
#include "last.h"
#include "mempool.h"
#include "mime.h"
#include "ng.h"
#include "ngdata.h"
#include "ngsrch.h"
#include "ngstuff.h"
#include "nntpinit.h"
#include "only.h"
#include "opt.h"
#include "rcln.h"
#include "rcstuff.h"
#include "respond.h"
#include "rthread.h"
#include "terminal.h"
#include "trn.h"
#include "univ.h"
#include "util.h"
#include "util2.h"

#ifdef _WIN32
#include <process.h>
#endif

long g_our_pid{};

bool initialize(int argc, char *argv[])
{
    char *tcbuf = safemalloc(TCBUF_SIZE); /* make temp buffer for termcap and */
                                          /* other initialization stuff */

    g_our_pid = (long)getpid();

    /* init terminal */

    term_init();                        /* must precede opt_init() so that */
                                        /* ospeed is set for baud-rate */
                                        /* switches.  Actually terminal */
                                        /* mode setting is in term_set() */
    mp_init();

    /* init syntax etc. for searching (must also precede opt_init()) */

    search_init();

    /* we have to know g_rn_lib to look up global switches in %X/INIT */

    env_init(tcbuf, true);
    head_init();

    /* decode switches */

    opt_init(argc,argv,&tcbuf);         /* must not do % interps! */
                                        /* (but may mung environment) */
    color_init();

    /* init signals, status flags */

    final_init();

    /* start up file expansion and the % interpreter */

    intrp_init(tcbuf, TCBUF_SIZE);

    /* now make sure we have a current working directory */

    if (!g_checkflag)
    {
        cwd_check();
    }

    if (init_nntp() < 0)
    {
        finalize(1);
    }

    /* if we aren't just checking, turn off echo */

    if (!g_checkflag)
    {
        term_set(tcbuf);
    }

    /* get info on last trn run, if any */

    last_init();

    free(tcbuf);                        /* recover 1024 bytes */

    univ_init();

    /* check for news news */

    if (!g_checkflag)
    {
        newsnews_check();
    }

    /* process the newsid(s) and associate the newsrc(s) */

    datasrc_init();
    ngdata_init();

    /* now read in the .newsrc file(s) */

    bool foundany = rcstuff_init();

    /* it looks like we will actually read something, so init everything */

    addng_init();
    art_init();
    artio_init();
    artsrch_init();
    backpage_init();
    bits_init();
    cache_init();
    help_init();
    kfile_init();
    mime_init();
    ng_init();
    ngsrch_init();
    ngstuff_init();
    only_init();
    rcln_init();
    respond_init();
    trn_init();
    decode_init();
    thread_init();
    util_init();
    xmouse_init(argv[0]);

    writelast();        /* remember last runtime in .rnlast */

    if (g_maxngtodo)                    /* patterns on command line? */
    {
        foundany |= scanactive(true);
    }

    return foundany;
}

void newsnews_check()
{
    const char *newsnewsname = filexp(NEWSNEWSNAME);
    if (FILE *fp = fopen(newsnewsname, "r"))
    {
        stat_t news_news_stat{};
        fstat(fileno(fp),&news_news_stat);
        if (news_news_stat.st_mtime > (time_t)g_lasttime) {
            while (fgets(g_buf,sizeof(g_buf),fp) != nullptr)
            {
                fputs(g_buf, stdout);
            }
            get_anything();
            putchar('\n');
        }
        fclose(fp);
    }
}
