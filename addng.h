/* addng.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#ifndef TRN_ADDNG_H
#define TRN_ADDNG_H

#include <cstdint>

#include "enum-flags.h"

struct DATASRC;

enum addgroup_flags : std::uint8_t
{
    AGF_NONE = 0x00,
    AGF_SEL = 0x01,
    AGF_DEL = 0x02,
    AGF_DELSEL = 0x04,
    AGF_INCLUDED = 0x10,
    AGF_EXCLUDED = 0x20
};
DECLARE_FLAGS_ENUM(addgroup_flags, std::uint8_t);

struct ADDGROUP
{
    ADDGROUP      *next;
    ADDGROUP      *prev;
    DATASRC       *datasrc;
    ART_NUM        toread; /* number of articles to be read (for sorting) */
    NG_NUM         num;    /* a possible sort order for this group */
    addgroup_flags flags;
    char           name[1];
};

extern ADDGROUP* g_first_addgroup;
extern ADDGROUP* g_last_addgroup;
extern ADDGROUP* g_sel_page_gp;
extern ADDGROUP* g_sel_next_gp;

void addng_init();
bool find_new_groups();
bool scanactive(bool add_matching);
void sort_addgroups();

#endif
