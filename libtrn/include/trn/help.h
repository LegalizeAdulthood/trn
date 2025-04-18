/* trn/help.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_HELP_H
#define TRN_HELP_H

// help locations
enum HelpLocation
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
int help_newsgroup();
int help_newsgroup_selector();
int help_add_group_selector();
int help_subs();
int help_article_selector();
int help_multirc();
int help_options();
int help_scan_article();
int help_univ();
int univ_key_help(HelpLocation where);

#endif
