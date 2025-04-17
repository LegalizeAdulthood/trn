/* final.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/fdio.h>

#include "config/common.h"
#include "trn/final.h"

#include "nntp/nntpclient.h"
#include "nntp/nntpinit.h"
#include "trn/list.h"
#include "trn/bits.h"
#include "trn/change_dir.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/rcstuff.h"
#include "trn/scoresave.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

#ifdef HAS_SIGBLOCK
#ifndef sigmask
#define sigmask(m)      (1 << ((m)-1))
#endif
#endif

bool g_panic{};       /* we got hung up or something-- so leave tty alone */
bool g_doing_ng{};    /* do we need to reconstitute current rc line? */
char g_int_count{};   /* how many interrupts we've had */
bool g_bos_on_stop{}; /* set when handling the stop signal would leave the screen a mess */

void final_init()
{
#ifdef SIGTSTP
    sigset(SIGTSTP, stop_catcher);      /* job control signals */
    sigset(SIGTTOU, stop_catcher);      /* job control signals */
    sigset(SIGTTIN, stop_catcher);      /* job control signals */
#endif

    sigset(SIGINT, int_catcher);        /* always catch interrupts */
#ifdef SIGHUP
    sigset(SIGHUP, sig_catcher);        /* and hangups */
#endif
#ifdef SIGWINCH
    sigset(SIGWINCH, winch_catcher);
#endif
#ifdef SIGPIPE
    sigset(SIGPIPE,pipe_catcher);
#endif

#ifndef lint
#ifdef SIGEMT
    sigignore(SIGEMT);          /* Ignore EMT signals from old [t]rn's */
#endif
#endif

#ifdef DEBUG
    /* sometimes we WANT a core dump */
    if (debug & DEB_COREDUMPSOK)
        return;
#endif
    sigset(SIGILL, sig_catcher);
#ifdef SIGTRAP
    sigset(SIGTRAP, sig_catcher);
#endif
    sigset(SIGFPE, sig_catcher);
#ifdef SIGBUS
    sigset(SIGBUS, sig_catcher);
#endif
    sigset(SIGSEGV, sig_catcher);
#ifdef SIGSYS
    sigset(SIGSYS, sig_catcher);
#endif
    sigset(SIGTERM, sig_catcher);
#ifdef SIGXCPU
    sigset(SIGXCPU, sig_catcher);
#endif
#ifdef SIGXFSZ
    sigset(SIGXFSZ, sig_catcher);
#endif
}

[[noreturn]] //
void finalize(int status)
{
    sc_sv_save_file();   /* save any scores from memory to disk */
    update_thread_kill_file();
    color_default();
    termlib_reset();
    if (g_bizarre)
    {
        resetty();
    }
    xmouse_off();       /* turn off mouse tracking (if on) */
    std::fflush(stdout);

    change_dir(g_tmp_dir);
    if (!g_check_flag)
    {
        unuse_multirc(g_multirc);
    }
    data_source_finalize();
    for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
    {
        char *s = nntp_tmp_name(i);
        remove(s);
    }
    cleanup_nntp();
    if (!g_head_name.empty())
    {
        remove(g_head_name.c_str());
    }
    if (status < 0)
    {
        sigset(SIGILL,SIG_DFL);
#ifdef HAS_SIGBLOCK
        sigsetmask(sigblock(0) & ~(sigmask(SIGILL) | sigmask(SIGIOT)));
#endif
        std::abort();
    }
#ifdef RESTORE_ORIGDIR
    if (!g_orig_dir.empty())
    {
        change_dir(g_orig_dir);
    }
#endif
    std::exit(status);
}

/* come here on interrupt */

Signal_t int_catcher(int dummy)
{
    sigset(SIGINT,int_catcher);
#ifdef DEBUG
    if (debug)
    {
        write(2,"int_catcher\n",12);
    }
#endif
    if (!g_waiting)
    {
        if (g_int_count++)              /* was there already an interrupt? */
        {
            if (g_int_count == 3 || g_int_count > 5)
            {
                write(2,"\nBye-bye.\n",10);
                sig_catcher(0);         /* emulate the other signals */
            }
            write(2,"\n(Interrupt -- one more to kill trn)\n",37);
        }
    }
}

