/* addng.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

struct ADDGROUP {
    ADDGROUP* next;
    ADDGROUP* prev;
    DATASRC* datasrc;
    ART_NUM toread;	/* number of articles to be read (for sorting) */
    NG_NUM num;		/* a possible sort order for this group */
    char flags;
    char name[1];
};

enum
{
    AGF_SEL = 0x01,
    AGF_DEL = 0x02,
    AGF_DELSEL = 0x04,
    AGF_INCLUDED = 0x10,
    AGF_EXCLUDED = 0x20
};

EXT ADDGROUP* first_addgroup;
EXT ADDGROUP* last_addgroup;

EXT ADDGROUP* sel_page_gp;
EXT ADDGROUP* sel_next_gp;

void addng_init();
bool find_new_groups();
bool scanactive(bool add_matching);
void sort_addgroups();
