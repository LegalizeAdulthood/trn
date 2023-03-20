/* This file is Copyright 1992, 1993 by Clifford A. Adams */
/* scanart.c
 *
 * article scan mode: screen oriented article interface
 * based loosely on the older "SubScan" mode.
 * Interface routines to the rest of trn
 */

#include "EXTERN.h"
#include "common.h"
#include "hash.h"
#include "cache.h"
#include "ng.h"		/* variable art, the next article to read. */
#include "ngdata.h"
#include "term.h"	/* macro to clear... */
#include "bits.h"	/* absfirst */
#include "artsrch.h"	/* needed for artstate.h */
#include "artstate.h"	/* for reread */
#include "rt-select.h"	/* selected_only */
#include "scan.h"
#include "smisc.h"
#include "spage.h"
#include "INTERN.h"
#include "scanart.h"		/* ordering dependency */
#include "EXTERN.h"
#include "samain.h"
#include "samisc.h"

sa_main_result sa_main()
{
    char sa_oldmode;	/* keep mode of caller */

    sa_in = true;
    sa_go = false;	/* ...do not collect $200... */
    s_follow_temp = false;

    if (lastart < absfirst) {
	s_beep();
	return SA_QUIT;
    }
    if (!sa_initialized) {
	sa_init();
	if (!sa_initialized)		/* still not working... */
	    return SA_ERR;		/* we don't belong here */
	sa_never_initialized = false;	/* we have entered at least once */
    } else
	s_change_context(sa_scan_context);

    /* unless "explicit" entry, read any marked articles */
    if (!sa_go_explicit) {
	long a;
	a = sa_readmarked_elig();
	if (a) {	/* there was an article */
	    art = sa_ents[a].artnum;
	    reread = true;
	    sa_clearmark(a);
	    /* trn 3.x won't read an unselected article if selected_only */
	    selected_only = false;
	    s_save_context();
	    return SA_NORM;
	}
    }
    sa_go_explicit = false;

    /* If called from the trn thread-selector and articles/threads were
     * selected there, "select" the articles and enter the zoom mode.
     */
    if (sa_do_selthreads) {
	sa_selthreads();
	sa_do_selthreads = false;
	sa_mode_zoom = true;		/* zoom in by default */
	s_top_ent = -1;		/* go to top of arts... */
    }

    sa_oldmode = mode;			/* save mode */
    mode = 's';				/* for RN macros */
    sa_main_result i = sa_mainloop();
    mode = sa_oldmode;			/* restore mode */

    if (i == SA_NORM || i == SA_FAKE) {
	art = sa_art;
	/* trn 3.x won't read an unselected article if selected_only */
	selected_only = false;
	if (sa_mode_read_elig)
	    reread = true;
    }
    if (sa_scan_context >= 0)
	s_save_context();
    return i;
}

/* called when more articles arrive */
void sa_grow(ART_NUM oldlast, ART_NUM last)
{
    if (!sa_initialized)
	return;
    sa_growarts(oldlast,last);
}

void sa_cleanup()
{
    /* we might be called by other routines which aren't sure
     * about the scan status
     */
    if (!sa_initialized)
	return;

    sa_cleanmain();
    clear();		/* should something else clear the screen? */
    sa_initialized = false;		/* goodbye... */
}
