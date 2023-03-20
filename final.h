/* final.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* cleanup status for fast exits */

EXT bool panic INIT(false);		/* we got hung up or something-- */
					/*  so leave tty alone */
EXT bool doing_ng INIT(false);		/* do we need to reconstitute */
					/* current rc line? */

EXT char int_count INIT(0);		/* how many interrupts we've had */

EXT bool bos_on_stop INIT(false);	/* set when handling the stop signal */
					/* would leave the screen a mess */

void final_init();
[[noreturn]] void finalize(int status);
Signal_t int_catcher(int dummy);
Signal_t sig_catcher(int signo);
Signal_t pipe_catcher(int signo);
#ifdef SIGTSTP
Signal_t stop_catcher(int signo);
#endif
