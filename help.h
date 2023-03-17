/* help.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

/* help locations */
#define UHELP_PAGE 1
#define UHELP_ART 2
#define UHELP_NG 3
#define UHELP_NGSEL 4
#define UHELP_ADDSEL 5
#ifdef ESCSUBS
#define UHELP_SUBS 6
#endif
#define UHELP_ARTSEL 7
#define UHELP_MULTIRC 8
#define UHELP_OPTIONS 9
#ifdef SCAN
#define UHELP_SCANART 10
#endif
#define UHELP_UNIV 11

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
