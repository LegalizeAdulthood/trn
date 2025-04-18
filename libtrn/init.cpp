/* init.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/init.h"

#include "nntp/nntpinit.h"
#include "trn/ngdata.h"
#include "trn/addng.h"
#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artsrch.h"
#include "trn/backpage.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "trn/decode.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/help.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/last.h"
#include "trn/mempool.h"
#include "trn/mime.h"
#include "trn/ng.h"
#include "trn/ngsrch.h"
#include "trn/ngstuff.h"
#include "trn/only.h"
#include "trn/opt.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/respond.h"
#include "trn/rthread.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/univ.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#ifdef _WIN32
#include <process.h>
#endif

#include <cstdio>
#include <cstdlib>

long g_our_pid{};

bool initialize(int argc, char *argv[])
{
    char *tcbuf = safe_malloc(TCBUF_SIZE); // make temp buffer for termcap and
                                          // other initialization stuff

    g_our_pid = (long)getpid();

    // init terminal

    term_init();                        // must precede opt_init() so that
                                        // ospeed is set for baud-rate
                                        // switches.  Actually terminal
                                        // mode setting is in term_set()
    mp_init();

    // init syntax etc. for searching (must also precede opt_init())

    search_init();

    // we have to know g_rn_lib to look up global switches in %X/INIT

    env_init(tcbuf, true);
    head_init();

    // decode switches

    opt_init(argc,argv,&tcbuf);         // must not do % interps!
                                        // (but may mung environment)
    color_init();

    // init signals, status flags

    final_init();

    // start up file expansion and the % interpreter

    interp_init(tcbuf, TCBUF_SIZE);

    // now make sure we have a current working directory

    if (!g_check_flag)
    {
        cwd_check();
    }

    if (init_nntp() < 0)
    {
        finalize(1);
    }

    // if we aren't just checking, turn off echo

    if (!g_check_flag)
    {
        term_set(tcbuf);
    }

    // get info on last trn run, if any

    last_init();

    std::free(tcbuf);                        // recover 1024 bytes

    univ_init();

    // check for news news

    if (!g_check_flag)
    {
        news_news_check();
    }

    // process the newsid(s) and associate the newsrc(s)

    data_source_init();
    newsgroup_data_init();

    // now read in the .newsrc file(s)

    bool foundany = rcstuff_init();

    // it looks like we will actually read something, so init everything

    add_ng_init();
    art_init();
    art_io_init();
    art_search_init();
    back_page_init();
    bits_init();
    cache_init();
    help_init();
    kill_file_init();
    mime_init();
    ng_init();
    newsgroup_search_init();
    newsgroup_stuff_init();
    only_init();
    rcln_init();
    respond_init();
    trn_init();
    decode_init();
    thread_init();
    util_init();
    xmouse_init(argv[0]);

    write_last();        // remember last runtime in .rnlast

    if (g_max_newsgroup_to_do)                    // patterns on command line?
    {
        foundany |= scan_active(true);
    }

    return foundany;
}

void news_news_check()
{
    const char *newsnewsname = file_exp(NEWSNEWSNAME);
    if (std::FILE *fp = std::fopen(newsnewsname, "r"))
    {
        stat_t news_news_stat{};
        fstat(fileno(fp),&news_news_stat);
        if (news_news_stat.st_mtime > (std::time_t) g_last_time)
        {
            while (std::fgets(g_buf,sizeof(g_buf),fp) != nullptr)
            {
                std::fputs(g_buf, stdout);
            }
            get_anything();
            std::putchar('\n');
        }
        std::fclose(fp);
    }
}
