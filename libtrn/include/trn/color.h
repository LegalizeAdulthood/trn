/* trn/color.h - Color handling for trn 4.0.
 */
/* This software is copyrighted as detailed in the LICENSE file, and
 * this file is also Copyright 1995 by Gran Larsson <hoh@approve.se>. */
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_COLOR_H
#define TRN_COLOR_H

#include <trn/terminal.h>

#include <string>
#include <string_view>

//
// Object numbers.
//

enum ObjectNumber
{
    COLOR_DEFAULT = 0,
    COLOR_NG_NAME,    // NG name in thread selector
    COLOR_PLUS,       // + in thread selector
    COLOR_MINUS,      // - in thread selector
    COLOR_STAR,       // * in thread selector
    COLOR_HEADER,     // headers in article display
    COLOR_SUBJECT,    // subject in article display
    COLOR_TREE,       // tree in article display
    COLOR_TREE_MARK,  // tree in article display, marked
    COLOR_MORE,       // the more prompt
    COLOR_HEADING,    // any heading
    COLOR_CMD,        // the command prompt
    COLOR_MOUSE,      // the mouse bar
    COLOR_NOTICE,     // notices, e.g. kill handling
    COLOR_SCORE,      // score objects
    COLOR_ART_LINE1,  // the article heading
    COLOR_MIME_SEP,   // the multipart mime separator
    COLOR_MIME_DESC,  // the multipart mime description line
    COLOR_CITE_DTEXT, // cited text color
    COLOR_BODY_TEXT,  // regular body text
    MAX_COLORS
};

struct ColorObj
{
    std::string name;
    std::string fg;
    std::string bg;
    int         attr;
};

struct ColorOutputContext
{
    void init();
    void deefault();

    enum
    {
        STACK_SIZE = 16
    };

    bool     use_colors{};
    ColorObj objects[MAX_COLORS]{};
    // The attribute stack.  The 0th element is always the "normal" object.
    ColorObj stack[STACK_SIZE]{};
    int      stack_pointer{};
    ColorObj prior{"", {}, {}, NO_MARKING};

    // TODO: private
    void output();
};

extern ColorOutputContext g_color;

void color_rc_attribute(std::string_view object, std::string_view value);
void color_object(int object, bool push);
void color_pop();
void color_string(int object, std::string_view str);

#endif
