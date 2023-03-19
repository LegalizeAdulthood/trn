/* backpage.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* things for doing the 'back page' command */

EXT int varyfd INIT(0);			/* virtual array file for storing  */
					/* file offsets */
EXT ART_POS varybuf[VARYSIZE];		/* current window onto virtual array */

EXT long oldoffset INIT(-1);		/* offset to block currently in window */

void backpage_init();
ART_POS vrdary(ART_LINE indx);
void vwtary(ART_LINE indx, ART_POS newvalue);
