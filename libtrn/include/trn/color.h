/* trn/color.h - Color handling for trn 4.0.
 */
/* This software is copyrighted as detailed in the LICENSE file, and
 * this file is also Copyright 1995 by Gran Larsson <hoh@approve.se>. */
#ifndef TRN_COLOR_H
#define TRN_COLOR_H

/*
** Object numbers.
*/

enum object_number
{
    COLOR_DEFAULT = 0,
    COLOR_NGNAME,    /* NG name in thread selector      */
    COLOR_PLUS,      /* + in thread selector            */
    COLOR_MINUS,     /* - in thread selector            */
    COLOR_STAR,      /* * in thread selector            */
    COLOR_HEADER,    /* headers in article display      */
    COLOR_SUBJECT,   /* subject in article display      */
    COLOR_TREE,      /* tree in article display         */
    COLOR_TREE_MARK, /* tree in article display, marked */
    COLOR_MORE,      /* the more prompt                 */
    COLOR_HEADING,   /* any heading                     */
    COLOR_CMD,       /* the command prompt              */
    COLOR_MOUSE,     /* the mouse bar                   */
    COLOR_NOTICE,    /* notices, e.g. kill handling     */
    COLOR_SCORE,     /* score objects                   */
    COLOR_ARTLINE1,  /* the article heading             */
    COLOR_MIMESEP,   /* the multipart mime separator    */
    COLOR_MIMEDESC,  /* the multipart mime description line */
    COLOR_CITEDTEXT, /* cited text color                */
    COLOR_BODYTEXT,  /* regular body text               */
    MAX_COLORS
};

void color_init();
void color_rc_attribute(const char *object, char *value);
void color_object(int object, bool push);
void color_pop();
void color_string(int object, const char *str);
void color_default();

#endif
