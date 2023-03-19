/* color.h - Color handling for trn 4.0.
 */
/* This software is copyrighted as detailed in the LICENSE file, and
 * this file is also Copyright 1995 by Gran Larsson <hoh@approve.se>. */

/*
** Object numbers.
*/

enum
{
    COLOR_DEFAULT = 0,
    COLOR_NGNAME = 1,     /* NG name in thread selector	*/
    COLOR_PLUS = 2,       /* + in thread selector		*/
    COLOR_MINUS = 3,      /* - in thread selector		*/
    COLOR_STAR = 4,       /* * in thread selector		*/
    COLOR_HEADER = 5,     /* headers in article display	*/
    COLOR_SUBJECT = 6,    /* subject in article display	*/
    COLOR_TREE = 7,       /* tree in article display	*/
    COLOR_TREE_MARK = 8,  /* tree in article display, marked */
    COLOR_MORE = 9,       /* the more prompt		*/
    COLOR_HEADING = 10,   /* any heading			*/
    COLOR_CMD = 11,       /* the command prompt		*/
    COLOR_MOUSE = 12,     /* the mouse bar		*/
    COLOR_NOTICE = 13,    /* notices, e.g. kill handling	*/
    COLOR_SCORE = 14,     /* score objects		*/
    COLOR_ARTLINE1 = 15,  /* the article heading		*/
    COLOR_MIMESEP = 16,   /* the multipart mime separator	*/
    COLOR_MIMEDESC = 17,  /* the multipart mime description line */
    COLOR_CITEDTEXT = 18, /* cited text color		*/
    COLOR_BODYTEXT = 19,  /* regular body text		*/
    MAX_COLORS = 20
};

void color_init();
void color_rc_attribute(const char *object, char *value);
void color_object(int object, bool push);
void color_pop();
void color_string(int object, const char *str);
void color_default();
