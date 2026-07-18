/* color.cpp -  Color handling for trn 4.0.
 */
/* This software is copyrighted as detailed in the LICENSE file, and
 * this file is also Copyright 1995 by Gran Larsson <hoh@approve.se>. */
// Copyright (c) 2026, Richard Thomson

//
// The color handling is implemented as an attribute stack containing
// foreground color, background color, and video attribute for an object.
// Objects are screen features like "thread tree", "header lines",
// "subject line", and "command prompt". The intended use is something
// like this:
//
//      color_object(COLOR_HEADER, 1);
//      fputs(header_string, stdout);
//      color_pop();
//
// The color_pop function will take care of restoring all colors and
// attribute to the state before the color_object function was called.
//
// Colors and attributes are parsed from the [attribute] section
// in the trnrc file. Escape sequences for the colors are picked up
// from term.c by calling the function tc_color_capability.
//
// If colors were specified in the [attribute] section, then colors
// are used, otherwise only normal monochrome video attributes.
//

#include <trn/color.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <trn/final.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <cstdio>
#include <string>
#include <string_view>

struct ColorObj
{
    std::string_view name;
    std::string fg;
    std::string bg;
    int attr;
};

enum
{
    STACK_SIZE = 16
};

static bool s_use_colors{};

static void output_color();

//
// Object properties.
//
// Give default attributes that are used if the trnrc file has no,
// or just a few, lines in the [attribute] section.
//

static ColorObj s_objects[MAX_COLORS] =
// clang-format off
{
    { "default",          {}, {}, NO_MARKING   },
    { "ngname",           {}, {}, STANDOUT     },
    { "plus",             {}, {}, LAST_MARKING },
    { "minus",            {}, {}, LAST_MARKING },
    { "star",             {}, {}, LAST_MARKING },
    { "header",           {}, {}, LAST_MARKING },
    { "subject",          {}, {}, UNDERLINE    },
    { "tree",             {}, {}, LAST_MARKING },
    { "tree marker",      {}, {}, STANDOUT     },
    { "more",             {}, {}, STANDOUT     },
    { "heading",          {}, {}, STANDOUT     },
    { "command",          {}, {}, STANDOUT     },
    { "mouse bar",        {}, {}, STANDOUT     },
    { "notice",           {}, {}, STANDOUT     },
    { "score",            {}, {}, STANDOUT     },
    { "art heading",      {}, {}, LAST_MARKING },
    { "mime separator",   {}, {}, STANDOUT     },
    { "mime description", {}, {}, UNDERLINE    },
    { "cited text",       {}, {}, LAST_MARKING },
    { "body text",        {}, {}, NO_MARKING   },
};
// clang-format on

// The attribute stack.  The 0th element is always the "normal" object.
static ColorObj s_color_stack[STACK_SIZE];
static int      s_stack_pointer{};

// Initialize color support after trnrc is read.
void color_init()
{
    if (s_use_colors)
    {
        // Get default capabilities.
        const char *fg_capability = tc_color_capability("fg default");
        if (fg_capability == nullptr)
        {
            std::fprintf(stderr, "trn: you need a 'fg default' definition in the [termcap] section.\n");
            finalize(1);
        }
        const char *bg_capability = tc_color_capability("bg default");
        if (bg_capability == nullptr)
        {
            std::fprintf(stderr, "trn: you need a 'bg default' definition in the [termcap] section.\n");
            finalize(1);
        }
        std::string_view fg{fg_capability};
        std::string_view bg{bg_capability};
        if (fg == bg)
        {
            bg = "";
        }
        for (ColorObj &obj : s_objects)
        {
            if (obj.fg.empty())
            {
                obj.fg.assign(fg.data(), fg.size());
            }
            if (obj.bg.empty())
            {
                obj.bg.assign(bg.data(), bg.size());
            }
        }
    }

    if (s_objects[COLOR_DEFAULT].attr == LAST_MARKING)
    {
        s_objects[COLOR_DEFAULT].attr = NO_MARKING;
    }

    // Set color to default.
    color_default();
}

