/* help.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef HELP_H
#define HELP_H

/* help locations */
enum
{
    UHELP_PAGE = 1,
    UHELP_ART = 2,
    UHELP_NG = 3,
    UHELP_NGSEL = 4,
    UHELP_ADDSEL = 5,
    UHELP_SUBS = 6,
    UHELP_ARTSEL = 7,
    UHELP_MULTIRC = 8,
    UHELP_OPTIONS = 9,
    UHELP_SCANART = 10,
    UHELP_UNIV = 11
};

void help_init();
int help_page();
int help_art();
int help_ng();
int help_ngsel();
int help_addsel();
int help_subs();
int help_artsel();
int help_multirc();
int help_options();
int help_scanart();
int help_univ();
int univ_key_help(int where);

#endif
