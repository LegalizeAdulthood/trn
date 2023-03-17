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

void help_init(void);
int help_page(void);
int help_art(void);
int help_ng(void);
int help_ngsel(void);
int help_addsel(void);
#ifdef ESCSUBS
int help_subs(void);
#endif
int help_artsel(void);
int help_multirc(void);
int help_options(void);
#ifdef SCAN
int help_scanart(void);
#endif
int help_univ(void);
int univ_key_help(int);
