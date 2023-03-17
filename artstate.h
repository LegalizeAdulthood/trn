/* artstate.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT bool reread INIT(false);		/* consider current art temporarily */
					/* unread? */

EXT bool do_fseek INIT(false);	/* should we back up in article file? */

EXT bool oldsubject INIT(false);	/* not 1st art in subject thread */
EXT ART_LINE topline INIT(-1);		/* top line of current screen */
EXT bool do_hiding INIT(true);		/* hide header lines with -h? */
EXT bool is_mime INIT(false);		/* process mime in an article? */
EXT bool multimedia_mime INIT(false);	/* images/audio to see/hear? */
EXT bool rotate INIT(false);		/* has rotation been requested? */
EXT char* prompt;			/* pointer to current prompt */

EXT char* firstline INIT(nullptr);		/* special first line? */
#ifdef CUSTOMLINES
EXT char* hideline INIT(nullptr);		/* custom line hiding? */
EXT char* pagestop INIT(nullptr);		/* custom page terminator? */
EXT COMPEX hide_compex;
EXT COMPEX page_compex;
#endif
