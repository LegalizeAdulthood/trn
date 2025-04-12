/* trn/final.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_FINAL_H
#define TRN_FINAL_H

/* cleanup status for fast exits */

extern bool g_panic;       /* we got hung up or something-- so leave tty alone */
extern bool g_doing_ng;    /* do we need to reconstitute current rc line? */
extern char g_int_count;   /* how many interrupts we've had */
extern bool g_bos_on_stop; /* set when handling the stop signal would leave the screen a mess */

void final_init();
[[noreturn]] void finalize(int status);
Signal_t int_catcher(int dummy);
Signal_t sig_catcher(int signo);
Signal_t pipe_catcher(int signo);
#ifdef SIGTSTP
Signal_t stop_catcher(int signo);
#endif

#endif
