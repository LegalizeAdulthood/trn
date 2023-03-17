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
void finalize(int)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR >= 5)
    __attribute__((noreturn))
#endif
    ;
Signal_t int_catcher(int);
Signal_t sig_catcher(int);
#ifdef SUPPORT_NNTP
Signal_t pipe_catcher(int);
#endif
#ifdef SIGTSTP
Signal_t stop_catcher(int);
#endif
