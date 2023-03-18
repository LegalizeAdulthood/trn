/* help.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#define FOO 1

/* help locations */
enum
{
    UHELP_PAGE = 1,
    UHELP_ART = 2,
    UHELP_NG = 3,
    UHELP_NGSEL = 4,
    UHELP_ADDSEL = 5,
#ifdef ESCSUBS
    UHELP_SUBS = 6,
#endif
    UHELP_ARTSEL = 7,
    UHELP_MULTIRC = 8,
    UHELP_OPTIONS = 9,
#ifdef SCAN
    UHELP_SCANART = 10,
#endif
    UHELP_UNIV = 11
};

void help_init();
int help_page();
int help_art();
int help_ng();
int help_ngsel();
int help_addsel();
#ifdef ESCSUBS
int help_subs();
#endif
int help_artsel();
int help_multirc();
int help_options();
#ifdef SCAN
int help_scanart();
#endif
int help_univ();
int univ_key_help(int);
