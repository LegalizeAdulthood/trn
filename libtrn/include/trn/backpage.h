/* trn/backpage.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_BACKPAGE_H
#define TRN_BACKPAGE_H

#include <config/typedef.h>

/* things for doing the 'back page' command */

void backpage_init();
ART_POS vrdary(ART_LINE indx);
void vwtary(ART_LINE indx, ART_POS newvalue);

#endif