// Parse a line from the [attribute] section of trnrc.
void color_rc_attribute(std::string_view object, char *value)
{
    // Find the specified object.
    const std::string object_name{object};
    int i;
    for (i = 0; i < MAX_COLORS; i++)
    {
        if (string_case_equal(object, s_objects[i].name))
        {
            break;
        }
    }
    if (i >= MAX_COLORS)
    {
        std::fprintf(stderr,"trn: unknown object '%s' in [attribute] section.\n",
                object_name.c_str());
        finalize(1);
    }

    // Parse the video attribute.
    if (*value == 's' || *value == 'S')
    {
        s_objects[i].attr = STANDOUT;
    }
    else if (*value == 'u' || *value == 'U')
    {
        s_objects[i].attr = UNDERLINE;
    }
    else if (*value == 'n' || *value == 'N')
    {
        s_objects[i].attr = NO_MARKING;
    }
    else if (*value == '-')
    {
        s_objects[i].attr = LAST_MARKING;
    }
    else
    {
        std::fprintf(stderr,"trn: bad attribute '%s' for %s in [attribute] section.\n",
                value, object_name.c_str());
        finalize(1);
    }

    // See if they specified a color
    char *s = skip_non_space(value);
    s = skip_space(s);
    if (!*s)
    {
        s_objects[i].fg.clear();
        s_objects[i].bg.clear();
        return;
    }
    char *t = skip_non_space(s);
    char* n = nullptr;
    if (*t)
    {
        n = t++;
        *n = '\0';
        t = skip_space(t);
    }

    // We have both colors and attributes, so turn colors on.
    s_use_colors = true;

    // Parse the foreground color.
    if (*s == '-')
    {
        s_objects[i].fg.clear();
    }
    else
    {
        const std::string capability = fmt::format("fg {}", s);
        s_objects[i].fg = tc_color_capability(capability.c_str());
        if (s_objects[i].fg.empty())
        {
            std::fprintf(stderr,"trn: no color '%s' for %s in [attribute] section.\n",
                    capability.c_str(), object_name.c_str());
            finalize(1);
        }
    }
    if (n)
    {
        *n = ' ';
        n = nullptr;
    }

    // Make sure we have one more parameter.
    s = t;
    t = skip_non_space(t);
    if (*t)
    {
        n = t++;
        *n = '\0';
        t = skip_space(t);
    }
    if (!*s || *t)
    {
        std::fprintf(stderr,"trn: wrong number of parameters for %s in [attribute] section.\n",
                object_name.c_str());
        finalize(1);
    }

    // Parse the background color.
    if (*s == '-')
    {
        s_objects[i].bg = nullptr;
    }
    else
    {
        const std::string capability = fmt::format("bg {}", s);
        s_objects[i].bg = tc_color_capability(capability.c_str());
        if (s_objects[i].bg.empty())
        {
            std::fprintf(stderr,"trn: no color '%s' for %s in [attribute] section.\n",
                    capability.c_str(), object_name.c_str());
            finalize(1);
        }
    }
    if (n)
    {
        *n = ' ';
    }
}

// Turn on color attribute for an object.
void color_object(int object, bool push)
{
    // Merge in the colors/attributes that we are not setting
    // from the current object.
    ColorObj merged = s_color_stack[s_stack_pointer];

    // Merge in the new colors/attributes.
    if (!s_objects[object].fg.empty())
    {
        merged.fg = s_objects[object].fg;
    }
    if (!s_objects[object].bg.empty())
    {
        merged.bg = s_objects[object].bg;
    }
    if (s_objects[object].attr != LAST_MARKING)
    {
        merged.attr = s_objects[object].attr;
    }

    // Push onto stack.
    if (push && ++s_stack_pointer >= STACK_SIZE)
    {
        // TODO: error reporting?
        s_stack_pointer = 0;            // empty stack
        color_default();                // and set normal colors
        return;
    }
    s_color_stack[s_stack_pointer] = merged;

    // Set colors/attributes.
    output_color();
}

// Pop the color/attribute stack.
void color_pop()
{
    // Trying to pop an empty stack?
    if (--s_stack_pointer < 0)
    {
        s_stack_pointer = 0;
    }
    else
    {
        output_color();
    }
}

// Color a string with the given object's color/attribute.
void color_string(int object, std::string_view str)
{
    const bool had_newline = !str.empty() && str.back() == '\n';
    if (had_newline)
    {
        str.remove_suffix(1);
    }
    if (!s_use_colors && *g_tc_UC && s_objects[object].attr == UNDERLINE)
    {
        const std::string text{str};
        under_print(text.c_str()); // hack for stupid terminals
    }
    else
    {
        color_object(object, true);
        if (!str.empty())
        {
            std::fwrite(str.data(), 1, str.size(), stdout);
        }
        color_pop();
    }
    if (had_newline)
    {
        std::putchar('\n');
    }
}

// Turn off color attribute.
void color_default()
{
    s_color_stack[s_stack_pointer] = s_objects[COLOR_DEFAULT];
    output_color();
}

// Set colors/attribute for an object.
static void output_color()
{
    static ColorObj prior{"", {}, {}, NO_MARKING};
    ColorObj* op = &s_color_stack[s_stack_pointer];

    // If no change, just return.
    if (op->attr == prior.attr && op->fg == prior.fg && op->bg == prior.bg)
    {
        return;
    }

    // Start by turning off any existing colors and/or attributes.
    if (s_use_colors)
    {
        if (s_objects[COLOR_DEFAULT].fg != prior.fg //
            || s_objects[COLOR_DEFAULT].bg != prior.bg)
        {
            prior.fg = s_objects[COLOR_DEFAULT].fg;
            prior.bg = s_objects[COLOR_DEFAULT].bg;
            std::fputs(prior.fg.c_str(), stdout);
            std::fputs(prior.bg.c_str(), stdout);
        }
    }
    switch (prior.attr)
    {
    case NO_MARKING:
        break;

    case STANDOUT:
        un_standout();
        break;

    case UNDERLINE:
        un_underline();
        break;
    }

    // For color terminals we set the foreground and background color.
    if (s_use_colors)
    {
        if (op->fg != prior.fg)
        {
            prior.fg = op->fg;
            std::fputs(prior.fg.c_str(), stdout);
        }
        if (op->bg != prior.bg)
        {
            prior.bg = op->bg;
            std::fputs(prior.bg.c_str(), stdout);
        }
    }

    // For both monochrome and color terminals we set the video attribute.
    switch (prior.attr = op->attr)
    {
    case NO_MARKING:
        break;

    case STANDOUT:
#ifdef NO_FIREWORKS
        no_so_fire();
#endif
        standout();
        break;

    case UNDERLINE:
#ifdef NO_FIREWORKS
        no_ul_fire();
#endif
        underline();
        break;
    }
}