/* come here on signal other than interrupt, stop, or cont */

Signal_t sig_catcher(int signo)
{
    static char *signame[]{
        "",
        "HUP",
        "INT",
        "QUIT",
        "ILL",
        "TRAP",
        "IOT",
        "EMT",
        "FPE",
        "KILL",
        "BUS",
        "SEGV",
        "SYS",
        "PIPE",
        "ALRM",
        "TERM",
        "???"
#ifdef SIGTSTP
        ,"STOP",
        "TSTP",
        "CONT",
        "CHLD",
        "TTIN",
        "TTOU",
        "TINT",
        "XCPU",
        "XFSZ"
#ifdef SIGPROF
        ,"VTALARM",
        "PROF"
#endif
#endif
        };

#ifdef DEBUG
    if (debug)
    {
        std::printf("\nSIG%s--.newsrc not restored in debug\n",signame[signo]);
        finalize(-1);
    }
#endif
    if (g_panic)
    {
#ifdef HAS_SIGBLOCK
        sigsetmask(sigblock(0) & ~(sigmask(SIGILL) | sigmask(SIGIOT)));
#endif
        std::abort();
    }
    (void) sigset(SIGILL,SIG_DFL);
    g_panic = true;                     /* disable terminal I/O */
    if (g_doing_ng)                     /* need we reconstitute rc line? */
    {
        yank_back();
        bits_to_rc();                   /* then do so (hope this works) */
    }
    g_doing_ng = false;
    if (!write_newsrcs(g_multirc))      /* write anything that's changed */
    {
        /* TODO: get_old_newsrcs(g_multirc);  ?? */
    }
    update_thread_kill_file();

#ifdef SIGHUP
    if (signo != SIGHUP)
#endif
    {
        if (g_verbose)
        {
            std::printf("\nCaught %s%s--.newsrc restored\n", signo ? "a SIG" : "an internal error", signame[signo]);
        }
        else
        {
            std::printf("\nSignal %d--bye bye\n", signo);
        }
    }
    switch (signo)
    {
#ifdef SIGBUS
    case SIGBUS:
#endif
    case SIGILL:
    case SIGSEGV:
        finalize(-signo);
    }
    finalize(1);                                /* and blow up */
}

Signal_t pipe_catcher(int signo)
{
#ifdef SIGPIPE
    ;/* TODO: we lost the current nntp connection */
    sigset(SIGPIPE,pipe_catcher);
#endif
}

/* come here on stop signal */

#ifdef SIGTSTP
Signal_t stop_catcher(int signo)
{
    if (!g_waiting)
    {
        xmouse_off();
        checkpoint_newsrcs();   /* good chance of crash while stopped */
        if (g_bos_on_stop)
        {
            goto_xy(0, g_tc_LINES-1);
            std::putchar('\n');
        }
        termlib_reset();
        resetty();              /* this is the point of all this */
#ifdef DEBUG
        if (debug)
        {
            write(2,"stop_catcher\n",13);
        }
#endif
        std::fflush(stdout);
        sigset(signo,SIG_DFL);  /* enable stop */
#ifdef HAS_SIGBLOCK
        sigsetmask(sigblock(0) & ~(1 << (signo-1)));
#endif
        kill(0,signo);          /* and do the stop */
        savetty();
#ifdef MAILCALL
        g_mailcount = 0;                    /* force recheck */
#endif
        if (!g_panic)
        {
            if (!g_waiting)
            {
                termlib_init();
                noecho();                       /* set no echo */
                crmode();                       /* set cbreak mode */
                forceme("\f");                  /* cause a refresh */
                                                /* (defined only if TIOCSTI defined) */
                errno = 0;                      /* needed for getcmd */
            }
        }
        xmouse_check();
    }
    sigset(signo,stop_catcher); /* unenable the stop */
}
#endif
