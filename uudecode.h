/* uudecode.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* Length of a normal uuencoded line, including newline */
#define UULENGTH 62

int uue_prescan(char *, char **, int *, int *);
int uudecode(FILE *, int);
